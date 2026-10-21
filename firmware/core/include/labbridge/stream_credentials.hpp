#pragma once
#include "auth.hpp"
#include "stream.hpp"
namespace labbridge {
// Bind stream credentials to the HTTP upgrade's trusted socket origin and
// HttpOnly cookie. RPC messages cannot replace either of those identities.
class StreamCredentials {
  Auth &auth_;
  Headers upgrade_;
  AuthContext context_;
  Headers bound(const std::string &credential) const {
    Headers result{{"Authorization", "Bearer " + credential},
                   {"X-Lab-CSRF", credential},
                   {"Content-Type", "application/json"}};
    for (auto name : {"Cookie", "Origin"}) {
      auto found = upgrade_.find(name);
      if (found != upgrade_.end())
        result[name] = found->second;
    }
    return result;
  }

public:
  StreamCredentials(Auth &auth, Headers upgrade, AuthContext context)
      : auth_(auth), upgrade_(std::move(upgrade)),
        context_(std::move(context)) {
    context_.local = false;
  }
  bool verify(const std::string &credential) {
    // A private mutation policy also checks cookie CSRF and Origin. Public AP
    // setup/status routes cannot authenticate a stream or release any data.
    return !credential.empty() && auth_.authorize("POST", "/api/v1/devices",
                                                  bound(credential), context_)
                                          .status == 200;
  }
  Response dispatch(const std::string &method, const std::string &path,
                    const Json &body, const Headers &rpc,
                    const StreamSession::Dispatch &execute) {
    if (Auth::route(path))
      return problem(403,
                     "Password/session management uses the HTTP auth worker");
    auto found = rpc.find("Authorization");
    if (found == rpc.end() || found->second.rfind("Bearer ", 0) != 0)
      return problem(401, "Authenticate the stream first");
    auto headers = bound(found->second.substr(7));
    for (auto name : {"X-Lab-Lease", "Idempotency-Key", "If-Match"}) {
      auto field = rpc.find(name);
      if (field != rpc.end())
        headers[name] = field->second;
    }
    auto access = auth_.authorize(method, path, headers, context_);
    if (access.status != 200)
      return access;
    return execute(method, path, body, headers);
  }
};
} // namespace labbridge
