#include "../firmware/platform/http_access.hpp"
#include <cassert>
#include <cstring>
#include <unistd.h>
using namespace labbridge;

static AuthContext endpoint(const char *ip, const char *host) {
  sockaddr_storage address{};
  socklen_t length;
  if (std::strchr(ip, ':')) {
    auto &v6 = reinterpret_cast<sockaddr_in6 &>(address);
    v6.sin6_family = AF_INET6;
    assert(inet_pton(AF_INET6, ip, &v6.sin6_addr) == 1);
    length = sizeof(v6);
  } else {
    auto &v4 = reinterpret_cast<sockaddr_in &>(address);
    v4.sin_family = AF_INET;
    assert(inet_pton(AF_INET, ip, &v4.sin_addr) == 1);
    length = sizeof(v4);
  }
  return http::context(address, length, host, "node.local");
}

int main() {
  Auth auth(
      "synthetic-bearer", Json(), [] { return std::string(64, 'a'); },
      [](const auto &, const auto &, unsigned) { return std::string(64, 'b'); },
      [](const Json &) {}, [] { return 0; });
  for (const char *ip : {"192.168.4.1", "::ffff:192.168.4.1"}) {
    auto ap = endpoint(ip, "192.168.4.1");
    assert(ap.access_point);
    assert(ap.origin == "http://192.168.4.1");
    assert(auth.authorize("GET", "/api/v1/wifi/scan", {}, ap).status == 200);
    assert(auth.request("GET", "/api/v1/auth/status", Json(), {}, ap)
               .body.boolean("setup_allowed"));
    assert(auth.authorize("GET", "/api/v1/devices", {}, ap).status == 401);
  }
  for (const char *ip : {"192.0.2.10", "::ffff:192.0.2.10"}) {
    auto lan = endpoint(ip, "192.0.2.10");
    assert(!lan.access_point);
    assert(lan.origin == "http://192.0.2.10");
    assert(auth.authorize("GET", "/api/v1/wifi/scan", {}, lan).status == 401);
    Headers post{{"Origin", lan.origin}, {"Content-Type", "application/json"}};
    auto body = Json::parse(
        R"({"password":"synthetic password","confirm_password":"synthetic password"})");
    // Valid same-origin LAN requests reach bootstrap policy, rather than being
    // rejected because a dual-stack socket was decoded as sockaddr_in.
    auto response = auth.request("POST", "/api/v1/auth/setup", body, post, lan);
    assert(response.status == 403);
    assert(response.body.text("detail").find("setup AP or USB") !=
           std::string::npos);
    assert(endpoint(ip, "NODE.LOCAL:80").origin == "http://node.local:80");
    assert(endpoint(ip, "foreign.invalid").origin.empty());
  }
  auto v6 = endpoint("2001:db8::1", "[2001:db8::1]:80");
  assert(!v6.access_point && v6.origin == "http://[2001:db8::1]:80");
  sockaddr_storage invalid{};
  assert(http::context(invalid, 0, "node.local", "node.local").origin.empty());
  assert(endpoint("::ffff:192.168.4.1", "").origin.empty());

  // Exercise getsockname on an actual dual-stack server socket, as ESP-IDF
  // creates when LWIP_IPV6 is enabled, rather than injecting an AuthContext.
  int server = socket(AF_INET6, SOCK_STREAM, 0);
  assert(server >= 0);
  int only_v6 = 0;
  assert(setsockopt(server, IPPROTO_IPV6, IPV6_V6ONLY, &only_v6,
                    sizeof(only_v6)) == 0);
  sockaddr_in6 listening{};
  listening.sin6_family = AF_INET6;
  assert(bind(server, reinterpret_cast<sockaddr *>(&listening),
              sizeof(listening)) == 0);
  assert(listen(server, 1) == 0);
  socklen_t length = sizeof(listening);
  assert(getsockname(server, reinterpret_cast<sockaddr *>(&listening),
                     &length) == 0);
  int client = socket(AF_INET, SOCK_STREAM, 0);
  assert(client >= 0);
  sockaddr_in destination{};
  destination.sin_family = AF_INET;
  destination.sin_port = listening.sin6_port;
  destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  assert(connect(client, reinterpret_cast<sockaddr *>(&destination),
                 sizeof(destination)) == 0);
  int accepted = accept(server, nullptr, nullptr);
  assert(accepted >= 0);
  sockaddr_storage local{};
  length = sizeof(local);
  assert(getsockname(accepted, reinterpret_cast<sockaddr *>(&local), &length) ==
         0);
  assert(local.ss_family == AF_INET6);
  auto actual = http::context(local, length, "127.0.0.1", "node.local");
  assert(!actual.access_point && actual.origin == "http://127.0.0.1");
  close(accepted);
  close(client);
  close(server);
}
