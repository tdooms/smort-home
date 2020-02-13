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


scanner::scanner(std::shared_ptr<boost::asio::io_context> context)
                : context(std::move(context)), listen_socket(*this->context),
                scan_socket(*this->context), timer(*this->context),
                message(), buffer(buffer_size, '\0'), message_count(0)
{
    const auto listen_address = boost::asio::ip::address();
    const auto multicast_address = boost::asio::ip::address::from_string(multicast_ip);

    boost::asio::ip::udp::endpoint listen_endpoint(listen_address, multicast_port);
    multicast_endpoint = boost::asio::ip::udp::endpoint(multicast_address, multicast_port);

    listen_socket.open(listen_endpoint.protocol());
    listen_socket.bind(listen_endpoint);
    listen_socket.set_option(boost::asio::ip::multicast::join_group(multicast_address));

    scan_socket.open(multicast_endpoint.protocol());

    message += "M-SEARCH * HTTP/1.1\r\n";
    message += "HOST: 239.255.255.250:1982\r\n";
    message += "MAN: \"ssdp:discover\"\r\n";
    message += "ST: wifi_bulb";

    async_broadcast();
    async_receive();
    async_listen();

    thread = std::thread([&](){ this->context->run(); });
}

scanner::~scanner()
{
    context->stop();
    thread.join();
}

std::map<uint64_t, device> scanner::get_devices() const
{
    return devices;
}

std::shared_ptr<boost::asio::io_context> scanner::get_io_context() const
{
    return context;
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
            boost::asio::buffer(message), multicast_endpoint,
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
    const static std::string http_ok = "HTTP/1.1 200 OK\r\n";
    const static std::string notify = "NOTIFY * HTTP/1.1\r\n";

    auto response_iter = response.begin();
    if(std::equal(http_ok.begin(), http_ok.end(), response.begin()))
    {
        response_iter += http_ok.size();
    }
    else if(std::equal(notify.begin(), notify.end(), response.begin()))
    {
        response_iter += notify.size();
    }
    else return;

    std::map<std::string, std::string> data;

    while(true)
    {
        const auto key_end = std::find(response_iter, response.end(), ':');
        const auto value_start = key_end + 2;
        if(value_start >= response.end()) return;

        const auto value_end = std::find(value_start, response.end(), '\r');
        data.emplace(std::string(response_iter, key_end), std::string(value_start, value_end));

        if(value_end + 2 >= response.end()) break;
        response_iter = value_end + 2;
    }

    const auto index = data["Location"].find_first_of(':', 11);
    if(index == std::string::npos) return;

    const auto ip_address = data["Location"].substr(11, index - 11);
    const auto port = std::stoul(data["Location"].substr(index + 1));
    const auto id = std::stoul(data["id"], nullptr, 16);
    const auto name = data["name"];

    const auto iter = devices.find(id);
    if(iter != devices.end()) return;

    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(ip_address), port);

    yeelight::device device(context, endpoint, id, name);
    devices.try_emplace(id, device).second;
}


}