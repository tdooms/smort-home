//============================================================================
// @name        : lamp.h
// @author      : Thomas Dooms
// @date        : 1/17/20
// @version     :
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description :
//============================================================================

#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <variant>
#include <vector>

#include <boost/asio.hpp>
#include <queue>
#include <set>
#include <utility/color.h>

#include "util.h"


namespace yeelight
{

enum class State
{
    connected,
    disconnected,
};

enum class Parameter
{
    connected,
    powered,
    brightness,
    color,
};

enum class Error
{
    none,
    not_connected
};

using Value = uint32_t;

class Device
{
    public:
    Device(const std::shared_ptr<boost::asio::io_context>& context,
           boost::asio::ip::tcp::endpoint                  endpoint);

    Device(const Device&) = delete;

    Device operator=(const Device&) = delete;

    ////////////////////////////////////////////////////

    void set_update_callback(std::function<void(Parameter, Value)> callback);

    void set_error_callback(std::function<void(Error)> callback);

    ////////////////////////////////////////////////////

    void toggle();

    void set_color_temperature(size_t temp, std::chrono::milliseconds duration = default_duration);

    void set_rgb_color(dot::color color, std::chrono::milliseconds duration = default_duration);

    void set_brightness(size_t brightness, std::chrono::milliseconds duration = default_duration);

    void set_powered(bool on, std::chrono::milliseconds duration = default_duration);

    void start_color_flow(flow_stop_action action, const std::vector<flow_state>& states);

    void stop_color_flow();

    void set_shutdown_timer(std::chrono::minutes time);

    void remove_shutdown_timer();

    void set_name(std::string name);

    private:
    // this function assures the operation is sent, even across tcp connections
    template <typename... Args>
    void send_operation(std::string method, Args... args);

    void start_listening();

    void start_tcp_listening();

    void start_icmp_listening();

    void try_connecting();

    void reset_ping_timer();

    void send_ping();

    bool handle_tcp_error(boost::system::error_code error, std::string info);

    bool handle_icmp_error(boost::system::error_code error, std::string info);

    bool handle_wait_error(boost::system::error_code error, std::string info);

    // io stuff
    std::shared_ptr<boost::asio::io_context> context;

    boost::asio::ip::tcp::endpoint tcp_endpoint;
    boost::asio::ip::tcp::socket   tcp_socket;

    boost::asio::ip::icmp::endpoint icmp_endpoint;
    boost::asio::ip::icmp::socket   icmp_socket;

    // various
    std::string buffer;
    State       state;

    // various
    std::set<uint64_t> pending_requests;
    uint64_t           message_id;
    uint64_t           ping_id;

    // callback functions
    std::function<void(Parameter, Value)> update_callback;
    std::function<void(Error)>            error_callback;

    // variables for disconnect checking
    const uint32_t ping_timeout      = 3000;
    const uint32_t operation_timeout = 1000;

    boost::asio::deadline_timer operation_timer;
    boost::asio::deadline_timer ping_timer;

    // static variables
    constexpr static auto default_duration = std::chrono::milliseconds(300);
    //    inline const static std::map<std::string, Parameter> parameter_map
    //    = { { "powered", powered }, { "power", on }, { "bright", brightness } };
};

} // namespace yeelight
