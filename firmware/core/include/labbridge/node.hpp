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
};
class Node {
public:
  using Handler =
      std::function<Response(const Json &, const Headers &, uint64_t)>;
  Node(std::string id, std::string boot, std::string board)
      : id_(std::move(id)), boot_(std::move(boot)), board_(std::move(board)) {}
  void add(const std::string &id, std::shared_ptr<Driver> driver);
  void bind(const std::string &operation, Handler handler) {
    handlers_[operation] = std::move(handler);
  }
  Response request(const std::string &method, const std::string &path,
                   const Json &body, const Headers &, uint64_t now);
  void tick(uint64_t now);

private:
  struct Lease {
    std::vector<std::string> devices;
    uint64_t deadline, ttl;
    bool active;
  };
  struct Saved {
    std::string signature, id, device;
    Json body;
  };
  std::string id_, boot_, board_;
  uint64_t counter_ = 0;
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
