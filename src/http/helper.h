//============================================================================
// @name        : requester.h
// @author      : Thomas Dooms
// @date        : 2/14/20
// @version     :
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description :
//============================================================================


#pragma once

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

class target_builder
{
    public:
    explicit target_builder(std::string path) : result(std::move(path)) {}

    template <typename Type>
    target_builder& operator()(const std::string& key, const Type& value)
    {
        if(not first) result += '&';
        else
            result += '?';
        first = false;

        result += key;
        result += '=';

        if constexpr(std::is_same_v<std::decay_t<Type>, char*> or std::is_same_v<Type, std::string>)
            result += value;
        else
            result += std::to_string(value);

        return *this;
    }

    std::string string() const { return result; }

    private:
    bool first = true;
    std::string result;
};

namespace http = boost::beast::http;

class http_requester
{
    public:
    explicit http_requester(std::string host_name);
    ~http_requester();

    http::response<http::string_body> get_response(const std::string& target);
    http::response<http::string_body> get_response(const target_builder& builder);

    private:
    boost::asio::io_context ioc;
    boost::beast::flat_buffer buffer;
    boost::beast::tcp_stream stream;

    std::string host;

    constexpr static auto port = "80";
    constexpr static auto version = 11;
};
