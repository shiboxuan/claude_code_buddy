// claude_code_buddy 固件入口 — M5Stack StickS3
// FW-P0: 闪屏 + USB CDC
// FW-P1: serial transport + 握手/ack/pong
// FW-P2: AppState（snapshot/config 应用）+ 静音持久化
// FW-P3: UiRouter 页面路由 + 状态条 + mascot/status 占位页 + 渲染调度（串口不阻塞渲染）
// 详见 docs/claude_code_buddy/plans/firmware-development-plan.md

#include <Arduino.h>
#include <M5Unified.h>
#include <string>

#include "app/AppState.h"
#include "app/AudioController.h"
#include "app/ButtonController.h"
#include "app/Config.h"
#include "app/Protocol.h"
#include "app/SerialTransport.h"
#include "app/Types.h"
#include "app/UiRouter.h"

static ccb::SerialTransport transport;
static ccb::Protocol protocol;
static ccb::AppState state;
static ccb::ButtonController buttons;
static ccb::AudioController audio;

static uint32_t g_lastHelloMs = 0;
static const uint32_t kHelloRetryMs = 1000;  // 未握手则每秒重发 hello

// 渲染调度（FW-P3-T05）：render-on-change + 帧率上限，串口消息不阻塞渲染
static bool g_needRedraw = true;
static uint32_t g_lastRenderMs = 0;
static const uint32_t kFrameIntervalMs = 100;  // 10 FPS（6-12 范围），mascot 动画帧率

static void requestRedraw() { g_needRedraw = true; }

static void sendHello() {
  char buf[256];
  if (protocol.serializeHello(buf, sizeof(buf), state.muted)) transport.sendLine(buf);
}

static void sendAck(uint32_t seq) {
  char buf[128];
  if (protocol.serializeAck(buf, sizeof(buf), seq, millis())) transport.sendLine(buf);
  state.lastSeq = seq;
}

static void sendPong(int64_t echoTsMs) {
  char buf[128];
  if (protocol.serializePong(buf, sizeof(buf), static_cast<uint32_t>(millis()), echoTsMs))
    transport.sendLine(buf);
}

static void sendError(const char* code, const char* msg = nullptr) {
  char buf[160];
  if (protocol.serializeError(buf, sizeof(buf), code, millis(), msg)) transport.sendLine(buf);
}

// FW-P6-T04：非按钮触发的切页上报 page 帧
static void sendPageFrame(ccb::Page prev) {
  char buf[128];
  if (protocol.serializePage(buf, sizeof(buf), state.page, state.muted, millis(), prev))
    transport.sendLine(buf);
}

static void handleLine(const std::string& line) {
  ccb::ParseResult r;
  protocol.parse(line, r);

  if (!r.ok) {
    sendError("json_parse_error");
    return;
  }
  if (!r.protocolOk) {
    sendError("version_mismatch", "protocol mismatch");
    return;
  }
  if (!r.requiredOk) {
    sendError("missing_required_field");
    return;
  }

  switch (r.type) {
    case ccb::FrameType::HelloAck:
      state.applyHelloAck(r.ackOk);
      sendAck(r.seq);
      if (!r.ackOk) {
        state.errorCode = "version_mismatch";
        ccb::Page prev = state.page;
        ccb::setPage(state, ccb::Page::Error);  // 版本不兼容进 error 页
        if (state.page != prev) sendPageFrame(prev);  // FW-P6-T04
      } else if (state.page == ccb::Page::Error) {
        ccb::Page prev = state.page;
        ccb::setPage(state, ccb::Page::Mascot);  // 握手恢复 → 回 mascot
        if (state.page != prev) sendPageFrame(prev);
      }
      requestRedraw();  // 握手态变化 → 状态条更新
      break;
    case ccb::FrameType::DeviceSnapshot:
      state.applyDeviceSnapshot(protocol.doc());
      sendAck(r.seq);
      // FW-P5-T06：done(color=blue) 启动 doneTtl 计时；非 done 取消
      if (state.color == ccb::Color::Blue) state.doneTimer.start(millis());
      else state.doneTimer.cancel();
      // FW-P7：alert 提示音（边沿 + sound + 静音判定）
      if (state.alert.present)
        audio.handleAlert(state.alert.kind,
                          state.focus.present ? state.focus.id.c_str() : nullptr,
                          state.alert.sound, state.muted);
      requestRedraw();
      break;
    case ccb::FrameType::SessionSnapshot:
      state.applySessionSnapshot(protocol.doc());
      sendAck(r.seq);
      requestRedraw();
      break;
    case ccb::FrameType::Alert: {
      const JsonDocument& d = protocol.doc();
      state.alert.present = true;
      state.alert.kind = ccb::alertKindFromString(d["kind"] | "");
      state.alert.sound = d["sound"] | false;
      const char* sid = d["session_id"].is<const char*>() ? d["session_id"].as<const char*>() : nullptr;
      audio.handleAlert(state.alert.kind, sid, state.alert.sound, state.muted);  // FW-P7
      sendAck(r.seq);
      requestRedraw();
      break;
    }
    case ccb::FrameType::Config:
      state.applyConfig(protocol.doc());
      sendAck(r.seq);
      requestRedraw();
      break;
    case ccb::FrameType::Ping:
      sendPong(r.tsMs);
      break;
    default:
      sendError("unknown_message_type", r.typeStr.c_str());
      break;
  }
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  transport.begin();
  state.loadMuted();  // FW-P2-T04：从 NVS 恢复静音
  audio.begin();      // FW-P7-T05：音量保守上限

  // 首帧渲染 mascot 页（状态条显示 USB: wait）
  ccb::renderCurrentPage(state, millis());
  g_lastRenderMs = millis();
  g_needRedraw = false;

  sendHello();
  g_lastHelloMs = millis();
}

void loop() {
  M5.update();

  // 串口读（非阻塞）
  transport.poll();
  if (transport.consumeFrameTooLarge()) sendError("frame_too_large");
  std::string line;
  while (transport.recvLine(line)) handleLine(line);

  // 按钮（FW-P6 ButtonController：短/长按检测 + 动作 + 上报 button 帧；FR-013 仅本地+上报）
  if (buttons.poll(millis(), state, protocol, transport)) requestRedraw();

  // 渲染调度：render-on-change + 帧率驱动 mascot 动画，串口消息不阻塞渲染
  bool frameDue = (millis() - g_lastRenderMs >= kFrameIntervalMs);
  if (g_needRedraw || frameDue) {
    ccb::renderCurrentPage(state, millis());
    g_needRedraw = false;
    g_lastRenderMs = millis();
  }

  // FW-P5-T06：done 超时(doneTtlMs，默认 5s)本地回 idle（BR-007）
  {
    uint32_t ttl = state.config.hasDoneTtl ? state.config.doneTtlMs : 5000;
    if (state.doneTimer.expired(millis(), ttl)) {
      state.globalState = ccb::GlobalState::Idle;
      state.color = ccb::Color::Green;
      requestRedraw();
    }
  }

  // 未握手则周期重发 hello（含断线重连后重握手，FW-P8-T04）
  if (!state.handshakeDone && !state.handshakeError &&
      millis() - g_lastHelloMs >= kHelloRetryMs) {
    sendHello();
    g_lastHelloMs = millis();
  }

  delay(2);  // 让出 CPU，保持非阻塞
}
