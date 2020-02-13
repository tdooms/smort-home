//============================================================================
// @name        : manager.cpp
// @author      : Thomas Dooms
// @date        : 2/12/20
// @version     : 
// @copyright   : BA1 Informatica - Thomas Dooms - University of Antwerp
// @description : 
//============================================================================

#include <fstream>
#include <nlohmann/json.h>

#include "manager.h"

namespace yeelight
{

std::vector<device> manager::make_device_list_from_scanner(const yeelight::scanner& scanner)
{
    std::vector<device> result;
    for(const auto& [id, device] : scanner.get_devices())
    {
        result.emplace_back(device);
    }
    return result;
}

std::vector<device> manager::merge_scan_and_json(const yeelight::scanner& scanner)
{
    return merge_device_lists( make_device_list_from_scanner(scanner),
            make_device_list_from_json(scanner.get_io_context()));
}

std::vector<device> manager::make_device_list_from_json(std::shared_ptr<boost::asio::io_context> context)
{
    std::ifstream file("devices.json");
    const auto json = nlohmann::json::parse(file);

    std::vector<device> result;
    for(const auto& elem : json)
    {
        const auto address = boost::asio::ip::make_address(std::string(elem["ip"]));
        boost::asio::ip::tcp::endpoint endpoint(address, elem["port"]);
        result.emplace_back(context, endpoint, elem["id"], elem["name"]);
    }
    return result;
}

std::optional<yeelight::device> manager::get_device(const std::string& name, const device_list& devices)
{
    const auto comp = [&](const auto& device){ return device.get_name() == name; };
    const auto iter = std::find_if(devices.begin(), devices.end(), comp);

    if(iter == devices.end()) return {};
    else return *iter;
}

void manager::merge_to_json(const device_list& device_list)
{
    const auto list = merge_device_lists(make_device_list_from_json(nullptr), device_list);
    write_to_json(list);
}

void manager::write_to_json(const device_list& device_list)
{
    nlohmann::json json;
    for(size_t i = 0; i < device_list.size(); i++)
    {
        const auto& device = device_list[i];
        json[i] = { {"name", device.get_name()}, {"id", device.get_id()}, {"ip", device.get_ip_address()}, {"port", device.get_port()} };
    }
    std::ofstream file("devices.json");
    file << json;
}

std::vector<yeelight::device> manager::merge_device_lists(const device_list& first, const device_list& second)
{
    std::vector<device> result = first;
    result.insert(result.end(), second.begin(), second.end());

    const auto lt_comp = [](const auto& lhs, const auto& rhs){ return lhs.get_id() <  rhs.get_id(); };
    const auto eq_comp = [](const auto& lhs, const auto& rhs){ return lhs.get_id() == rhs.get_id(); };

    std::sort(result.begin(), result.end(), lt_comp);
    const auto size = std::unique(result.begin(), result.end(), eq_comp) - result.begin();
    result.resize(size);
    return result;
}
}