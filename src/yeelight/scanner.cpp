//============================================================================
// @name        : deviceManager.cpp
// @author      : Thomas Dooms
// @date        : 1/30/20
// @version     : 
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description : 
//============================================================================

#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>

#include <nlohmann/json.h>
#include <filesystem>
#include <fstream>

#include "scanner.h"

namespace yeelight
{


scanner::scanner()
: context(std::make_shared<boost::asio::io_context>()), listen_socket(*context),
scan_socket(*context), timer(*context), message(), buffer(buffer_size, '\0'), message_count(0)
{
    const auto listen_address = boost::asio::ip::address();
    const auto multicast_address = boost::asio::ip::address::from_string(multicast_ip);

    boost::asio::ip::udp::endpoint listen_endpoint(listen_address, multicast_port);
    endpoint = boost::asio::ip::udp::endpoint(multicast_address, multicast_port);

    listen_socket.open(listen_endpoint.protocol());
    listen_socket.bind(listen_endpoint);
    listen_socket.set_option(boost::asio::ip::multicast::join_group(multicast_address));

    scan_socket.open(endpoint.protocol());

    message += "M-SEARCH * HTTP/1.1\r\n";
    message += "HOST: 239.255.255.250:1982\r\n";
    message += "MAN: \"ssdp:discover\"\r\n";
    message += "ST: wifi_bulb";

    async_broadcast();
    async_receive();
    async_listen();

    thread = std::thread([&](){ context->run(); });
}

scanner::~scanner()
{
    context->stop();
    thread.join();
}

std::shared_ptr<device> scanner::get_device(uint64_t id)
{
    const auto iter = devices.find(id);
    if(iter == devices.end()) return nullptr;
    else return iter->second;
}

std::shared_ptr<device> scanner::get_device(const std::string& name)
{
    const auto comp = [&](const auto& device){ return device.second->get_name() == name; };
    const auto iter = std::find_if(devices.begin(), devices.end(), comp);
    if(iter == devices.end()) return nullptr;
    else return iter->second;
}

void scanner::async_receive()
{
    scan_socket.async_receive(
            boost::asio::buffer(buffer, buffer_size),
            boost::bind(&scanner::handle_receive, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
}

void scanner::async_broadcast()
{
    scan_socket.async_send_to(
            boost::asio::buffer(message), endpoint,
            boost::bind(&scanner::handle_send_to, this,
                    boost::asio::placeholders::error));
}

void scanner::async_listen()
{
    listen_socket.async_receive_from(
            boost::asio::buffer(buffer, buffer_size), sender_endpoint,
            boost::bind(&scanner::handle_receive_from, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
}

void scanner::handle_receive(const boost::system::error_code& error, const size_t bytes_received)
{
    if (error) return;
    handle_response(std::string_view(buffer.data(), bytes_received));
    async_receive();
}

void scanner::handle_receive_from(const boost::system::error_code& error, const size_t bytes_received)
{
    if (error) return;
    handle_response(std::string_view(buffer.data(), bytes_received));
    async_listen();
}

void scanner::handle_send_to(const boost::system::error_code& error)
{
    if (not error and message_count < max_message_count)
    {
        timer.expires_from_now(boost::posix_time::seconds(1));
        timer.async_wait(
                boost::bind(&scanner::handle_timeout, this,
                            boost::asio::placeholders::error));
    }
}

void scanner::handle_timeout(const boost::system::error_code& error)
{
    if (error) return;
    async_broadcast();
}

void scanner::handle_response(std::string_view response)
{
    const auto id = device::get_id_from_string(response);
    if(not id.has_value()) return;

    if(devices.find(*id) != devices.end()) return;

    auto device = device::parse_from_string(response, context);
    if(device == nullptr) return;

    devices.emplace(*id, std::move(device));
}


}