// AppState 实现 — FW-P2-T01/T02/T04/T05
#include "AppState.h"

#ifdef ESP32
#include <Preferences.h>
#endif

namespace ccb {

namespace {
const char* strOrEmpty(const char* s) { return s ? s : ""; }

bool isNumber(JsonVariantConst v) {
  return v.is<int>() || v.is<unsigned>() || v.is<int64_t>() || v.is<uint64_t>() ||
         v.is<float>() || v.is<double>();
}

float readNumber(JsonVariantConst v) { return v.as<float>(); }
}  // namespace

void AppState::applyDeviceSnapshot(const JsonDocument& doc) {
  globalState = globalStateFromString(doc["global_state"] | "");
  color = colorFromString(doc["color"] | "");

  JsonObjectConst c = doc["counts"].as<JsonObjectConst>();
  if (c) {
    counts.sessions = c["sessions"] | 0;
    counts.working = c["working"] | 0;
    counts.attention = c["attention"] | 0;
    counts.error = c["error"] | 0;
  }

  // focus_session（可为 null）
  focus.present = false;
  JsonObjectConst fs = doc["focus_session"].as<JsonObjectConst>();
  if (fs) {
    focus.present = true;
    focus.id = strOrEmpty(fs["id"]);
    focus.label = strOrEmpty(fs["label"]);
    focus.repo = strOrEmpty(fs["repo"]);
    focus.cwd = strOrEmpty(fs["cwd"]);
    focus.state = sessionStateFromString(fs["state"] | "");
    focus.title = strOrEmpty(fs["title"]);
    focus.line1 = strOrEmpty(fs["line1"]);
    focus.line2 = strOrEmpty(fs["line2"]);
    if (isNumber(fs["progress"])) {
      focus.hasProgress = true;
      focus.progress = readNumber(fs["progress"]);
    } else {
      focus.hasProgress = false;
    }
  }

  // alert（可为 null）
  alert.present = false;
  JsonObjectConst al = doc["alert"].as<JsonObjectConst>();
  if (al) {
    alert.present = true;
    alert.kind = alertKindFromString(al["kind"] | "");
    alert.sound = al["sound"] | false;
  }
}

void AppState::applySessionSnapshot(const JsonDocument& doc) {
  JsonObjectConst s = doc["session"].as<JsonObjectConst>();
  if (!s) {
    detail.present = false;
    return;
  }
  detail.present = true;
  detail.idShort = strOrEmpty(s["session_id_short"]);
  detail.state = sessionStateFromString(s["state"] | "");
  detail.color = colorFromString(s["color"] | "");
  detail.repo = strOrEmpty(s["repo"]);
  detail.cwdLabel = strOrEmpty(s["cwd_label"]);
  detail.title = strOrEmpty(s["title"]);
  detail.line1 = strOrEmpty(s["line1"]);
  detail.line2 = strOrEmpty(s["line2"]);
  if (isNumber(s["progress"])) {
    detail.hasProgress = true;
    detail.progress = readNumber(s["progress"]);
  } else {
    detail.hasProgress = false;
  }
  detail.ageSec = s["age_sec"] | 0;
}

void AppState::applyConfig(const JsonDocument& doc) {
  if (doc["sound_enabled"].is<bool>()) {
    config.hasSoundEnabled = true;
    config.soundEnabled = doc["sound_enabled"].as<bool>();
  }
  if (doc["privacy_mode"].is<bool>()) {  // FW-P2-T05
    config.hasPrivacyMode = true;
    config.privacyMode = doc["privacy_mode"].as<bool>();
  }
  if (doc["brightness"].is<int>()) {
    config.hasBrightness = true;
    config.brightness = doc["brightness"].as<int>();
  }
  if (doc["done_ttl_ms"].is<int>()) {
    config.hasDoneTtl = true;
    config.doneTtlMs = doc["done_ttl_ms"].as<int>();
  }
}

void AppState::applyHelloAck(bool ok) {
  handshakeDone = ok;
  handshakeError = !ok;
}

void AppState::setMuted(bool m) {
  if (m == muted) return;
  muted = m;
  saveMuted();
}

void AppState::loadMuted() {
#ifdef ESP32
  Preferences p;
  p.begin("ccb", true);
  muted = p.getBool("muted", false);
  p.end();
#else
  muted = false;
#endif
}

void AppState::saveMuted() {
#ifdef ESP32
  Preferences p;
  p.begin("ccb", false);
  p.putBool("muted", muted);
  p.end();
#endif
}

void AppState::resetConnection() {
  handshakeDone = false;
  handshakeError = false;
  globalState = GlobalState::DeviceDisconnected;
  color = Color::Gray;
}

}  // namespace ccb
