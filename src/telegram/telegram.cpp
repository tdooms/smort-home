//============================================================================
// @name        : telegram.cpp
// @author      : Thomas Dooms
// @date        : 1/27/20
// @version     : 
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description : 
//============================================================================

#include "telegram.h"
#include <signals/connect.h>

namespace telegram
{

manager::manager()
{
    td::Client::execute({0, td_api::make_object<td_api::setLogVerbosityLevel>(1)});
    client = std::make_unique<td::Client>();
}

manager::~manager()
{
    should_run = false;
    thread.join();
}

void manager::run()
{
    const auto func = [&]()
    {
        while(should_run)
        {
            if(needs_restart)
            {
                client.reset();
                handlers.clear();
                chat_titles.clear();
                users.clear();
            }
            else if(not is_authorized)
            {
                handle_response(client->receive(10));
            }
            else
            {
                while(handle_response(client->receive(10)));
                std::this_thread::sleep_for(100ms);
            }
        }
    };
    thread = std::thread(func);
}


void manager::send_query(td_api::object_ptr<td_api::Function> function, std::function<void(td_api::object_ptr<td_api::Object>)> handler)
{
    if(handler != nullptr)
    {
        handlers.emplace(current_query_id, std::move(handler));
    }
    client->send(td::Client::Request{current_query_id, std::move(function)});

    current_query_id++;
}

bool manager::handle_response(td::Client::Response response)
{
    if (response.object == nullptr) return false;

    if (response.id == 0)
    {
        handle_update(std::move(response.object));
    }
    else
    {
        const auto iter = handlers.find(response.id);
        if (iter != handlers.end()) iter->second(std::move(response.object));
    }
    return true;
}

void manager::handle_update(td_api::object_ptr<td_api::Object> update)
{
    td_api::downcast_call(
            *update, overloaded(
                    [&](td_api::updateAuthorizationState& update_authorization_state)
                    {
                        handle_authorization_update(std::move(update_authorization_state.authorization_state_));
                    },
                    [&](td_api::updateNewChat& update_new_chat)
                    {
                        chat_titles.try_emplace(update_new_chat.chat_->id_, std::move(update_new_chat.chat_->title_));
                    },
                    [&](td_api::updateChatTitle& update_chat_title)
                    {
                        chat_titles.try_emplace(update_chat_title.chat_id_, std::move(update_chat_title.title_));
                    },
                    [&](td_api::updateUser& update_user)
                    {
                        users.try_emplace(update_user.user_->id_, std::move(update_user.user_));
                    },
                    [&](td_api::updateNewMessage& update_new_message)
                    {
                        if(update_new_message.message_->content_->get_id() == td_api::messageText::ID)
                        {
                            const auto sender_id = update_new_message.message_->sender_user_id_;
                            const auto sender_iter = users.find(sender_id);

                            auto sender = (sender_iter != users.end()) ? sender_iter->second->first_name_ : "";
                            auto contents = static_cast<td_api::messageText&>(*update_new_message.message_->content_).text_->text_;

                            dot::emit(this, &message_received, std::move(sender), std::move(contents));
                        }
                    },
                    [&](td_api::updateCall& update_new_call)
                    {
                        std::cout << users.at(update_new_call.call_->user_id_)->first_name_ << " is calling";
                    },
                    [&]([[maybe_unused]] auto& update)
                    {
//                        std::cout << update.get_id() << '\n';
                    }
            ));
}

void manager::handle_authorization_update(td_api::object_ptr<td_api::AuthorizationState> authorization_state)
{
    td_api::downcast_call(
            *authorization_state,
            overloaded(
                    [this](td_api::authorizationStateReady &)
                    {
                        is_authorized = true;
                        std::cout << "Got authorization" << std::endl;
                    },
                    [this](td_api::authorizationStateLoggingOut &)
                    {
                        is_authorized = false;
                        std::cout << "Logging out" << std::endl;
                    },
                    [this](td_api::authorizationStateClosing &)
                    {
                        std::cout << "Closing" << std::endl;
                    },
                    [this](td_api::authorizationStateClosed &)
                    {
                        is_authorized = false;
                        needs_restart = true;
                        std::cout << "Terminated" << std::endl;
                    },
                    [this](td_api::authorizationStateWaitCode &)
                    {
                        std::cout << "Enter authentication code: " << std::flush;
                        std::string code;
                        std::cin >> code;
                        send_query(td_api::make_object<td_api::checkAuthenticationCode>(code));
                    },
                    [this](td_api::authorizationStateWaitRegistration &)
                    {
                        send_query(td_api::make_object<td_api::registerUser>(first_name, last_name));
                    },
                    [this](td_api::authorizationStateWaitPassword &)
                    {
                        std::cout << "Enter authentication password: " << std::flush;
                        std::string password;
                        std::cin >> password;
                        send_query(td_api::make_object<td_api::checkAuthenticationPassword>(password));
                    },
                    [this](td_api::authorizationStateWaitPhoneNumber &)
                    {
                        send_query(td_api::make_object<td_api::setAuthenticationPhoneNumber>(phone_number, nullptr));
                    },
                    [this](td_api::authorizationStateWaitEncryptionKey &)
                    {
                        send_query(td_api::make_object<td_api::checkDatabaseEncryptionKey>(encryption_key));
                    },
                    [this](td_api::authorizationStateWaitOtherDeviceConfirmation &)
                    {
                        throw std::runtime_error("does not support 2state authentication");
                    },
                    [this](td_api::authorizationStateWaitTdlibParameters &)
                    {
                        auto parameters = td_api::make_object<td_api::tdlibParameters>();
                        parameters->database_directory_ = "tdlib";
                        parameters->use_message_database_ = true;
                        parameters->use_secret_chats_ = true;
                        parameters->api_id_ = 1154553;
                        parameters->api_hash_ = "bac4581ae438e8c4701b05f02dee8226";
                        parameters->system_language_code_ = "en";
                        parameters->device_model_ = "Desktop";
                        parameters->system_version_ = "Unknown";
                        parameters->application_version_ = "1.0";
                        parameters->enable_storage_optimizer_ = true;

                        send_query(td_api::make_object<td_api::setTdlibParameters>(std::move(parameters)));
                    }));
}

}