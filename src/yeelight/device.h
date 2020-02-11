//============================================================================
// @name        : lamp.h
// @author      : Thomas Dooms
// @date        : 1/17/20
// @version     : 
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description : 
//============================================================================


#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <map>
#include <memory>
#include <variant>

#include <utility/color.h>
#include <boost/asio.hpp>

#include "util.h"

namespace yeelight
{

class device
{
public:
    device(std::shared_ptr<boost::asio::io_context> context, const std::string& ip_address, size_t port,
            std::string model, std::string firmware_version, std::vector<std::string> support,
            bool is_on, size_t brightness, device_color color, std::string name, size_t id);

    device(const device&) = delete;
    device& operator=(const device&) = delete;

    [[nodiscard]] uint64_t get_id();

    [[nodiscard]] const std::string& get_name();

     bool toggle();

     bool set_color_temperature(size_t new_temperature, std::chrono::milliseconds duration = default_duration);

     bool set_rgb_color(dot::color new_color, std::chrono::milliseconds duration = default_duration);

     bool set_brightness(size_t new_brightness, std::chrono::milliseconds duration = default_duration);

     bool set_power(bool on, std::chrono::milliseconds duration = default_duration);

     bool start_color_flow(flow_stop_action action, const std::vector<flow_state>& states);

     bool stop_color_flow();

     bool set_shutdown_timer(std::chrono::minutes time);

     bool remove_shutdown_timer();

     bool set_name(std::string new_name);

     bool pulse_color(dot::color new_color, std::chrono::milliseconds duration = default_duration);

     bool toggle_twice(std::chrono::milliseconds duration = default_duration);

    static std::shared_ptr<device> parse_from_string(std::string_view string, std::shared_ptr<boost::asio::io_context> context);
    static std::optional<uint64_t> get_id_from_string(std::string_view string);

private:
    template<typename... Args>
    bool send_operation(std::string method, Args... args);

    void handle_send(const boost::system::error_code& error, size_t bytes_transferred);

    std::string smooth_or_sudden(std::chrono::milliseconds duration);

    bool return_to_old_state();

    std::shared_ptr<boost::asio::io_context> context;
    boost::asio::ip::tcp::endpoint endpoint;
    boost::asio::ip::tcp::socket send_socket;

    std::string model;
    std::string firmware_version;
    std::vector<std::string> support;

    bool is_on;
    size_t brightness;
    device_color color;

    std::string name;
    size_t id;

    constexpr static auto default_duration = std::chrono::milliseconds(300);
};

}

