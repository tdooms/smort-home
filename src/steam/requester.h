//============================================================================
// @name        : manager.h
// @author      : Thomas Dooms
// @date        : 2/14/20
// @version     :
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description :
//============================================================================


#pragma once

#include <chrono>
#include <iostream>
#include <nlohmann/json.h>
#include <vector>

#include "../http/helper.h"


namespace steam
{

enum class persona_state
{
    offline = 0,
    online = 1,
    busy = 2,
    away = 3,
    snooze = 4,
    looking_to_trade = 5,
    looking_to_play = 6
};

struct steam_friend
{
    uint64_t steam_id;
    std::chrono::seconds friend_since;
};

struct game_info
{
    std::string game_name;
    int64_t game_version;
};

struct player_summary
{
    uint64_t steam_id;
    std::string name;

    persona_state state;
    std::chrono::seconds last_logoff;

    int64_t game_id;
    std::string game_info;
};

struct play_info
{
    int64_t game_id;
    std::string game_name;

    std::chrono::minutes playtime_2weeks;
    std::chrono::minutes playtime_forever;

    std::chrono::minutes playtime_windows;
    std::chrono::minutes playtime_linux;
    std::chrono::minutes playtime_mac;
};

class requester
{
    public:
    requester();

    std::vector<steam_friend> get_friends(const std::string& key, uint64_t steam_id);

    game_info get_game_info(const std::string& key, uint64_t game_id);

    std::vector<player_summary>
    get_player_summaries(const std::string& key, const std::vector<uint64_t>& steam_ids);

    std::vector<play_info> get_recently_played(const std::string& key, uint64_t steam_id);

    std::vector<player_summary> get_friends_playing_same_game(const std::string& key, uint64_t steam_id);

    private:
    http_requester req;
};

} // namespace steam
