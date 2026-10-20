#pragma once
#include "meter_driver.hpp"
#include <cstdint>
namespace labbridge::ina228 {
struct RegisterIo {
  virtual ~RegisterIo() = default;
  virtual bool read(uint8_t, uint8_t *, size_t) = 0;
  virtual bool write(uint8_t, uint16_t) = 0;
};
struct Calibration {
  double shunt_ohms, max_current_a;
  unsigned adc_range;
  void validate() const;
  uint16_t register_value() const;
  double current_lsb() const;
};
enum class Status { valid, waiting, fault };
struct Sample {
  Status status = Status::fault;
  std::map<std::string, double> values;
};
class Reader {
  RegisterIo &io_;
  Calibration calibration_;
  bool configured_ = false, have_baseline_ = false;
  uint64_t energy_base_ = 0, charge_base_ = 0;
  bool read(uint8_t reg, size_t size, uint64_t &value);

public:
  Reader(RegisterIo &io, Calibration calibration)
      : io_(io), calibration_(calibration) {
    calibration.validate();
  }
  bool initialize();
  Sample sample();
};
std::vector<SensorSpec> channels();
} // namespace labbridge::ina228
