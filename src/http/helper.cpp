//============================================================================
// @name        : requester.cpp
// @author      : Thomas Dooms
// @date        : 2/14/20
// @version     : 
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description : 
//============================================================================

#include "helper.h"

http_requester::http_requester(std::string host_name) : stream(ioc), host(std::move(host_name))
{
    // These objects perform our I/O
    boost::asio::ip::tcp::resolver resolver(ioc);

    // Look up the domain name
    const auto results = resolver.resolve(host, port);

    // Make the connection on the IP address we get from a lookup
    stream.connect(results);
}

http_requester::~http_requester()
{
    boost::beast::error_code ec;
    stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
}

http::response<http::string_body> http_requester::get_response(const std::string& target)
{
    // Set up an HTTP GET request message
    http::request<http::string_body> request{http::verb::get, target, version};
    request.set(http::field::host, host);
    request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    // Send the HTTP request to the remote host
    http::write(stream, request);

    // Declare a container to hold the response
    http::response<boost::beast::http::string_body> response;

    // Receive the HTTP response
    http::read(stream, buffer, response);

    // Return the request
    return response;
}

http::response<http::string_body> http_requester::get_response(const target_builder& builder)
{
    return get_response(builder.string());
}