#include "yeelight/manager.h"
#include "telegram/telegram.h"

#include <chrono>
#include <thread>
#include <iostream>


int main()
{
    using namespace std::chrono_literals;

    telegram::manager telegram;

    yeelight::scanner scanner;
    const auto devices = yeelight::manager::make_device_list_from_json();

    const auto strip = yeelight::manager::get_device("strip", devices);
    const auto bedroom = yeelight::manager::get_device("bedroom", devices);
    const auto study = yeelight::manager::get_device("study", devices);

    if(strip.has_value())
    {
        const std::function<void(std::string, std::string)> change_to_pink_if_silke = [&](std::string name, std::string contents)
        {
//        if(name != "Silke") return;
            if(contents != "#ikwilaandacht") return;
            strip->toggle_twice();
        };

        dot::connect(&telegram, &telegram::manager::message_received, change_to_pink_if_silke);
    }
    else std::cerr << "strip could not connect\n";

    telegram.run();

//    while(true)
//    {
//        if(study.has_value()) study->set_rgb_color(dot::color::red(), 500ms);
//        if(bedroom.has_value()) bedroom->set_rgb_color(dot::color::red(), 500ms);
//        std::this_thread::sleep_for(1s);
//
//        if(study.has_value()) study->set_rgb_color(dot::color::blue(), 500ms);
//        if(bedroom.has_value()) bedroom->set_rgb_color(dot::color::blue(), 500ms);
//        std::this_thread::sleep_for(1s);
//    }

//    while(true)
//    {
//        const auto color = dot::color::random();
//        if(study.has_value()) study->set_rgb_color(color, 500ms);
//        if(bedroom.has_value()) bedroom->set_rgb_color(color, 500ms);
//        std::this_thread::sleep_for(1s);
//    }

    study->set_color_temperature(4000);
    bedroom->set_brightness(1);
    bedroom->set_rgb_color(dot::color::red());

    std::this_thread::sleep_for(1h);
    yeelight::manager::merge_to_json(devices);
}
