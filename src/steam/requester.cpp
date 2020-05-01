//============================================================================
// @name        : manager.cpp
// @author      : Thomas Dooms
// @date        : 2/14/20
// @version     : 
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description : 
//============================================================================

#include "requester.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/transformed.hpp>

namespace steam
{

requester::requester() : req("api.steampowered.com") {}

std::vector<steam_friend> requester::get_friends(const std::string& key, uint64_t steam_id)
{
    target_builder builder("/ISteamUser/GetFriendList/v0001/");
    builder("key", key)("steamid", steam_id);

    const auto response = req.get_response(builder);
    const auto json = nlohmann::json::parse(response.body());

    std::vector<steam_friend> friends;
    for(const auto& elem : json["friendslist"]["friends"])
    {
        steam_friend sf =
                {
                        std::stoul(elem["steamid"].get<std::string>()),
                        std::chrono::seconds(elem["friend_since"])
                };
        friends.emplace_back(sf);
    }
    return friends;
}

game_info requester::get_game_info(const std::string& key, uint64_t game_id)
{
    target_builder builder("/ISteamUserStats/GetSchemaForGame/v2/");
    builder("key", key)("appid", game_id);

    const auto response = req.get_response(builder);
    const auto json = nlohmann::json::parse(response.body());

    return {json["game"]["gameName"], json["game"]["gameVersion"]};
}

std::vector<player_summary> requester::get_player_summaries(const std::string& key, const std::vector<uint64_t>& steam_ids)
{
    const auto func = static_cast<std::string(*)(uint64_t)>(std::to_string);
    target_builder builder("/ISteamUser/GetPlayerSummaries/v0002/");
    builder("key", key)("steamids", boost::join(steam_ids | boost::adaptors::transformed(func), ","));

    const auto response = req.get_response(builder);
    const auto json = nlohmann::json::parse(response.body());

    std::vector<player_summary> summaries;
    for(const auto& elem : json["response"]["players"])
    {
        player_summary summary =
                {
                        std::stoul(elem["steamid"].get<std::string>()),
                        elem["personaname"].get<std::string>(),
                        elem["personastate"].get<persona_state>(),
                        std::chrono::seconds(elem["lastlogoff"]),
                        elem.find("gameid") == elem.end() ? -1 : std::stol(elem["gameid"].get<std::string>()),
                        elem.find("gameextrainfo") == elem.end() ? "" : elem["gameextrainfo"].get<std::string>()
                };

        summaries.emplace_back(std::move(summary));
    }
    return summaries;
}

std::vector<play_info> requester::get_recently_played(const std::string& key, uint64_t steam_id)
{
    target_builder builder("/IPlayerService/GetRecentlyPlayedGames/v0001/");
    builder("key", key)("steamid", steam_id);

    const auto response = req.get_response(builder);
    const auto json = nlohmann::json::parse(response.body());

    std::vector<play_info> infos;
    for(const auto& elem : json["response"]["games"])
    {
        play_info info =
                {
                        elem["appid"],
                        elem["name"],
                        std::chrono::minutes(elem["playtime_2weeks"]),
                        std::chrono::minutes(elem["playtime_forever"]),
                        std::chrono::minutes(elem["playtime_windows_forever"]),
                        std::chrono::minutes(elem["playtime_linux_forever"]),
                        std::chrono::minutes(elem["playtime_mac_forever"])
                };
        infos.emplace_back(std::move(info));
    }

    return infos;
}

std::vector<player_summary> requester::get_friends_playing_same_game(const std::string& key, uint64_t steam_id)
{
    const auto friends = get_friends(key, steam_id);

    std::vector<uint64_t> steam_ids;
    steam_ids.emplace_back(steam_id);
    for(const auto& elem : friends) steam_ids.emplace_back(elem.steam_id);

    const auto summaries = get_player_summaries(key, steam_ids);

    const auto comp = [&](const auto& elem){ return elem.steam_id == steam_id; };
    const auto iter = std::find_if(summaries.begin(), summaries.end(), comp);

    if(iter == summaries.end()) return {};
    const auto summary = *iter;

    if(summary.state == persona_state::offline or summary.game_id == -1) return {};

    std::vector<player_summary> result;
    for(const auto& elem : summaries)
    {
        if(elem.steam_id == summary.steam_id) continue;
        if(elem.state == persona_state::offline) continue;
        if(elem.game_id != summary.game_id) continue;
        result.emplace_back(elem);
    }
    return result;
}

}