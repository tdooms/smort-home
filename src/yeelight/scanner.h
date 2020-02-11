//============================================================================
// @name        : nettest.h
// @author      : Thomas Dooms
// @date        : 1/30/20
// @version     : 
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description : 
//============================================================================


#pragma once

#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "device.h"

namespace yeelight
{


using namespace std::chrono_literals;


class scanner
{
public:
    explicit scanner();
    ~scanner();

    std::shared_ptr<device> get_device(uint64_t id);
    std::shared_ptr<device> get_device(const std::string& name);

private:
    void async_receive();

    void async_broadcast();

    void async_listen();

    void handle_receive(const boost::system::error_code& error, size_t bytes_received);

    void handle_receive_from(const boost::system::error_code& error, size_t bytes_received);

    void handle_send_to(const boost::system::error_code& error);

    void handle_timeout(const boost::system::error_code& error);

    void handle_response(std::string_view response);


    std::shared_ptr<boost::asio::io_context> context;

    boost::asio::ip::udp::socket listen_socket;
    boost::asio::ip::udp::socket scan_socket;

    boost::asio::ip::udp::endpoint sender_endpoint;
    boost::asio::ip::udp::endpoint endpoint;

    boost::asio::deadline_timer timer;

    std::string message;
    std::string buffer;
    size_t message_count;

    std::map<uint64_t, std::shared_ptr<device>> devices;
    std::map<std::string, uint64_t> names;

    std::thread thread;
    std::mutex mutex;

    constexpr static auto buffer_size = 1024;
    constexpr static auto max_message_count = 10;

    constexpr static auto multicast_port = 1982;
    constexpr static auto multicast_ip = "239.255.255.250";
};

}