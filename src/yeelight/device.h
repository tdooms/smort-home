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

enum State
{
    stateless,
    connected,
    disconnected,
};

class Device
{
    public:
    Device(const std::shared_ptr<boost::asio::io_context>& context, boost::asio::ip::tcp::endpoint endpoint);

    Device(const Device&) = delete;

    Device operator=(const Device&) = delete;

    ////////////////////////////////////////////////////

    void when_connected(std::function<void()> operation, bool remove = false);

    void when_disconnected(std::function<void()> operation, bool remove = false);

    void when_updated(std::function<void()> operation, bool remove = false);

    ////////////////////////////////////////////////////

    std::pair<bool, bool> get_powered();

    std::pair<int32_t, bool> get_brightness();

    std::pair<std::string, bool> get_name();

    ////////////////////////////////////////////////////

    void toggle();

    void set_color_temperature(size_t new_temperature, std::chrono::milliseconds duration = default_duration);

    void set_rgb_color(dot::color new_color, std::chrono::milliseconds duration = default_duration);

    void set_brightness(size_t new_brightness, std::chrono::milliseconds duration = default_duration);

    void set_powered(bool on, std::chrono::milliseconds duration = default_duration);

    void start_color_flow(flow_stop_action action, const std::vector<flow_state>& states);

    void stop_color_flow();

    void set_shutdown_timer(std::chrono::minutes time);

    void remove_shutdown_timer();

    void set_name(std::string new_name);

    private:

    using CallbackList = std::vector<std::pair<std::function<void()>, bool>>;


    template <typename... Args>
    uint64_t send_operation(std::string method, Args... args);

    void update_params();

    void start_listening();

    void try_connecting();

    void connect_handler();

    void disconnect_handler();

    void execute_callbacks(CallbackList& callbacks);

    // io stuff
    std::shared_ptr<boost::asio::io_context>        context;
    boost::asio::ip::tcp::endpoint                  endpoint;
    boost::asio::ip::tcp::socket                    send_socket;
    std::string                                     buffer;
    State                                           state;
    uint64_t                                        message_id;
    std::set<uint64_t>                              param_request;
    std::map<uint64_t, boost::asio::deadline_timer> timeouts;

    // contained data, bool indicates if it has been updates since query
    std::pair<bool, bool>        powered;
    std::pair<int32_t, bool>     brightness;
    std::pair<std::string, bool> name;

    // handler functions
    CallbackList on_connection;
    CallbackList on_disconnection;
    CallbackList on_update;

    // static variables
    constexpr static auto default_duration = std::chrono::milliseconds(300);
};

} // namespace yeelight
