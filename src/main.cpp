#include "yeelight/scanner.h"
#include "telegram.h"

#include <chrono>
#include <thread>
#include <iostream>


int main()
{
    using namespace std::chrono_literals;

    yeelight::scanner scanner;
    telegram::service telegram;

    std::this_thread::sleep_for(600ms);
    auto led_strip = scanner.get_device("strip");
    auto study = scanner.get_device("study");
    auto bedroom = scanner.get_device("bedroom");

    if(led_strip != nullptr)
    {
        const std::function<void(std::string, std::string)> change_to_pink_if_silke = [&](std::string name, std::string contents)
        {
//        if(name != "Silke") return;
            if(contents != "#ikwilaandacht") return;
            led_strip->toggle_twice();
        };

        dot::connect(&telegram, &telegram::service::message_received, change_to_pink_if_silke);
    }
    else std::cerr << "strip could not connect\n";

    telegram.run();

//    while(true)
//    {
//        if(study != nullptr) study->set_rgb_color(dot::color::red(), 200ms);
//        if(bedroom != nullptr) bedroom->set_rgb_color(dot::color::red(), 200ms);
//        std::this_thread::sleep_for(400ms);
//
//        if(study != nullptr) study->set_rgb_color(dot::color::blue(), 200ms);
//        if(bedroom != nullptr) bedroom->set_rgb_color(dot::color::blue(), 200ms);
//        std::this_thread::sleep_for(400ms);
//    }

    study->set_color_temperature(4000);
    bedroom->set_rgb_color(dot::color::red());
}
