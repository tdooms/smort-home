//============================================================================
// @name        : manager.h
// @author      : Thomas Dooms
// @date        : 2/12/20
// @version     : 
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description : 
//============================================================================


#pragma once

#include "scanner.h"

namespace yeelight
{
    class manager
    {
    public:
        using device_list = std::vector<yeelight::device>;

        [[nodiscard]] static device_list make_device_list_from_scanner(const yeelight::scanner& scanner);
        [[nodiscard]] static device_list merge_scan_and_json(const yeelight::scanner& scanner);
        [[nodiscard]] static device_list make_device_list_from_json(std::shared_ptr<boost::asio::io_context> context = std::make_shared<boost::asio::io_context>());

        [[nodiscard]] static std::optional<yeelight::device> get_device(const std::string& name, const device_list& devices);

        static void merge_to_json(const device_list& device_list);
        static void write_to_json(const device_list& device_list);
    private:
        static std::vector<yeelight::device> merge_device_lists(const device_list& first, const device_list& second);
    };
}





