#pragma once
#include "json.hpp"
#include <functional>
#include <map>
#include <memory>
#include <vector>
namespace labbridge {
using Headers = std::map<std::string, std::string>;
struct Response {
  int status;
  Json body;
  Headers headers{};
};
class Driver {
public:
  virtual ~Driver() = default;
  virtual Json describe(uint64_t) = 0;
  virtual Json measurements(uint64_t) = 0;
  virtual bool supports(const std::string &) const = 0;
  virtual void tick(uint64_t) = 0;
  virtual void stop(uint64_t) = 0;
  virtual Json execute(const std::string &, const Json &, uint64_t) = 0;
  virtual Json operation(uint64_t) { return Json(); }
  virtual bool safe_to_reconfigure(uint64_t now) {
    auto d = describe(now);
    return d.text("output") == "off" && d.text("state") == "ready";
  }
};
struct CellGuard {
  std::string sensor, channel;
  double minimum_c, maximum_c;
  uint64_t max_age_ms;
};
class Node {
public:
  using Handler =
      std::function<Response(const Json &, const Headers &, uint64_t)>;
  Node(std::string id, std::string boot, std::string board,
       std::string version = "0.1.0-dev")
      : id_(std::move(id)), boot_(std::move(boot)), board_(std::move(board)),
        version_(std::move(version)) {}
  void add(const std::string &id, std::shared_ptr<Driver> driver);
  void bind(const std::string &operation, Handler handler) {
    handlers_[operation] = std::move(handler);
  }
  Response request(const std::string &method, const std::string &path,
                   const Json &body, const Headers &, uint64_t now);
  void tick(uint64_t now);
  void inhibit(std::string reason, uint64_t now);
  bool quiescent(uint64_t now);
  bool idle(uint64_t now);
  void cell_guard(const std::string &device, CellGuard guard);

private:
  struct Lease {
    std::vector<std::string> devices;
    uint64_t deadline, ttl;
    bool active;
  };
  struct Saved {
    std::string signature, id, device;
    uint64_t created_ms;
  };
  bool temperature_ok(const std::string &, uint64_t) const;
  void cancel_active(const std::string &, const std::string &);
  std::map<std::string, CellGuard> cell_guards_;
  std::map<std::string, bool> guard_tripped_;
  std::string id_, boot_, board_, version_, inhibition_;
  uint64_t counter_ = 0, prune_at_ = 0;
  std::map<std::string, std::shared_ptr<Driver>> devices_;
  std::map<std::string, Handler> handlers_;
  std::map<std::string, Lease> leases_;
  std::map<std::string, Saved> keys_;
  std::map<std::string, Json> operations_;
  std::map<std::string, std::string> active_operations_;
  std::string next_id(const std::string &prefix) {
    return boot_ + "-" + prefix + std::to_string(++counter_);
  }
  bool owns(const std::string &token, const std::string &device,
            uint64_t now) const;
  void release(Lease &, uint64_t now);
};
bool validate_request(const std::string &operation, const Json &body,
                      std::string &reason);
Response problem(int status, const std::string &detail);
} // namespace labbridge
