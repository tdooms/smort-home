//============================================================================
// @name        : telegram.h
// @author      : Thomas Dooms
// @date        : 1/18/20
// @version     : 
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description : 
//============================================================================


#pragma once

#include <td/telegram/td_api.h>
#include <td/telegram/Client.h>
#include <signals/connect.h>

#include <map>
#include <functional>
#include <iostream>
#include <chrono>
#include <thread>

// stolen from github something something
namespace detail
{
    template <class... Fs>
    struct overload;

    template <class F>
    struct overload<F> : public F
    {
        explicit overload(F f) : F(f) {}
    };

    template <class F, class... Fs>
    struct overload<F, Fs...> : public overload<F>, overload<Fs...>
    {
        explicit overload(F f, Fs... fs) : overload<F>(f), overload<Fs...>(fs...) {}

        using overload<F>::operator();
        using overload<Fs...>::operator();
    };
}  // namespace detail

template <class... F>
auto overloaded(F... f)
{
    return detail::overload<F...>(f...);
}

namespace td_api = td::td_api;
using namespace std::chrono_literals;

namespace telegram
{

class service
{
public:
    explicit service();
    ~service();

    void run();

    signal(message_received, std::string, std::string);

private:
    void send_query(td_api::object_ptr<td_api::Function> function, std::function<void(td_api::object_ptr<td_api::Object>)> handler = nullptr);

    bool handle_response(td::Client::Response response);

    void handle_update(td_api::object_ptr<td_api::Object> update);

    void handle_authorization_update(td_api::object_ptr<td_api::AuthorizationState> authorization_state);

    std::unique_ptr<td::Client> client;

    bool is_authorized = false;
    bool needs_restart = false;

    std::map<std::uint64_t, std::function<void(td_api::object_ptr<td_api::Object>)>> handlers;
    std::uint64_t current_query_id = 1;

    std::map<std::int32_t, std::string> chat_titles;
    std::map<std::int32_t, td_api::object_ptr<td_api::user>> users;

    std::string first_name = "Thomas";
    std::string last_name = "Dooms";
    std::string phone_number = "+32478523443";
    std::string encryption_key = "t";

    std::thread thread;
    bool should_run = true;
};

}