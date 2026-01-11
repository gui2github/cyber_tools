#pragma once
// 注册service name 和对应 request 和 response 类型
#include <map>
#include <string>
#include <string_view>
// request 和 response 类型
inline static const std::map<std::string, std::pair<std::string, std::string>> service_map_impl_ = {
  {"imu_service", {"gwm.sensors.imu.IMUData", "gwm.sensors.imu.IMUData"}},
  {"/gwm/sm/parking_server", {"gwm.sm.parking.ParkingSMInfo", "gwm.sm.parking.ParkingSMResponse"}},
};

// parameter server name
static constexpr std::string_view parameter_server_name_[] = {
  "parameter_server", "parameter_client"};
static constexpr std::string_view parameter_client_name_[] = {"parameter_client"};
