// Protocol 实现 — FW-P1-T02/T03/T04/T05
#include "Protocol.h"

#include <string.h>

namespace ccb {

namespace {
FrameType typeFromString(const char* t) {
  if (!t) return FrameType::Unknown;
  if (strcmp(t, "hello_ack") == 0) return FrameType::HelloAck;
  if (strcmp(t, "device_snapshot") == 0) return FrameType::DeviceSnapshot;
  if (strcmp(t, "session_snapshot") == 0) return FrameType::SessionSnapshot;
  if (strcmp(t, "alert") == 0) return FrameType::Alert;
  if (strcmp(t, "config") == 0) return FrameType::Config;
  if (strcmp(t, "ping") == 0) return FrameType::Ping;
  if (strcmp(t, "hello") == 0) return FrameType::Hello;
  if (strcmp(t, "ack") == 0) return FrameType::Ack;
  if (strcmp(t, "pong") == 0) return FrameType::Pong;
  if (strcmp(t, "error") == 0) return FrameType::Error;
  if (strcmp(t, "button") == 0) return FrameType::Button;
  if (strcmp(t, "mute") == 0) return FrameType::Mute;
  if (strcmp(t, "page") == 0) return FrameType::Page;
  return FrameType::Unknown;
}
}  // namespace

void Protocol::parse(const std::string& line, ParseResult& out) {
  out = ParseResult{};

  if (line.empty()) {
    out.ok = false;
    out.error = "json_parse_error";
    return;
  }

  doc_.clear();
  DeserializationError err = deserializeJson(doc_, line.c_str());
  if (err) {
    out.ok = false;
    out.error = "json_parse_error";
    return;
  }
  out.ok = true;

  // protocol 校验（§4.2）
  const char* proto = doc_["protocol"] | "";
  out.protocolOk = (strcmp(proto, PROTOCOL_VERSION) == 0);

  // type
  const char* type = doc_["type"] | "";
  out.typeStr = type ? type : "";
  out.type = typeFromString(type);

  // seq（关键帧）
  if (doc_["seq"].is<int>() || doc_["seq"].is<int64_t>() || doc_["seq"].is<unsigned>() ||
      doc_["seq"].is<uint64_t>()) {
    out.seq = doc_["seq"].as<uint32_t>();
  }

  // 各类型必填字段校验（§4.5）+ 关键字段提取
  auto needStr = [&](const char* k) { return doc_[k].is<const char*>(); };
  auto needInt = [&](const char* k) {
    return doc_[k].is<int>() || doc_[k].is<int64_t>() || doc_[k].is<unsigned>() ||
           doc_[k].is<uint64_t>();
  };

  switch (out.type) {
    case FrameType::HelloAck:
      if (!needStr("adapter_version") || !needInt("seq") || !doc_["ok"].is<bool>()) {
        out.requiredOk = false;
      } else {
        out.ackOk = doc_["ok"].as<bool>();
        out.adapterVersion = doc_["adapter_version"].as<const char*>();
      }
      break;
    case FrameType::DeviceSnapshot:
      if (!needInt("seq") || !needStr("global_state") || !needStr("color") ||
          !doc_["counts"].is<JsonObject>()) {
        out.requiredOk = false;
      }
      break;
    case FrameType::SessionSnapshot:
      if (!needInt("seq") || !doc_["session"].is<JsonObject>()) out.requiredOk = false;
      break;
    case FrameType::Alert:
      if (!needInt("seq") || !needStr("kind") || !doc_["sound"].is<bool>())
        out.requiredOk = false;
      break;
    case FrameType::Config:
      if (!needInt("seq")) out.requiredOk = false;
      break;
    case FrameType::Ping:
      if (!needInt("ts_ms")) {
        out.requiredOk = false;
      } else {
        out.tsMs = doc_["ts_ms"].as<int64_t>();
      }
      break;
    default:
      break;
  }
}

bool Protocol::serializeHello(char* buf, size_t size, bool muted) {
  JsonDocument d;
  d["type"] = "hello";
  d["protocol"] = PROTOCOL_VERSION;
  d["device"] = DEVICE_NAME;
  d["fw_version"] = FW_VERSION;
  JsonArray features = d["features"].to<JsonArray>();
  features.add(FEATURES_LCD);
  features.add(FEATURES_SPEAKER);
  features.add(FEATURES_BUTTONS);
  d["muted"] = muted;
  return serializeJson(d, buf, size) > 0;
}

bool Protocol::serializeAck(char* buf, size_t size, uint32_t seq, uint32_t uptimeMs) {
  JsonDocument d;
  d["type"] = "ack";
  d["protocol"] = PROTOCOL_VERSION;
  d["seq"] = seq;
  d["uptime_ms"] = uptimeMs;
  return serializeJson(d, buf, size) > 0;
}

bool Protocol::serializePong(char* buf, size_t size, uint32_t tsMs, int64_t echoTsMs) {
  JsonDocument d;
  d["type"] = "pong";
  d["protocol"] = PROTOCOL_VERSION;
  d["ts_ms"] = tsMs;        // device 侧 uptime（无 RTC）
  d["echo_ts_ms"] = echoTsMs;
  return serializeJson(d, buf, size) > 0;
}

bool Protocol::serializeError(char* buf, size_t size, const char* code, uint32_t uptimeMs,
                              const char* msg) {
  JsonDocument d;
  d["type"] = "error";
  d["protocol"] = PROTOCOL_VERSION;
  d["code"] = code;
  d["uptime_ms"] = uptimeMs;
  if (msg) d["message"] = msg;
  return serializeJson(d, buf, size) > 0;
}

}  // namespace ccb
