#pragma once
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
namespace labbridge {
struct ConfigurationError : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct DeviceSpec {
  std::string id, driver, transport, endpoint;
  std::vector<int> pins;
};
struct BoardProfile {
  std::string id;
  std::map<std::string, unsigned> capacity;
  std::vector<int> available_pins;
  static BoardProfile stick_s3();
  static BoardProfile s3_usb_otg();
  static BoardProfile esp32_devkit();
};
class Registry {
public:
  explicit Registry(BoardProfile board) : board_(std::move(board)) {}
  void add(DeviceSpec spec);
  const std::vector<DeviceSpec> &devices() const { return devices_; }
  const BoardProfile &board() const { return board_; }

private:
  BoardProfile board_;
  std::vector<DeviceSpec> devices_;
};
} // namespace labbridge
