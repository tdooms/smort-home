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

#include <filesystem>
#include <fstream>
#include <nlohmann/json.h>

#include "scanner.h"

namespace yeelight
{


Scanner::Scanner(std::function<void(std::unique_ptr<Device>)> handler)
: context(std::make_shared<boost::asio::io_context>()), work(*context), listen_socket(*context),
  scan_socket(*context), timer(*context), message(), buffer(buffer_size, '\0'), devices(), handler(std::move(handler))
{
    const auto listen_address    = boost::asio::ip::address();
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

    read_from_file();
    thread = std::thread([&]() { context->run(); });
}

Scanner::~Scanner()
{
    context->stop();
    write_to_file(); // while we wait for the context to stop we cna write back to the file
    thread.join();
}

void Scanner::async_receive()
{
    const auto handler = [&](const auto& error, auto bytes) {
        if(error) return;

        handle_response(std::string_view(buffer.data(), bytes));
        async_receive();
    };

    scan_socket.async_receive(boost::asio::buffer(buffer, buffer_size), handler);
}

void Scanner::async_broadcast()
{
    const auto handler = [&](const auto& error, [[maybe_unused]] auto bytes) {
        if(error) return;

        timer.expires_from_now(boost::posix_time::seconds(1));
        timer.async_wait([&](auto& error) {
            if(error) return;
            async_broadcast();
        });
    };

    scan_socket.async_send_to(boost::asio::buffer(message), multicast_endpoint, handler);
}

void Scanner::async_listen()
{
    const auto handler = [&](const auto& error, auto bytes) {
        if(error) return;
        handle_response(std::string_view(buffer.data(), bytes));
        async_listen();
    };

    listen_socket.async_receive_from(boost::asio::buffer(buffer, buffer_size), sender_endpoint, handler);
}

void Scanner::handle_response(std::string_view response)
{
    const static std::string http_ok = "HTTP/1.1 200 OK\r\n";
    const static std::string notify  = "NOTIFY * HTTP/1.1\r\n";

    auto response_iter = response.begin();
    if(std::equal(http_ok.begin(), http_ok.end(), response.begin()))
    {
        response_iter += http_ok.size();
    }
    else if(std::equal(notify.begin(), notify.end(), response.begin()))
    {
        response_iter += notify.size();
    }
    else
        return;

    std::map<std::string, std::string> data;

    while(true)
    {
        const auto key_end     = std::find(response_iter, response.end(), ':');
        const auto value_start = key_end + 2;
        if(value_start >= response.end()) return;

        const auto value_end = std::find(value_start, response.end(), '\r');
        data.emplace(std::string(response_iter, key_end), std::string(value_start, value_end));

        if(value_end + 2 >= response.end()) break;
        response_iter = value_end + 2;
    }

    const auto index = data["Location"].find_first_of(':', 11);
    if(index == std::string::npos) return;

    const auto id   = std::stoul(data["id"], nullptr, 16);
    const auto ip   = data["Location"].substr(11, index - 11);

    handle_new_device(id, ip);
}

void Scanner::read_from_file()
{
    std::ifstream file(path);
    const auto json = nlohmann::json::parse(file);

    for(const auto& elem : json)
    {
        handle_new_device(elem["id"], elem["ip"]);
    }
}

void Scanner::write_to_file()
{
    std::ofstream file(path);
    auto json = nlohmann::json();

    size_t index = 0;
    for(const auto& elem : devices)
    {
        json[index]["id"] = elem.first;
        json[index]["ip"] = elem.second;
        index++;
    }
    file << json << std::flush;
}

void Scanner::handle_new_device(uint64_t id, std::string ip)
{
    const auto [iter, emplaced] = devices.try_emplace(id, ip);
    if (not emplaced) return;

    const auto address = boost::asio::ip::make_address(ip);
    const auto endpoint = boost::asio::ip::tcp::endpoint(address, tcp_port);

    handler(std::make_unique<Device>(context, endpoint));
}


} // namespace yeelight