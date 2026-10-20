#include "esp_wifi.h"
#include "lwip/sockets.h"
#include "platform.hpp"
#include "rd_wifi.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <fcntl.h>
#include <map>
#include <set>
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
uint64_t provision_bursts = 0, provision_join_acks = 0;
bool listening = false;
std::string listener_error, provision_error;
std::atomic<uint32_t> provision_tx_completed{0}, provision_tx_failed{0};
void touch_tx_done(const esp_80211_tx_info_t *info) {
  if (!info || info->ifidx != WIFI_IF_AP)
    return;
  if (info->tx_status == WIFI_SEND_SUCCESS)
    ++provision_tx_completed;
  else
    ++provision_tx_failed;
}
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
      .set("provision_packets", double(provision_packets))
      .set("provision_bursts", double(provision_bursts))
      .set("provision_join_acks", double(provision_join_acks))
      .set("provision_tx_completed", double(provision_tx_completed.load()))
      .set("provision_tx_failed", double(provision_tx_failed.load()));
  j.set("ap_ssid", ap_ssid)
      .set("ap_address", ap_address)
      .set("tcp_port", double(tcp_port))
      .set("listener_running", listening)
      .set("candidates", all)
      .set("metrics", metrics);
  wifi_sta_list_t stations{};
  if (esp_wifi_ap_get_sta_list(&stations) == ESP_OK)
    j.set("ap_clients", double(stations.num));
  uint8_t channel = 0;
  wifi_second_chan_t secondary;
  if (esp_wifi_get_channel(&channel, &secondary) == ESP_OK && channel)
    j.set("ap_channel", double(channel));
  if (!listener_error.empty())
    j.set("error", listener_error);
  else if (!provision_error.empty())
    j.set("error", provision_error);
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
  // Use the normal stack in both AP modes, matching the PC's multicast UDP
  // transport. Raw unencrypted broadcasts are not the PC wire profile.
  int udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  int ack = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  sockaddr_in local{}, result_address{};
  local.sin_family = result_address.sin_family = AF_INET;
  local.sin_addr.s_addr = result_address.sin_addr.s_addr =
      inet_addr(ap_address);
  result_address.sin_port = htons(18266);
  in_addr multicast_interface{};
  multicast_interface.s_addr = inet_addr(ap_address);
  if (udp < 0 || ack < 0 ||
      bind(udp, reinterpret_cast<sockaddr *>(&local), sizeof(local)) < 0 ||
      setsockopt(udp, IPPROTO_IP, IP_MULTICAST_IF, &multicast_interface,
                 sizeof(multicast_interface)) < 0 ||
      bind(ack, reinterpret_cast<sockaddr *>(&result_address),
           sizeof(result_address)) < 0) {
    if (udp >= 0)
      close(udp);
    if (ack >= 0)
      close(ack);
    {
      std::lock_guard<std::mutex> lock(management);
      listener_error = "Provisioning UDP unavailable";
      provisioning.fail();
    }
    vTaskDelete(nullptr);
    return;
  }
  nonblocking(udp);
  nonblocking(ack);
  // Retain diagnostic counters for older clients, but this sender does not
  // use raw TX and must not count UDP submissions as radio completions.
  esp_wifi_register_80211_tx_cb(touch_tx_done);
  rd60xx::PcTouchSender sender;
  std::string previous_phase, previous_session;
  std::set<uint32_t> seen_acks;
  std::array<uint8_t, 515> payload{};
  uint64_t backpressure_since = 0;
  size_t burst_packets = 0, burst_size = 0;
  bool acknowledged = false;
  for (;;) {
    std::string phase, session;
    {
      std::lock_guard<std::mutex> lock(management);
      provisioning.tick(now_ms());
      phase = provisioning.phase();
      session = provisioning.session_id();
    }
    if (phase != "server_address" && phase != "wifi") {
      sender.stop();
      previous_phase.clear();
      sleep_ms(5);
      continue;
    }
    if (previous_phase != phase || previous_session != session) {
      if (previous_session != session) {
        sender = rd60xx::PcTouchSender{};
        seen_acks.clear();
      }
      // Remove late results before arming a new phase. An ACK never advances
      // management state or authorizes an instrument; TCP identity is separate.
      std::array<uint8_t, 64> discard{};
      for (int i = 0; i < 8 && recv(ack, discard.data(), discard.size(), 0) > 0;
           ++i) {
      }
      try {
        auto password = read_nvs("ap").text("psk");
        auto data = rd60xx::pc_touch_data(phase == "server_address", ap_ssid,
                                          password, {192, 168, 4, 1});
        burst_size = 156 + data.size();
        burst_packets = 0;
        sender.start(std::move(data), now_ms(), phase == "wifi");
      } catch (const std::invalid_argument &e) {
        std::lock_guard<std::mutex> lock(management);
        provision_error = e.what();
        provisioning.fail();
        continue;
      }
      previous_phase = phase;
      previous_session = session;
      acknowledged = false;
      backpressure_since = 0;
    }
    if (phase == "wifi") {
      for (int i = 0; i < 4; ++i) {
        std::array<uint8_t, 64> bytes{};
        sockaddr_in source{};
        socklen_t length = sizeof(source);
        auto n = recvfrom(ack, bytes.data(), bytes.size(), 0,
                          reinterpret_cast<sockaddr *>(&source), &length);
        if (n < 0)
          break;
        auto ip =
            rd60xx::pc_join_address(Bytes(bytes.begin(), bytes.begin() + n));
        if (!ip || (*ip)[0] != 192 || (*ip)[1] != 168 || (*ip)[2] != 4 ||
            (*ip)[3] < 2 || (*ip)[3] == 255)
          continue;
        auto value =
            htonl((uint32_t((*ip)[0]) << 24) | (uint32_t((*ip)[1]) << 16) |
                  (uint32_t((*ip)[2]) << 8) | (*ip)[3]);
        if (source.sin_addr.s_addr != value || !seen_acks.insert(value).second)
          continue;
        {
          std::lock_guard<std::mutex> lock(management);
          ++provision_join_acks;
        }
        sender.stop();
        acknowledged = true;
      }
    }
    auto now = now_ms();
    auto packet = sender.peek(now);
    if (acknowledged || !packet) {
      sleep_ms(5);
      continue;
    }
    auto group = packet->group;
    std::string destination = "234." + std::to_string(group) + "." +
                              std::to_string(group) + "." +
                              std::to_string(group);
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(7001);
    target.sin_addr.s_addr = inet_addr(destination.c_str());
    auto n = sendto(udp, payload.data(), packet->length, 0,
                    reinterpret_cast<sockaddr *>(&target), sizeof(target));
    if (n == packet->length) {
      sender.sent(now_ms());
      backpressure_since = 0;
      std::lock_guard<std::mutex> lock(management);
      ++provision_packets;
      if (++burst_packets == burst_size) {
        ++provision_bursts;
        burst_packets = 0;
      }
      // No intentional packet delay. The UDP/driver queue supplies
      // backpressure.
    } else if (again() || errno == ENOMEM || errno == ENOBUFS) {
      if (!backpressure_since)
        backpressure_since = now;
      if (now - backpressure_since >= 1000) {
        std::lock_guard<std::mutex> lock(management);
        provision_error = "Provisioning UDP backpressure exceeded one second";
        provisioning.fail();
      }
      sleep_ms(1);
    } else {
      auto error = errno;
      std::lock_guard<std::mutex> lock(management);
      provision_error =
          "Provisioning UDP send failed: " + std::to_string(error);
      provisioning.fail();
    }
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
            provision_error.clear();
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
