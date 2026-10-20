#pragma once
#include "meter_driver.hpp"
namespace labbridge::yocto_watt {
bool valid_serial(const std::string &);
std::vector<SensorSpec> channels();
std::map<std::string, double> decode(const Json &api,
                                     const std::string &serial);
} // namespace labbridge::yocto_watt
