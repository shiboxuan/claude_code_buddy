// Native 单元测试 — ccb::AppState 帧应用逻辑 + Theme/Types（FW-P2-T01/T02/T03）
// AppState.cpp 为纯 C++（NVS 部分在非 ESP32 下为 no-op），可本地编译。
// 编译：
//   g++ -std=c++17 -Isrc -I.pio/libdeps/m5stack-sticks3/ArduinoJson/src \
//       tools/host_test/test_appstate.cpp src/app/AppState.cpp -o /tmp/test_appstate

#include "app/AppState.h"
#include "ui/Theme.h"

#include <cstdio>
#include <cstring>

static int failures = 0;
#define CHECK(cond, msg)                                     \
  do {                                                       \
    if (!(cond)) {                                           \
      printf("  FAIL: %s\n", msg);                           \
      ++failures;                                            \
    } else {                                                 \
      printf("  pass: %s\n", msg);                           \
    }                                                        \
  } while (0)

int main() {
  ccb::AppState s;
  JsonDocument d;

  // 1. device_snapshot 完整（global/color/counts/focus/alert）
  deserializeJson(d, R"({"type":"device_snapshot","protocol":"ccb-serial-v1","seq":2,"global_state":"working","color":"red","counts":{"sessions":3,"working":1,"attention":1,"error":1},"focus_session":{"id":"abc","label":"main","repo":"~/p","cwd":"~/p/src","state":"working","title":"editing","line1":"tool: Edit","line2":"file: x.cpp","progress":42},"alert":{"kind":"attention","sound":true}})");
  s.applyDeviceSnapshot(d);
  CHECK(s.globalState == ccb::GlobalState::Working, "ds global_state=working");
  CHECK(s.color == ccb::Color::Red, "ds color=red");
  CHECK(s.counts.sessions == 3 && s.counts.working == 1 && s.counts.attention == 1 && s.counts.error == 1, "ds counts");
  CHECK(s.focus.present && s.focus.id == "abc" && s.focus.label == "main" && s.focus.repo == "~/p" && s.focus.cwd == "~/p/src", "ds focus fields");
  CHECK(s.focus.state == ccb::SessionState::Working, "ds focus state=working");
  CHECK(s.focus.title == "editing" && s.focus.line1 == "tool: Edit" && s.focus.line2 == "file: x.cpp", "ds focus title/line1/line2");
  CHECK(s.focus.hasProgress && s.focus.progress == 42.0f, "ds focus progress=42");
  CHECK(s.alert.present && s.alert.kind == ccb::AlertKind::Attention && s.alert.sound, "ds alert");

  // 2. device_snapshot focus/alert 为 null
  deserializeJson(d, R"({"type":"device_snapshot","protocol":"ccb-serial-v1","seq":3,"global_state":"idle","color":"green","counts":{"sessions":0,"working":0,"attention":0,"error":0},"focus_session":null,"alert":null})");
  s.applyDeviceSnapshot(d);
  CHECK(s.globalState == ccb::GlobalState::Idle && s.color == ccb::Color::Green, "ds idle/green");
  CHECK(!s.focus.present && !s.alert.present, "ds null focus/alert");

  // 3. device_snapshot progress 为 null
  deserializeJson(d, R"({"type":"device_snapshot","protocol":"ccb-serial-v1","seq":6,"global_state":"idle","color":"green","counts":{"sessions":0,"working":0,"attention":0,"error":0},"focus_session":{"id":"x","label":"y","repo":"r","cwd":"c","state":"idle","title":"t","line1":"a","line2":"b","progress":null},"alert":null})");
  s.applyDeviceSnapshot(d);
  CHECK(s.focus.present && !s.focus.hasProgress, "ds focus progress=null");

  // 4. session_snapshot（含 float progress + age_sec）
  deserializeJson(d, R"({"type":"session_snapshot","protocol":"ccb-serial-v1","seq":4,"session":{"session_id_short":"abc123","state":"attention","color":"yellow","repo":"~/p","cwd_label":"src","title":"t","line1":"l1","line2":"l2","progress":75.5,"age_sec":200}})");
  s.applySessionSnapshot(d);
  CHECK(s.detail.present && s.detail.idShort == "abc123", "ss id_short");
  CHECK(s.detail.state == ccb::SessionState::Attention && s.detail.color == ccb::Color::Yellow, "ss state/color");
  CHECK(s.detail.repo == "~/p" && s.detail.cwdLabel == "src", "ss repo/cwd_label");
  CHECK(s.detail.title == "t" && s.detail.line1 == "l1" && s.detail.line2 == "l2", "ss title/lines");
  CHECK(s.detail.hasProgress && s.detail.progress == 75.5f, "ss progress=75.5");
  CHECK(s.detail.ageSec == 200, "ss age_sec");

  // 5. config（全部字段，含 privacy_mode 扩展位）
  deserializeJson(d, R"({"type":"config","protocol":"ccb-serial-v1","seq":5,"sound_enabled":false,"privacy_mode":true,"brightness":40,"done_ttl_ms":3000})");
  s.applyConfig(d);
  CHECK(s.config.hasSoundEnabled && !s.config.soundEnabled, "cfg sound_enabled=false");
  CHECK(s.config.hasPrivacyMode && s.config.privacyMode, "cfg privacy_mode=true (扩展位)");
  CHECK(s.config.hasBrightness && s.config.brightness == 40, "cfg brightness=40");
  CHECK(s.config.hasDoneTtl && s.config.doneTtlMs == 3000, "cfg done_ttl_ms=3000");

  // 6. hello_ack
  s.applyHelloAck(true);
  CHECK(s.handshakeDone && !s.handshakeError, "hello_ack ok=true");
  s.applyHelloAck(false);
  CHECK(!s.handshakeDone && s.handshakeError, "hello_ack ok=false");

  // 7. setMuted 不重复写（仅测状态翻转，NVS 在 native 下 no-op）
  s.muted = false;
  s.setMuted(true);
  CHECK(s.muted == true, "setMuted(true)");
  s.setMuted(true);
  CHECK(s.muted == true, "setMuted(true) idempotent");

  // 8. Theme 颜色映射
  CHECK(ccb::theme::rgb565(ccb::Color::Green) == ccb::theme::hexToRgb565(ccb::theme::HEX_GREEN), "theme rgb565(green)");
  CHECK(ccb::theme::colorFor(ccb::GlobalState::Working) == ccb::Color::Red, "theme colorFor(working)=red");
  CHECK(ccb::theme::colorFor(ccb::GlobalState::Idle) == ccb::Color::Green, "theme colorFor(idle)=green");
  CHECK(ccb::theme::colorFor(ccb::GlobalState::Attention) == ccb::Color::Yellow, "theme colorFor(attention)=yellow");
  CHECK(ccb::theme::SCREEN_W == 135 && ccb::theme::SCREEN_H == 240, "theme screen 135x240");
  CHECK(ccb::theme::STATUS_BAR_H >= 12 && ccb::theme::STATUS_BAR_H <= 18, "theme status bar 12-18");

  // 9. Types 字符串转换
  CHECK(ccb::pageFromString("detail") == ccb::Page::Detail, "types pageFromString");
  CHECK(std::strcmp(ccb::toString(ccb::Page::Mascot), "mascot") == 0, "types toString(page)");

  if (failures == 0) {
    printf("\nALL TESTS PASSED\n");
    return 0;
  }
  printf("\n%d FAILURE(S)\n", failures);
  return 1;
}
