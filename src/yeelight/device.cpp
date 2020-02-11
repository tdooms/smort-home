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

#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <nlohmann/json.h>

namespace
{
    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
}

namespace yeelight
{

using namespace std::chrono_literals;

    device::device(std::shared_ptr<boost::asio::io_context> context, const std::string& ip_address, size_t port,
            std::string model, std::string firmware_version, std::vector<std::string> support,
            bool is_on, size_t brightness, device_color color, std::string name, size_t id)
            : context(std::move(context)), endpoint(boost::asio::ip::address::from_string(ip_address), port),
            send_socket(*this->context), model(std::move(model)), firmware_version(std::move(firmware_version)),
            support(std::move(support)), is_on(is_on), brightness(brightness), color(color), name(std::move(name)), id(id) {}

uint64_t device::get_id()
{
    return id;
}

const std::string& device::get_name()
{
    return name;
}

bool device::toggle()
{
    return send_operation("toggle");
}

bool device::set_color_temperature(size_t new_temperature, std::chrono::milliseconds duration)
{
    return send_operation("set_ct_abx", new_temperature, smooth_or_sudden(duration), duration.count());
}

bool device::set_rgb_color(dot::color new_color, std::chrono::milliseconds duration)
{
    return send_operation("set_rgb", dot::color::to_rgb(new_color), smooth_or_sudden(duration), duration.count());
}

bool device::set_brightness(size_t new_brightness, std::chrono::milliseconds duration)
{
    return send_operation("set_bright", new_brightness, smooth_or_sudden(duration), duration.count());
}

bool device::set_power(bool on, std::chrono::milliseconds duration)
{
    is_on = on;
    return send_operation("set_power", on ? "on" : "off", smooth_or_sudden(duration), duration.count());
}

bool device::start_color_flow(flow_stop_action action, const std::vector<flow_state>& states)
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

bool device::stop_color_flow()
{
    return send_operation("stop_cf");
}

bool device::set_shutdown_timer(std::chrono::minutes time)
{
    return send_operation("cron_add", 0, time.count());
}

bool device::remove_shutdown_timer()
{
    return send_operation("cron_del", 0);
}

bool device::set_name(std::string new_name)
{
    const auto res = send_operation("set_name", new_name);
    name = std::move(new_name);
    return res;
}

bool device::pulse_color(dot::color new_color, std::chrono::milliseconds duration)
{
//    const auto func = [&](){};
    bool result = true;
    result &= set_rgb_color(new_color, duration);
    std::this_thread::sleep_for(2 * duration);
    result &= return_to_old_state();
    return result;
}

bool device::return_to_old_state()
{
    return std::visit(overloaded
    {
        [&](temperature_color c){ return set_color_temperature(c.temperature); },
        [&](rgb_color c){ return set_rgb_color(c); }
    }, color);
}

bool device::toggle_twice(std::chrono::milliseconds duration)
{
    bool result = true;
    result &= toggle();
    std::this_thread::sleep_for(duration);
    result &= toggle();
    return result;
}

template<typename... Args>
bool device::send_operation(std::string method, Args... args)
{
    if(std::find(support.begin(), support.end(), method) == support.end())
    {
        std::cerr << "lamp with name: " << name << " does not support method " << method;
        return false;
    }

    if(not send_socket.is_open())
    {
        boost::system::error_code error;

        send_socket.open(endpoint.protocol(), error);
        send_socket.connect(endpoint, error);
        if (error) return false;
    }

    auto json = nlohmann::json();
    json["id"] = 1;
    json["method"] = std::move(method);
    (json["params"].emplace_back(std::move(args)), ...);

    send_socket.async_send(
            boost::asio::buffer(json.dump() + "\r\n"),
            boost::bind(&device::handle_send, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));

    return true;
}

void device::handle_send(const boost::system::error_code& error, size_t bytes_transferred)
{
    if (error) return;
    std::cout << bytes_transferred << " bytes sent succesfully\n";
}

// --------------------------------------------------------------------- //

std::shared_ptr<device> device::parse_from_string(std::string_view string, std::shared_ptr<boost::asio::io_context> context)
{
    const static std::string http_ok = "HTTP/1.1 200 OK\r\n";
    const static std::string notify = "NOTIFY * HTTP/1.1\r\n";

    auto iter = string.begin();
    if(std::equal(http_ok.begin(), http_ok.end(), string.begin()))
    {
        iter += http_ok.size();
    }
    else if(std::equal(notify.begin(), notify.end(), string.begin()))
    {
        iter += notify.size();
    }
    else return nullptr;

    std::map<std::string, std::string> data;

    while(true)
    {
        const auto key_end = std::find(iter, string.end(), ':');
        const auto value_start = key_end + 2;
        if(value_start >= string.end()) return {};

        const auto value_end = std::find(value_start, string.end(), '\r');
        data.emplace(std::string(iter, key_end), std::string(value_start, value_end));

        if(value_end + 2 >= string.end()) break;
        iter = value_end + 2;
    }

    const auto index = data["Location"].find_first_of(':', 11);
    if(index == std::string::npos) return {};

    const auto ip_address = data["Location"].substr(11, index - 11);
    const auto port = std::stoul(data["Location"].substr(index + 1));

    device_color color;
    const auto mode = color_mode(std::stoul(data["color_mode"]));
    if (mode == color_mode::rgb) color = rgb_color::from_rgb(std::stol(data["rgb"]));
    else if (mode == color_mode::temperature) color = temperature_color{std::stoul(data["ct"])};
    else return {};

    std::vector<std::string> support;
    boost::split(support, data["support"], ::isspace);

    return std::make_shared<device>(
            context, ip_address, port, data["model"], data["fw_ver"], support, data["power"] == "on",
            std::stoul(data["bright"]), color, data["name"],
            std::stoul(data["id"], nullptr, 16));
}

std::optional<uint64_t> device::get_id_from_string(std::string_view string)
{
    const auto index = string.find("id: ");

    if(index != std::string::npos)
    {
        const auto iter = string.begin() + index;
        const auto substr = std::string(iter + 4, iter + 22);
        return std::stoull(substr, nullptr, 16);
    }
    else return {};
}

std::string device::smooth_or_sudden(std::chrono::milliseconds duration)
{
    return duration > std::chrono::milliseconds(30) ? "smooth" : "sudden";
}


}