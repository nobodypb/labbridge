#include "esp_wifi.h"
#include "lwip/sockets.h"
#include "platform.hpp"
#include "rd_wifi.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <fcntl.h>
#include <map>
namespace labbridge::esp {
namespace {
constexpr const char *ap_address = "192.168.4.1";
std::string ap_ssid = "LabBridge-Setup";
constexpr int tcp_port = 8080;
class TcpLink : public Endpoint {
public:
  unsigned model;
  std::mutex io;
  std::atomic<bool> online{false}, busy{false}, reset{false};
  std::atomic<uint64_t> epoch{1};
  Bytes rx, tx;
  explicit TcpLink(unsigned m) : model(m) {}
  bool connected() const override { return online; }
  bool writable() const override { return online && !busy && !reset; }
  uint64_t generation() const override { return epoch; }
  void reset_connection() override {
    reset = true;
    online = false;
  }
  void cancel_pending() override {
    std::lock_guard<std::mutex> lock(io);
    if (!tx.empty()) {
      tx.clear();
      busy = false;
    }
  }
  bool send(const Bytes &value) override {
    std::lock_guard<std::mutex> lock(io);
    if (!online || busy || reset || value.size() > 256)
      return false;
    tx = value;
    busy = true;
    return true;
  }
  Bytes receive() override {
    std::lock_guard<std::mutex> lock(io);
    Bytes b;
    b.swap(rx);
    return b;
  }
};
struct Connection {
  int fd = -1;
  uint32_t serial = 0;
  uint64_t deadline = 0;
  rd60xx::RtuStream parser;
  std::shared_ptr<TcpLink> link;
  Bytes pending;
  size_t offset = 0;
};
std::map<uint32_t, std::shared_ptr<TcpLink>> links;
rd60xx::IdentityBindings bindings;
std::mutex management;
rd60xx::Provisioning provisioning;
uint64_t session_number = 0, accepted = 0, disconnects = 0, rejected = 0,
         rx_bytes = 0, tx_bytes = 0, provision_packets = 0;
bool listening = false;
std::string listener_error;
void nonblocking(int fd) {
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}
bool again() {
  return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
}
void close_connection(Connection &c) {
  if (c.fd < 0)
    return;
  if (c.link) {
    c.link->online = false;
    ++c.link->epoch;
    std::lock_guard<std::mutex> lock(c.link->io);
    c.link->rx.clear();
    c.link->tx.clear();
    c.link->busy = false;
    c.link->reset = false;
  }
  {
    std::lock_guard<std::mutex> lock(management);
    bindings.release(c.fd);
    ++disconnects;
  }
  shutdown(c.fd, SHUT_RDWR);
  close(c.fd);
  c = Connection{};
}
Json status(uint64_t now) {
  std::lock_guard<std::mutex> lock(management);
  provisioning.tick(now);
  auto j = provisioning.status(now), all = bindings.describe();
  Json metrics;
  metrics.set("accepted", double(accepted))
      .set("disconnects", double(disconnects))
      .set("rejected", double(rejected))
      .set("rx_bytes", double(rx_bytes))
      .set("tx_bytes", double(tx_bytes))
      .set("provision_packets", double(provision_packets));
  j.set("ap_ssid", ap_ssid)
      .set("ap_address", ap_address)
      .set("tcp_port", double(tcp_port))
      .set("listener_running", listening)
      .set("candidates", all)
      .set("metrics", metrics);
  if (!listener_error.empty())
    j.set("error", listener_error);
  return j;
}
void worker(void *) {
  std::array<Connection, 4> connections;
  int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  sockaddr_in bind_address{};
  bind_address.sin_family = AF_INET;
  bind_address.sin_port = htons(tcp_port);
  bind_address.sin_addr.s_addr = inet_addr(ap_address);
  int reuse = 1;
  if (server >= 0)
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  if (server < 0 ||
      bind(server, reinterpret_cast<sockaddr *>(&bind_address),
           sizeof(bind_address)) < 0 ||
      listen(server, 4) < 0) {
    if (server >= 0)
      close(server);
    {
      std::lock_guard<std::mutex> lock(management);
      listener_error = "Cannot start RD TCP listener on AP";
    }
    vTaskDelete(nullptr);
    return;
  }
  nonblocking(server);
  {
    std::lock_guard<std::mutex> lock(management);
    listening = true;
  }
  for (;;) {
    auto now = now_ms();
    int peer = accept(server, nullptr, nullptr);
    if (peer >= 0) {
      auto slot = std::find_if(connections.begin(), connections.end(),
                               [](auto &c) { return c.fd < 0; });
      if (slot == connections.end()) {
        close(peer);
        std::lock_guard<std::mutex> lock(management);
        ++rejected;
      } else {
        nonblocking(peer);
        int yes = 1;
        setsockopt(peer, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
        slot->fd = peer;
        slot->deadline = now + 2000;
        slot->pending = rd60xx::read(1, 0, 20);
        std::lock_guard<std::mutex> lock(management);
        ++accepted;
      }
    }
    for (auto &c : connections) {
      if (c.fd < 0)
        continue;
      if (c.link && c.link->reset) {
        close_connection(c);
        continue;
      }
      if (c.pending.empty() && c.link) {
        std::lock_guard<std::mutex> lock(c.link->io);
        c.pending.swap(c.link->tx);
        c.offset = 0;
      }
      if (!c.pending.empty()) {
        int sent = send(c.fd, c.pending.data() + c.offset,
                        c.pending.size() - c.offset, 0);
        if (sent < 0 && !again()) {
          close_connection(c);
          continue;
        }
        if (sent > 0) {
          c.offset += sent;
          std::lock_guard<std::mutex> lock(management);
          tx_bytes += sent;
        }
        if (c.offset == c.pending.size()) {
          c.pending.clear();
          c.offset = 0;
          if (c.link)
            c.link->busy = false;
        }
      }
      uint8_t bytes[512];
      int n = recv(c.fd, bytes, sizeof(bytes), 0);
      if (n == 0 || (n < 0 && !again())) {
        close_connection(c);
        continue;
      }
      if (n > 0) {
        {
          std::lock_guard<std::mutex> lock(management);
          rx_bytes += n;
        }
        if (c.link) {
          std::lock_guard<std::mutex> lock(c.link->io);
          if (c.link->rx.size() + n > 4096)
            c.link->reset = true;
          else
            c.link->rx.insert(c.link->rx.end(), bytes, bytes + n);
        } else if (!c.serial) {
          bool identity_rejected = false;
          for (auto &frame : c.parser.feed(Bytes(bytes, bytes + n))) {
            try {
              auto identity = rd60xx::identify(frame);
              std::lock_guard<std::mutex> lock(management);
              auto claim = bindings.claim(identity, c.fd);
              if (claim == rd60xx::IdentityBindings::Claim::rejected) {
                ++rejected;
                identity_rejected = true;
                break;
              }
              c.serial = identity.serial;
              if (claim == rd60xx::IdentityBindings::Claim::bound) {
                c.link = links.at(c.serial);
                {
                  std::lock_guard<std::mutex> io(c.link->io);
                  c.link->rx.clear();
                  c.link->tx.clear();
                }
                c.link->reset = false;
                c.link->busy = false;
                ++c.link->epoch;
                c.link->online = true;
              }
              provisioning.connected(identity);
              break;
            } catch (const std::invalid_argument &) {
            }
          }
          if (identity_rejected) {
            close_connection(c);
            continue;
          }
        }
      }
      if (!c.serial && now >= c.deadline)
        close_connection(c);
    }
    sleep_ms(5);
  }
}
void touch_worker(void *) {
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    {
      std::lock_guard<std::mutex> lock(management);
      listener_error = "Provisioning UDP unavailable";
      provisioning.fail();
    }
    vTaskDelete(nullptr);
    return;
  }
  int yes = 1;
  setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
  sockaddr_in local{};
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = inet_addr(ap_address);
  if (bind(sock, reinterpret_cast<sockaddr *>(&local), sizeof(local)) < 0) {
    close(sock);
    {
      std::lock_guard<std::mutex> lock(management);
      listener_error = "Cannot bind provisioning to AP";
    }
    vTaskDelete(nullptr);
    return;
  }
  sockaddr_in target{};
  target.sin_family = AF_INET;
  target.sin_port = htons(7001);
  target.sin_addr.s_addr = inet_addr("192.168.4.255");
  std::string previous;
  uint64_t cycle = 0;
  size_t index = 0;
  rd60xx::TouchPackets packets;
  std::array<uint8_t, 1024> payload{};
  for (;;) {
    std::string phase;
    {
      std::lock_guard<std::mutex> lock(management);
      provisioning.tick(now_ms());
      phase = provisioning.phase();
    }
    if (phase != "server_address" && phase != "wifi") {
      previous.clear();
      sleep_ms(50);
      continue;
    }
    if (previous != phase) {
      std::array<uint8_t, 6> mac{};
      esp_wifi_get_mac(WIFI_IF_AP, mac.data());
      packets = rd60xx::esptouch_lengths(
          ap_ssid, phase == "server_address" ? ap_address : "",
          {192, 168, 4, 1}, mac);
      previous = phase;
      cycle = now_ms();
      index = 0;
    }
    auto elapsed = (now_ms() - cycle) % 6000;
    auto &lengths = elapsed < 2000 ? packets.guide : packets.data;
    auto size = lengths[index++ % lengths.size()];
    auto n = sendto(sock, payload.data(), size, 0,
                    reinterpret_cast<sockaddr *>(&target), sizeof(target));
    {
      std::lock_guard<std::mutex> lock(management);
      if (n == size)
        ++provision_packets;
      else
        provisioning.fail();
    }
    sleep_ms(8);
  }
}
} // namespace
std::shared_ptr<Endpoint> tcp_endpoint(const std::string &serial,
                                       unsigned model) {
  auto number = std::stoul(serial);
  auto link = std::make_shared<TcpLink>(model);
  bindings.configure(number, model);
  links.emplace(number, link);
  return link;
}
bool rd_provisioning_active() {
  std::lock_guard<std::mutex> lock(management);
  provisioning.tick(now_ms());
  return provisioning.active();
}
void start_rd_network() {
  ap_ssid = configuration.text("ap_ssid", "LabBridge-Setup");
  node->bind("getRdProvisioning",
             [](const Json &, const Headers &, uint64_t now) {
               return Response{200, status(now)};
             });
  node->bind(
      "startRdProvisioning", [](const Json &, const Headers &, uint64_t now) {
        if (!node->quiescent(now) || wifi_network_busy())
          return problem(409, "Stop/release instruments and finish Wi-Fi "
                              "changes before provisioning");
        {
          std::lock_guard<std::mutex> lock(management);
          if (!listening || !listener_error.empty())
            return problem(503, "RD network service unavailable");
          try {
            provisioning.start(
                random_id() + "-p" + std::to_string(++session_number), now);
          } catch (const std::invalid_argument &e) {
            return problem(409, e.what());
          }
        }
        return Response{200, status(now)};
      });
  node->bind("advanceRdProvisioning",
             [](const Json &j, const Headers &, uint64_t now) {
               {
                 std::lock_guard<std::mutex> lock(management);
                 try {
                   provisioning.advance(j.text("session_id"),
                                        j.text("expected_phase"), now);
                 } catch (const std::invalid_argument &e) {
                   return problem(409, e.what());
                 }
               }
               return Response{200, status(now)};
             });
  node->bind("cancelRdProvisioning",
             [](const Json &j, const Headers &, uint64_t now) {
               {
                 std::lock_guard<std::mutex> lock(management);
                 try {
                   provisioning.cancel(j.text("session_id"));
                 } catch (const std::invalid_argument &e) {
                   return problem(409, e.what());
                 }
               }
               return Response{200, status(now)};
             });
  auto tcp = xTaskCreate(worker, "rd_tcp", 6144, nullptr, 4, nullptr);
  auto touch = xTaskCreate(touch_worker, "rd_pair", 4096, nullptr, 3, nullptr);
  if (tcp != pdPASS || touch != pdPASS) {
    std::lock_guard<std::mutex> lock(management);
    listener_error = "RD worker allocation failed";
  }
}
} // namespace labbridge::esp
