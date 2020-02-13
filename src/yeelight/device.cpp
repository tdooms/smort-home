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

#include <boost/bind.hpp>
#include <nlohmann/json.h>

namespace yeelight
{

using namespace std::chrono_literals;

device::device(std::shared_ptr<boost::asio::io_context> context,
        boost::asio::ip::tcp::endpoint endpoint,
        size_t id, std::string name)
        : context(std::move(context)), endpoint(std::move(endpoint)),
        send_socket(*this->context), name(std::move(name)), id(id) {}

device::device(const device& device)
: context(device.context), endpoint(device.endpoint),
send_socket(*device.context), name(device.name), id(device.id) {}

uint64_t device::get_id() const
{
    return id;
}

std::string device::get_name() const
{
    return name;
}

std::string device::get_ip_address() const
{
    return endpoint.address().to_string();
}

uint64_t device::get_port() const
{
    return endpoint.port();
}

bool device::toggle() const
{
    return send_operation("toggle");
}

bool device::set_color_temperature(size_t new_temperature, std::chrono::milliseconds duration) const
{
    return send_operation("set_ct_abx", new_temperature, smooth_or_sudden(duration), duration.count());
}

bool device::set_rgb_color(dot::color new_color, std::chrono::milliseconds duration) const
{
    return send_operation("set_rgb", dot::color::to_rgb(new_color), smooth_or_sudden(duration), duration.count());
}

bool device::set_brightness(size_t new_brightness, std::chrono::milliseconds duration) const
{
    return send_operation("set_bright", new_brightness, smooth_or_sudden(duration), duration.count());
}

bool device::set_power(bool on, std::chrono::milliseconds duration) const
{
    return send_operation("set_power", on ? "on" : "off", smooth_or_sudden(duration), duration.count());
}

bool device::start_color_flow(flow_stop_action action, const std::vector<flow_state>& states) const
{
    std::stringstream stream;
    for(const auto& state : states)
    {
        stream << state.duration.count() << ',';
        stream << static_cast<size_t>(state.mode) << ',';

        if (state.mode == color_mode::temperature) stream << state.temperature << ',';
        else if (state.mode == color_mode::rgb) stream << dot::color::to_rgb(state.color) << ',';
        else if (state.mode == color_mode::sleep) stream << 0 << ',';
        else throw std::logic_error("cannot put color flow in hsv mode");

        stream << state.brightness;
    }

    return send_operation("start_cf", states.size(), static_cast<size_t>(action), stream.str());
}

bool device::stop_color_flow() const
{
    return send_operation("stop_cf");
}

bool device::set_shutdown_timer(std::chrono::minutes time) const
{
    return send_operation("cron_add", 0, time.count());
}

bool device::remove_shutdown_timer() const
{
    return send_operation("cron_del", 0);
}


bool device::toggle_twice(std::chrono::milliseconds duration) const
{
    bool result = true;
    result &= toggle();
    std::this_thread::sleep_for(duration);
    result &= toggle();
    return result;
}

bool device::set_name(std::string new_name)
{
    const auto res = send_operation("set_name", new_name);
    name = std::move(new_name);
    return res;
}

template<typename... Args>
bool device::send_operation(std::string method, Args... args) const
{
    if(not send_socket.is_open())
    {
        boost::system::error_code error;
        boost::asio::socket_base::keep_alive option(true);

        send_socket.open(endpoint.protocol(), error);
        send_socket.set_option(option);
        send_socket.connect(endpoint, error);
        if (error) return false;
    }

    auto json = nlohmann::json();
    json["id"] = 1;
    json["method"] = std::move(method);
    (json["params"].emplace_back(std::move(args)), ...);

    send_socket.async_send(
            boost::asio::buffer(json.dump() + "\r\n"),
            boost::bind(&device::handle_send,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));

    return true;
}

void device::handle_send(const boost::system::error_code& error, size_t bytes_transferred)
{
    if (error) return;
    std::cout << bytes_transferred << " bytes sent succesfully\n";
}

std::string device::smooth_or_sudden(std::chrono::milliseconds duration)
{
    return duration > std::chrono::milliseconds(30) ? "smooth" : "sudden";
}


}