//============================================================================
// @name        : util.h
// @author      : Thomas Dooms
// @date        : 2/10/20
// @version     : 
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description : 
//============================================================================


#pragma once

namespace yeelight
{
    enum class color_mode
    {
        rgb = 1,
        temperature = 2,
        hsv = 3,
        sleep = 7
    };

    enum class flow_stop_action
    {
        recover = 0,
        stay = 1,
        turn_off = 2
    };


    struct flow_state
    {
        std::chrono::milliseconds duration;
        color_mode mode;
        size_t temperature;
        dot::color color;
        size_t brightness;
    };

    struct temperature_color
    {
        size_t temperature;
    };

    using rgb_color = dot::color;

    using device_color = std::variant<temperature_color, rgb_color>;

}