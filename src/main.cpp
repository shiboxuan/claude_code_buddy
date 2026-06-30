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
#include "app/Config.h"
#include "app/Protocol.h"
#include "app/SerialTransport.h"
#include "app/Types.h"
#include "app/UiRouter.h"

static ccb::SerialTransport transport;
static ccb::Protocol protocol;
static ccb::AppState state;

static uint32_t g_lastHelloMs = 0;
static const uint32_t kHelloRetryMs = 1000;  // 未握手则每秒重发 hello

// 渲染调度（FW-P3-T05）：render-on-change + 帧率上限，串口消息不阻塞渲染
static bool g_needRedraw = true;
static uint32_t g_lastRenderMs = 0;
static const uint32_t kFrameIntervalMs = 50;  // ~20fps 渲染上限（FW-P4 动画将复用）

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
      requestRedraw();  // 握手态变化 → 状态条更新
      break;
    case ccb::FrameType::DeviceSnapshot:
      state.applyDeviceSnapshot(protocol.doc());
      sendAck(r.seq);
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

  // 首帧渲染 mascot 页（状态条显示 USB: wait）
  ccb::renderCurrentPage(state);
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

  // 按钮切页（FW-P3 骨架；短/长按检测与上报见 FW-P6 ButtonController）
  if (M5.BtnA.wasPressed()) {
    ccb::nextPage(state);
    requestRedraw();
  }
  if (M5.BtnB.wasPressed()) {
    ccb::backToMascot(state);
    requestRedraw();
  }

  // 渲染调度：render-on-change + 帧率上限，串口消息不阻塞渲染
  if (g_needRedraw && millis() - g_lastRenderMs >= kFrameIntervalMs) {
    ccb::renderCurrentPage(state);
    g_needRedraw = false;
    g_lastRenderMs = millis();
  }

  // 未握手则周期重发 hello（含断线重连后重握手，FW-P8-T04）
  if (!state.handshakeDone && !state.handshakeError &&
      millis() - g_lastHelloMs >= kHelloRetryMs) {
    sendHello();
    g_lastHelloMs = millis();
  }

  delay(2);  // 让出 CPU，保持非阻塞
}
