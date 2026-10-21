#include "labbridge/stream_credentials.hpp"
#include <cassert>
using namespace labbridge;
int main() {
  uint64_t now = 0;
  unsigned nonce = 0, calls = 0;
  Auth auth(
      "synthetic-bearer", Json(),
      [&] { return std::string(60, 'a') + std::to_string(1000 + nonce++); },
      [](const std::string &, const std::string &, unsigned) {
        return std::string(64, 'b');
      },
      [](const Json &) {}, [&] { return now; });
  AuthContext lan{false, false, "http://node.local"};
  Headers h{{"Origin", lan.origin}, {"Content-Type", "application/json"}};
  auto password = Json::parse(
      R"({"password":"synthetic password","confirm_password":"synthetic password"})");
  assert(auth.request("POST", "/api/v1/auth/setup", password, {},
                      {true, false, ""})
             .status == 200);
  auto login =
      auth.request("POST", "/api/v1/auth/login",
                   Json().set("password", "synthetic password"), h, lan);
  assert(login.status == 200);
  h["Cookie"] = login.headers.at("Set-Cookie")
                    .substr(0, login.headers.at("Set-Cookie").find(';'));
  auto csrf = login.body.text("csrf_token");
  StreamCredentials access(auth, h, lan);
  auto dispatch = [&](const std::string &, const std::string &, const Json &,
                      const Headers &headers) {
    ++calls;
    assert(headers.at("Cookie") == h.at("Cookie"));
    assert(headers.at("X-Lab-CSRF") == csrf);
    return Response{200, Json().set("confirmed", true)};
  };
  StreamSession stream(
      "boot", [&](const std::string &value) { return access.verify(value); },
      [&](const std::string &m, const std::string &p, const Json &b,
          const Headers &headers) {
        return access.dispatch(m, p, b, headers, dispatch);
      });
  auto auth_reply =
      stream.receive(Json().set("type", "auth").set("csrf_token", csrf), 0);
  assert(auth_reply.text("type") == "authenticated");
  assert(
      stream
          .receive(
              Json::parse(
                  R"({"method":"POST","path":"/api/v1/devices/actor/stop","body":{}})"),
              1)
          .number("status") == 200);
  assert(calls == 1);
  StreamCredentials foreign(
      auth, {{"Cookie", h.at("Cookie")}, {"Origin", "http://foreign.invalid"}},
      lan);
  assert(!foreign.verify(csrf));
  StreamCredentials absent(auth, {{"Origin", lan.origin}}, lan);
  assert(!absent.verify(csrf));
  assert(!access.verify("incorrect-csrf"));
  assert(absent.verify("synthetic-bearer"));
  auto logout = h;
  logout["X-Lab-CSRF"] = csrf;
  assert(auth.request("DELETE", "/api/v1/auth/session", Json(), logout, lan)
             .status == 200);
  assert(!stream.authenticated());
  stream.offer_snapshot(Json().set("private", true));
  assert(stream.take_encoded_snapshot().empty());
  assert(
      stream
          .receive(
              Json::parse(
                  R"({"method":"POST","path":"/api/v1/devices/actor/stop"})"),
              2)
          .number("status") == 401);
  assert(calls == 1);
  // Expiry and password replacement also invalidate an already authenticated
  // stream; neither needs real-time waiting or an instrument to test.
  login = auth.request("POST", "/api/v1/auth/login",
                       Json().set("password", "synthetic password"), h, lan);
  assert(login.status == 200);
  h["Cookie"] = login.headers.at("Set-Cookie")
                    .substr(0, login.headers.at("Set-Cookie").find(';'));
  csrf = login.body.text("csrf_token");
  StreamCredentials expiring(auth, h, lan);
  assert(expiring.verify(csrf));
  assert(!expiring.verify("incorrect-csrf"));
  now = Auth::session_ttl_ms + 1;
  assert(!expiring.verify(csrf));
  login = auth.request("POST", "/api/v1/auth/login",
                       Json().set("password", "synthetic password"), h, lan);
  assert(login.status == 200);
  h["Cookie"] = login.headers.at("Set-Cookie")
                    .substr(0, login.headers.at("Set-Cookie").find(';'));
  csrf = login.body.text("csrf_token");
  StreamCredentials replaced(auth, h, lan);
  assert(replaced.verify(csrf));
  auto changed = Json()
                     .set("password", "replacement password")
                     .set("confirm_password", "replacement password");
  assert(auth.request("PUT", "/api/v1/auth/password", changed, {},
                      {true, false, ""})
             .status == 200);
  assert(!replaced.verify(csrf));
}
