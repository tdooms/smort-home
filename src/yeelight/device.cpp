//============================================================================
// @name        : lamp.cpp
// @author      : Thomas Dooms
// @date        : 1/17/20
// @version     :
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description :
//============================================================================

#include "device.h"

#include <boost-icmp/icmp_header.hpp>
#include <boost-icmp/ipv4_header.hpp>

#include <algorithm>
#include <iostream>
#include <thread>

#include <boost-icmp/icmp_header.hpp>
#include <nlohmann/json.h>

namespace
{
std::string string_powered(std::chrono::milliseconds duration)
{
    return duration > std::chrono::milliseconds(30) ? "smooth" : "sudden";
}

} // namespace

namespace yeelight
{

using namespace std::chrono_literals;

Device::Device(const std::shared_ptr<boost::asio::io_context>& context,
               boost::asio::ip::tcp::endpoint                  endpoint)
: context(context), tcp_endpoint(std::move(endpoint)), tcp_socket(*context),
  icmp_endpoint(boost::asio::ip::make_address("10.1.1.10"), 1),
  icmp_socket(*context, boost::asio::ip::icmp::v4()), buffer(1024, '\0'),
  state(State::disconnected), pending_requests(), message_id(1), ping_id(1),
  update_callback(nullptr), error_callback(nullptr), operation_timer(*context),
  ping_timer(*context)
{
    // TODO: something something capabilities
    // for some reason the ICMP socket has to connect...
    // icmp_socket.connect(icmp_endpoint);

    // try to establish a connection
    try_connecting();
}

void Device::set_update_callback(std::function<void(Parameter, Value)> callback)
{
    update_callback = std::move(callback);
}

void Device::set_error_callback(std::function<void(Error)> callback)
{
    error_callback = std::move(callback);
}


void Device::toggle()
{
    send_operation("toggle");
}

void Device::set_color_temperature(size_t temp, std::chrono::milliseconds duration)
{
    send_operation("set_ct_abx", temp, string_powered(duration), duration.count());
}

void Device::set_rgb_color(dot::color color, std::chrono::milliseconds duration)
{
    send_operation("set_rgb", dot::color::to_rgb(color),
                   string_powered(duration), duration.count());
}

void Device::set_brightness(size_t brightness, std::chrono::milliseconds duration)
{
    send_operation("set_bright", brightness, string_powered(duration), duration.count());
}

void Device::set_powered(bool on, std::chrono::milliseconds duration)
{
    send_operation("set_power", on ? "on" : "off", string_powered(duration),
                   duration.count());
}

void Device::start_color_flow(flow_stop_action action, const std::vector<flow_state>& states)
{
    std::stringstream stream;
    for(const auto& state : states)
    {
        stream << state.duration.count() << ',';
        stream << static_cast<size_t>(state.mode) << ',';

        if(state.mode == color_mode::temperature)
            stream << state.temperature << ',';
        else if(state.mode == color_mode::rgb)
            stream << dot::color::to_rgb(state.color) << ',';
        else if(state.mode == color_mode::sleep)
            stream << 0 << ',';
        else
            throw std::logic_error("cannot put color flow in hsv mode");

        stream << state.brightness;
    }

    send_operation("start_cf", states.size(), static_cast<size_t>(action),
                   stream.str());
}

void Device::stop_color_flow()
{
    send_operation("stop_cf");
}

void Device::set_shutdown_timer(std::chrono::minutes time)
{
    send_operation("cron_add", 0, time.count());
}

void Device::remove_shutdown_timer()
{
    send_operation("cron_del", 0);
}

void Device::set_name(std::string name)
{
    send_operation("set_name", std::move(name));
}

template <typename... Args>
void Device::send_operation(std::string method, Args... args)
{
    const auto current_id      = message_id;
    const auto timeout_handler = [this, current_id](auto error) {
        if(not handle_wait_error(error, "send timeout")) return;

        if(pending_requests.find(current_id) != pending_requests.end())
        {
            // this means no response was found after x seconds,
            // so we assume the lamp was disconnected of for some reason
            state = State::disconnected;
            update_callback(Parameter::connected, 0);
            try_connecting();
        }
    };

    const auto send_handler
    = [this](auto error, auto) { handle_tcp_error(error, "send request"); };

    // if connected send the request
    if(state == State::connected)
    {
        auto json      = nlohmann::json();
        json["id"]     = message_id++;
        json["method"] = std::move(method);
        (json["params"].emplace_back(std::move(args)), ...);

        tcp_socket.async_send(boost::asio::buffer(json.dump() + "\r\n"), send_handler);

        operation_timer.expires_from_now(boost::posix_time::milliseconds(operation_timeout));
        operation_timer.async_wait(timeout_handler);
    }
    else
    {
        error_callback(Error::not_connected);
    }
}


void Device::start_listening()
{
    start_tcp_listening();
    start_icmp_listening();
}

void Device::start_tcp_listening()
{
    const auto tcp_receive = [this](const auto& error, auto bytes) {
        // TODO; repair state
        if(not handle_tcp_error(error, "tcp receive")) return;

        const auto json = nlohmann::json::parse(buffer.substr(0, bytes));

        if(json.contains("id"))
        {
            const uint64_t id = json["id"];
            pending_requests.erase(id);
        }
        else
        {
            std::cout << json << '\n';
        }

        reset_ping_timer();
        start_listening();
    };

    // start the receive loop
    tcp_socket.async_receive(boost::asio::buffer(buffer), tcp_receive);
}

void Device::start_icmp_listening()
{
    const auto ping_receive = [this](auto error, auto) {
        // TODO: maybe also check want went wrong and such
        if(not handle_icmp_error(error, "icmp receive")) return;

        icmp_header icmp_header;
        ipv4_header ipv4_header;
        std::string body;

        // this is awful, this will print data from the previous
        // request because the buffer is initialized with zeroes
        // TODO: something something bytes received smart stuff
        std::istringstream(buffer) >> ipv4_header >> icmp_header >> body;
        std::cout << "received ping: " << body << '\n';
        if(icmp_header.identifier() == 93 and icmp_header.sequence_number() == ping_id)
        {
            ping_id++;
        }

        start_icmp_listening();
    };

    // start the receive loop
    icmp_socket.async_receive(boost::asio::buffer(buffer), ping_receive);
}

void Device::try_connecting()
{
    if(state == State::connected)
    {
        throw std::logic_error("already connected");
    }

    const auto handler = [this](auto error) {
        if(error == boost::asio::error::connection_aborted
           or error == boost::asio::error::host_unreachable)
        {
            state = State::disconnected;
            update_callback(Parameter::connected, 0);
            try_connecting();
        }
        else if(error == boost::asio::error::connection_refused)
        {
            std::cout << "TODO: handle connection refused\n";
            state = State::disconnected;
            update_callback(Parameter::connected, 0);
            try_connecting();
        }
        else if(handle_tcp_error(error, "connecting tcp"))
        {
            state = State::connected;
            update_callback(Parameter::connected, 1);
            reset_ping_timer();
        }
    };

    boost::system::error_code            error;
    boost::asio::socket_base::keep_alive option(true);

    tcp_socket.open(tcp_endpoint.protocol(), error);
    tcp_socket.set_option(option);

    tcp_socket.async_connect(tcp_endpoint, handler);
}

void Device::reset_ping_timer()
{
    const std::function<void(boost::system::error_code)> timeout_handler = [this](auto error) {
        if(not handle_wait_error(error, "ping timeout")) return;
        // this means disconnect usually

        send_ping();
        reset_ping_timer();
    };

    ping_timer.cancel();
    ping_timer.expires_from_now(boost::posix_time::milliseconds(ping_timeout));
    ping_timer.async_wait(timeout_handler);
}

void Device::send_ping()
{
    // TODO: do bigbrain stuff without having to reconstructing the string every time
    std::string body("Silky");

    // Create an ICMP header for an echo request.
    icmp_header echo_request;
    echo_request.type(icmp_header::echo_request);
    echo_request.code(0); // must be 0
    echo_request.identifier(93);
    echo_request.sequence_number(ping_id);
    compute_checksum(echo_request, body.begin(), body.end());

    // Encode the request packet.
    boost::asio::streambuf request_buffer;
    std::ostream           os(&request_buffer);
    os << echo_request << body;

    const auto send_handler = [this](auto error, auto) {
        handle_icmp_error(error, "send ping");
        std::cout << "sending ping\n";
    };

    // Send the request.
    icmp_socket.async_send_to(request_buffer.data(), icmp_endpoint, send_handler);
}

bool Device::handle_tcp_error(boost::system::error_code error, std::string info)
{
    if(error == boost::asio::error::connection_reset or error == boost::asio::error::eof)
    {
        std::cout << "tcp error: " << error << ". trying to reconnect\n";

        state = State::disconnected;
        update_callback(Parameter::connected, 0);

        try_connecting();
        return false;
    }
    else if(error)
    {
        throw std::runtime_error("tcp error: " + std::to_string(error.value())
                                 + " " + std::move(info));
    }
    else
    {
        return true;
    }
}

bool Device::handle_icmp_error(boost::system::error_code error, std::string info)
{
    if(error)
    {
        throw std::runtime_error("icmp error: " + std::to_string(error.value())
                                 + " " + std::move(info));
    }
    else
    {
        return true;
    }
}

bool Device::handle_wait_error(boost::system::error_code error, std::string info)
{
    if(error == boost::asio::error::operation_aborted)
    {
        return true;
    }
    else if(error)
    {
        throw std::runtime_error("wait error: " + std::to_string(error.value())
                                 + " " + std::move(info));
    }
    else
    {
        return true;
    }
}


} // namespace yeelight