#include "labbridge/node.hpp"
#include "routes.hpp"
#include <algorithm>
#include <cmath>
#include <set>
namespace labbridge {
namespace {
const cJSON *field(const cJSON *j, const char *k) {
  return cJSON_GetObjectItemCaseSensitive(j, k);
}
bool validate(const cJSON *v, const cJSON *s, const cJSON *schemas,
              std::string &reason) {
  auto ref = field(s, "$ref");
  if (cJSON_IsString(ref)) {
    std::string name = ref->valuestring;
    return validate(v, field(schemas, name.substr(name.rfind('/') + 1).c_str()),
                    schemas, reason);
  }
  auto type = field(s, "type");
  std::string t = cJSON_IsString(type) ? type->valuestring : "";
  if ((t == "object" && !cJSON_IsObject(v)) ||
      (t == "array" && !cJSON_IsArray(v)) ||
      (t == "string" && !cJSON_IsString(v)) ||
      (t == "boolean" && !cJSON_IsBool(v)) ||
      ((t == "number" || t == "integer") &&
       (!cJSON_IsNumber(v) || !std::isfinite(v->valuedouble) ||
        (t == "integer" && std::floor(v->valuedouble) != v->valuedouble)))) {
    reason = "Incorrect JSON type";
    return false;
  }
  auto en = field(s, "enum");
  if (en) {
    bool found = false;
    for (auto e = en->child; e; e = e->next)
      if (cJSON_Compare(v, e, true))
        found = true;
    if (!found) {
      reason = "Value outside enum";
      return false;
    }
  }
  if (t == "number" || t == "integer") {
    auto lo = field(s, "minimum"), hi = field(s, "maximum"),
         multiple = field(s, "multipleOf");
    if ((lo && v->valuedouble < lo->valuedouble) ||
        (hi && v->valuedouble > hi->valuedouble) ||
        (multiple &&
         std::abs(v->valuedouble / multiple->valuedouble -
                  std::round(v->valuedouble / multiple->valuedouble)) > 1e-7)) {
      reason = "Number outside allowed range/step";
      return false;
    }
  }
  if (t == "string") {
    size_t len = std::string(v->valuestring).size();
    auto lo = field(s, "minLength"), hi = field(s, "maxLength");
    if ((lo && len < lo->valuedouble) || (hi && len > hi->valuedouble)) {
      reason = "String length outside range";
      return false;
    }
    if (field(s, "pattern")) {
      std::string x = v->valuestring;
      if (x.empty() || x.size() > 48 ||
          !std::all_of(x.begin(), x.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '_' || c == '-';
          })) {
        reason = "Invalid identifier";
        return false;
      }
    }
  }
  if (t == "object") {
    auto required = field(s, "required");
    if (required)
      for (auto r = required->child; r; r = r->next)
        if (!field(v, r->valuestring)) {
          reason = std::string("Missing ") + r->valuestring;
          return false;
        }
    auto properties = field(s, "properties");
    std::set<std::string> seen;
    for (auto p = v->child; p; p = p->next) {
      if (!seen.insert(p->string).second) {
        reason = "Duplicate property";
        return false;
      }
      auto prop = field(properties, p->string);
      if (!prop) {
        if (cJSON_IsFalse(field(s, "additionalProperties"))) {
          reason = "Unknown property";
          return false;
        }
      } else if (!validate(p, prop, schemas, reason))
        return false;
    }
  }
  if (t == "array") {
    auto min = field(s, "minItems"), max = field(s, "maxItems");
    int n = cJSON_GetArraySize(v);
    if ((min && n < min->valueint) || (max && n > max->valueint)) {
      reason = "Array size outside range";
      return false;
    }
    for (auto p = v->child; p; p = p->next)
      if (!validate(p, field(s, "items"), schemas, reason))
        return false;
  }
  return true;
}
std::vector<std::string> split(const std::string &s) {
  std::vector<std::string> out;
  size_t a = 0;
  for (size_t b = 0; b <= s.size(); ++b)
    if (b == s.size() || s[b] == '/') {
      out.push_back(s.substr(a, b - a));
      a = b + 1;
    }
  return out;
}
std::string header(const Headers &h, const std::string &name) {
  for (auto &p : h) {
    auto a = p.first, b = name;
    std::transform(a.begin(), a.end(), a.begin(), ::tolower);
    std::transform(b.begin(), b.end(), b.begin(), ::tolower);
    if (a == b)
      return p.second;
  }
  return "";
}
} // namespace
bool validate_request(const std::string &operation, const Json &body,
                      std::string &reason) {
  static Json rules = Json::parse(generated::constraints);
  auto schema = field(rules.at("requests"), operation.c_str());
  return !schema || validate(body.get(), schema, rules.at("schemas"), reason);
}
Response problem(int status, const std::string &detail) {
  Json j;
  j.set("type", "about:blank")
      .set("title", "Request failed")
      .set("status", double(status))
      .set("detail", detail);
  return {status, std::move(j), {{"Content-Type", "application/problem+json"}}};
}
void Node::add(const std::string &id, std::shared_ptr<Driver> d) {
  if (!d || devices_.count(id))
    throw std::invalid_argument("Duplicate/null driver");
  devices_[id] = std::move(d);
}
bool Node::owns(const std::string &token, const std::string &device,
                uint64_t now) const {
  auto l = leases_.find(token);
  return l != leases_.end() && l->second.active && now < l->second.deadline &&
         std::find(l->second.devices.begin(), l->second.devices.end(),
                   device) != l->second.devices.end();
}
void Node::release(Lease &l, uint64_t now) {
  if (!l.active)
    return;
  l.active = false;
  for (auto &d : l.devices)
    devices_.at(d)->stop(now);
}
void Node::tick(uint64_t now) {
  for (auto &p : leases_)
    if (p.second.active && now >= p.second.deadline)
      release(p.second, now);
  for (auto &p : devices_)
    p.second->tick(now);
  for (auto it = active_operations_.begin(); it != active_operations_.end();) {
    auto result = devices_.at(it->first)->operation(now);
    if (result.has("status")) {
      auto id = it->second;
      result.set("id", id).set("device_id", it->first);
      operations_[id] = result;
      auto s = result.text("status");
      if (s != "pending" && s != "running") {
        it = active_operations_.erase(it);
        continue;
      }
    }
    ++it;
  }
}
Response Node::request(const std::string &method, const std::string &path,
                       const Json &body, const Headers &headers, uint64_t now) {
  tick(now);
  std::string op, device, param;
  auto actual = split(path);
  for (auto &route : generated::routes) {
    if (method != route.method)
      continue;
    auto pattern = split(route.path);
    if (pattern.size() != actual.size())
      continue;
    bool match = true;
    std::string d, p;
    for (size_t i = 0; i < pattern.size(); ++i) {
      if (pattern[i] == "{deviceId}")
        d = actual[i];
      else if (!pattern[i].empty() && pattern[i][0] == '{')
        p = actual[i];
      else if (pattern[i] != actual[i])
        match = false;
    }
    if (match) {
      op = route.operation;
      device = d;
      param = p;
      break;
    }
  }
  if (op.empty())
    return problem(404, "Unknown route");
  std::string reason;
  if (!validate_request(op, body, reason))
    return problem(400, reason);
  if (!device.empty() && !devices_.count(device))
    return problem(404, "Unknown device");
  if (auto handler = handlers_.find(op); handler != handlers_.end())
    return handler->second(body, headers, now);
  if (op == "getNode") {
    Json j;
    j.set("id", id_)
        .set("boot_id", boot_)
        .set("firmware_version", "0.1.0-dev")
        .set("board", board_)
        .set("api_version", "0.1.0");
    return {200, j};
  }
  if (op == "listDevices") {
    auto list = Json::array();
    for (auto &p : devices_)
      list.append(p.second->describe(now));
    Json j;
    j.set("devices", list);
    return {200, j};
  }
  if (op == "getDevice")
    return {200, devices_.at(device)->describe(now)};
  if (op == "getMeasurements")
    return {200, devices_.at(device)->measurements(now)};
  if (op == "createLease") {
    std::vector<std::string> selected;
    auto a = body.at("device_ids");
    for (auto p = a->child; p; p = p->next) {
      std::string d = p->valuestring;
      if (!devices_.count(d))
        return problem(404, "Unknown lease device");
      if (std::find(selected.begin(), selected.end(), d) != selected.end())
        return problem(400, "Duplicate lease device");
      for (auto &l : leases_)
        if (owns(l.first, d, now))
          return problem(409, "Device already leased");
      selected.push_back(d);
    }
    if (leases_.size() >= 32) {
      for (auto it = leases_.begin(); it != leases_.end();)
        if (!it->second.active)
          it = leases_.erase(it);
        else
          ++it;
      if (leases_.size() >= 32)
        return problem(503, "Lease table full");
    }
    auto id = next_id("l");
    uint64_t ttl = body.number("ttl_ms", 10000);
    leases_[id] = {selected, now + ttl, ttl, true};
    param = id;
  }
  if (op == "getLease" || op == "createLease" || op == "heartbeatLease" ||
      op == "stopLease") {
    auto it = leases_.find(param);
    if (it == leases_.end())
      return problem(404, "Unknown lease");
    auto &l = it->second;
    if (op == "heartbeatLease") {
      if (!l.active)
        return problem(409, "Lease expired/stopped");
      l.deadline = now + l.ttl;
    }
    if (op == "stopLease")
      release(l, now);
    auto a = Json::array();
    for (auto &d : l.devices)
      a.append(d);
    Json j;
    j.set("id", param)
        .set("device_ids", a)
        .set("expires_at_ms", double(l.deadline))
        .set("state", l.active ? "active" : "stopped");
    return {200, j};
  }
  if (op == "getOperation") {
    auto it = operations_.find(param);
    if (it == operations_.end())
      return problem(404, "Unknown operation in this boot");
    return {200, it->second};
  }
  if (!device.empty()) {
    auto &driver = *devices_.at(device);
    if (method == "GET") {
      if (!driver.supports(op))
        return problem(422, "Capability unavailable");
      return {200, driver.execute(op, body, now)};
    }
    auto key = header(headers, "Idempotency-Key");
    if (key.empty() || key.size() > 96)
      return problem(400, "Idempotency-Key required (1..96 bytes)");
    std::string signature = method + path + body.dump();
    auto saved = keys_.find(key);
    if (saved != keys_.end()) {
      if (saved->second.signature != signature)
        return problem(409, "Idempotency key reused with different request");
      return {202,
              operations_.at(saved->second.id),
              {{"Location", "/api/v1/operations/" + saved->second.id}}};
    }
    if (keys_.size() >= 128)
      return problem(503, "Operation journal full; restart only after all "
                          "outputs are confirmed off");
    bool stop =
        op == "stopDevice" || (op == "setOutput" && !body.boolean("enabled"));
    if (!stop && !owns(header(headers, "X-Lab-Lease"), device, now))
      return problem(423, "Active device lease required");
    if (!stop && active_operations_.count(device))
      return problem(409, "Device operation in progress");
    if (!stop && !driver.supports(op))
      return problem(422, "Capability unavailable");
    Json result;
    try {
      if (stop) {
        driver.stop(now);
        result = driver.operation(now);
        if (!result.has("status"))
          result.set("status", "running")
              .set("stage", "stopping")
              .set("read_attempts", 0.)
              .set("write_attempts", 0.)
              .set("reconnects", 0.)
              .set("duration_ms", 0.)
              .set("final_output", "unknown")
              .set("restore_pending", false);
      } else
        result = driver.execute(op, body, now);
    } catch (const std::invalid_argument &e) {
      return problem(422, e.what());
    }
    auto id = next_id("o");
    result.set("id", id).set("device_id", device);
    operations_[id] = result;
    keys_.emplace(key, Saved{signature, id, device, body});
    if (result.text("status") == "running" ||
        result.text("status") == "pending")
      active_operations_[device] = id;
    return {202, result, {{"Location", "/api/v1/operations/" + id}}};
  }
  return problem(501, "Operation not configured on this node");
}
} // namespace labbridge
