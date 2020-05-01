//============================================================================
// @name        : nettest.h
// @author      : Thomas Dooms
// @date        : 1/30/20
// @version     :
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description :
//============================================================================


#pragma once

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <filesystem>
#include <string>
#include <map>

#include "device.h"

namespace yeelight
{

using namespace std::chrono_literals;


class Scanner
{
    public:
    explicit Scanner(std::function<void(std::unique_ptr<Device>)> handler);
    ~Scanner();

    private:
    void async_receive();

    void async_broadcast();

    void async_listen();

    void handle_response(std::string_view response);

    void read_from_file();

    void write_to_file();

    void handle_new_device(uint64_t id, std::string ip);

    std::shared_ptr<boost::asio::io_context> context;
    boost::asio::io_service::work work;
    std::thread thread;

    boost::asio::ip::udp::socket listen_socket;
    boost::asio::ip::udp::socket scan_socket;

    boost::asio::ip::udp::endpoint sender_endpoint;
    boost::asio::ip::udp::endpoint multicast_endpoint;

    boost::asio::deadline_timer timer;

    std::string message;
    std::string buffer;

    std::filesystem::path path = "devices.json";

    std::map<uint64_t, std::string> devices;
    std::function<void(std::unique_ptr<Device>)> handler;

    constexpr static auto buffer_size = 1024;
    constexpr static auto multicast_port = 1982;
    constexpr static auto tcp_port = 55443;
    constexpr static auto multicast_ip = "239.255.255.250";
};

} // namespace yeelight