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
#include <regex>
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
    device(std::shared_ptr<boost::asio::io_context> context,
            boost::asio::ip::tcp::endpoint endpoint,
            size_t id, std::string name);

    device(const device& device);

    uint64_t get_id() const;

    std::string get_name() const;

    std::string get_ip_address() const;

    uint64_t get_port() const;

    bool toggle() const;

    bool set_color_temperature(size_t new_temperature, std::chrono::milliseconds duration = default_duration) const;

    bool set_rgb_color(dot::color new_color, std::chrono::milliseconds duration = default_duration) const;

    bool set_brightness(size_t new_brightness, std::chrono::milliseconds duration = default_duration) const;

    bool set_power(bool on, std::chrono::milliseconds duration = default_duration) const;

    bool start_color_flow(flow_stop_action action, const std::vector<flow_state>& states) const;

    bool stop_color_flow() const;

    bool set_shutdown_timer(std::chrono::minutes time) const;

    bool remove_shutdown_timer() const;

    bool toggle_twice(std::chrono::milliseconds duration = default_duration) const;

    bool set_name(std::string new_name);


private:
    template<typename... Args>
    bool send_operation(std::string method, Args... args) const;

    static void handle_send(const boost::system::error_code& error, size_t bytes_transferred);

    static std::string smooth_or_sudden(std::chrono::milliseconds duration);

    std::shared_ptr<boost::asio::io_context> context;
    boost::asio::ip::tcp::endpoint endpoint;
    mutable boost::asio::ip::tcp::socket send_socket;

    std::string name;
    size_t id;

    constexpr static auto default_duration = std::chrono::milliseconds(300);
};

}

