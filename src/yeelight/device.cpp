//============================================================================
// @name        : lamp.cpp
// @author      : Thomas Dooms
// @date        : 1/17/20
// @version     :
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description :
//============================================================================

#include "device.h"

#include <algorithm>
#include <iostream>
#include <thread>

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
: context(context), endpoint(std::move(endpoint)), send_socket(*context), buffer(1024, '\0'),
  state(State::stateless), message_id(1)
{
    // try to establish a connection
    try_connecting();
}

void Device::when_connected(std::function<void()> operation, bool remove)
{
    if(state != State::connected)
    {
        on_connection.emplace_back(std::move(operation), remove);
    }
    else
    {
        operation();
    }
}

void Device::when_disconnected(std::function<void()> operation, bool remove)
{
    if(state != State::disconnected)
    {
        on_disconnection.emplace_back(std::move(operation), remove);
    }
    else
    {
        operation();
    }
}

void Device::when_updated(std::function<void()> operation, bool remove)
{
    on_update.emplace_back(std::move(operation), remove);
}

std::pair<bool, bool> Device::get_powered()
{
    const auto temp = powered;
    powered.second  = false;
    return temp;
}

std::pair<int32_t, bool> Device::get_brightness()
{
    const auto temp = brightness;
    powered.second  = false;
    return temp;
}

std::pair<std::string, bool> Device::get_name()
{
    const auto temp = name;
    powered.second  = false;
    return temp;
}


void Device::toggle()
{
    send_operation("toggle");
}

void Device::set_color_temperature(size_t new_temperature, std::chrono::milliseconds duration)
{
    send_operation("set_ct_abx", new_temperature, string_powered(duration), duration.count());
}

void Device::set_rgb_color(dot::color new_color, std::chrono::milliseconds duration)
{
    send_operation("set_rgb", dot::color::to_rgb(new_color), string_powered(duration), duration.count());
}

void Device::set_brightness(size_t new_brightness, std::chrono::milliseconds duration)
{
    send_operation("set_bright", new_brightness, string_powered(duration), duration.count());
}

void Device::set_powered(bool on, std::chrono::milliseconds duration)
{
    send_operation("set_power", on ? "on" : "off", string_powered(duration), duration.count());
}

void Device::start_color_flow(flow_stop_action action, const std::vector<flow_state>& states)
{
    std::stringstream stream;
    for(const auto& state : states)
    {
        stream << state.duration.count() << ',';
        stream << static_cast<size_t>(state.mode) << ',';

        if(state.mode == color_mode::temperature) stream << state.temperature << ',';
        else if(state.mode == color_mode::rgb)
            stream << dot::color::to_rgb(state.color) << ',';
        else if(state.mode == color_mode::sleep)
            stream << 0 << ',';
        else
            throw std::logic_error("cannot put color flow in hsv mode");

        stream << state.brightness;
    }

    send_operation("start_cf", states.size(), static_cast<size_t>(action), stream.str());
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

void Device::set_name(std::string new_name)
{
    send_operation("set_name", new_name);
}

template <typename... Args>
uint64_t Device::send_operation(std::string method, Args... args)
{
    const auto id            = message_id;
    const auto internal_send = [&, id]() {
        auto json      = nlohmann::json();
        json["id"]     = id;
        json["method"] = std::move(method);
        (json["params"].emplace_back(std::move(args)), ...);

        const auto handler = [&, id](const auto& error, [[maybe_unused]] auto bytes) {
            if(error) return;
            boost::asio::deadline_timer timer(*context, boost::posix_time::seconds(3));
            const auto                  after = [&]([[maybe_unused]] const auto& error) {
                if(error) return;
                if(state != State::disconnected) disconnect_handler();
            };
            timer.async_wait(after);
            timeouts.emplace(id, std::move(timer));
        };
        send_socket.async_send(boost::asio::buffer(json.dump() + "\r\n"), handler);
    };

    when_connected(internal_send);
    return message_id++;
}

void Device::update_params()
{
    const auto id = send_operation("get_prop", "power", "bright", "name");
    param_request.emplace(id);
}


void Device::start_listening()
{
    const auto recursive = [&](const auto& error, auto bytes) {
        if(error) return;

        const auto json = nlohmann::json::parse(buffer.substr(0, bytes));
        std::cout << json << '\n';

        if(json.contains("id"))
        {
            const uint64_t id = json["id"];
            timeouts.at(id).cancel();
            timeouts.erase(id);

            const auto check_assign = [](auto& var, const auto& elem) {
                if(elem != var.first) var = std::make_pair(elem, true);
            };

            if(param_request.find(id) != param_request.end())
            {
                check_assign(powered, json["result"][0] == "on");
                check_assign(brightness, std::stoi(std::string(json["result"][1])));
                check_assign(name, json["result"][2]);

                execute_callbacks(on_update);
            }
        }

        start_listening();
    };

    // start the receive loop, settings a new
    send_socket.async_receive(boost::asio::buffer(buffer), recursive);
}

void Device::try_connecting()
{
    const auto handler = [&](const auto& error) {
        if(not error or error == boost::asio::error::already_connected)
        {
            connect_handler();
        }
        else if(error == boost::asio::error::connection_aborted or error == boost::asio::error::host_unreachable
                or error == boost::asio::error::connection_refused)
        {
            try_connecting();
        }
        else
            std::cout << error << '\n';
    };

    boost::system::error_code            error;
    boost::asio::socket_base::keep_alive option(true);

    send_socket.open(endpoint.protocol(), error);
    send_socket.set_option(option);

    send_socket.async_connect(endpoint, handler);
}

void Device::connect_handler()
{
    state = State::connected;
    start_listening(); // start the receiving loop
    update_params();   // update internal values representing the state of the lamp

    execute_callbacks(on_connection);
}

void Device::disconnect_handler()
{
    state = State::disconnected;
    execute_callbacks(on_disconnection);

    // deconstruct the previous socket gracefully and try wit a new one
    send_socket = boost::asio::ip::tcp::socket(*context);

    try_connecting();
}

void Device::execute_callbacks(CallbackList& callbacks)
{
    for(const auto& [func, remove] : callbacks)
    {
        func();
    }

    const auto iter
    = std::remove_if(callbacks.begin(), callbacks.end(), [](const auto& elem) { return elem.second; });
    callbacks.erase(iter, callbacks.end());
}


} // namespace yeelight