// claude_code_buddy 固件入口 — M5Stack StickS3
// FW-P0: 闪屏 + USB CDC
// FW-P1: serial transport + 握手/ack/pong
// FW-P2: AppState（device_snapshot/session_snapshot/config 应用）+ 静音持久化
// 详见 docs/claude_code_buddy/plans/firmware-development-plan.md

#include <Arduino.h>
#include <M5Unified.h>
#include <string>

#include "app/AppState.h"
#include "app/Config.h"
#include "app/Protocol.h"
#include "app/SerialTransport.h"
#include "app/Types.h"

static ccb::SerialTransport transport;
static ccb::Protocol protocol;
static ccb::AppState state;

static uint32_t g_lastHelloMs = 0;
static const uint32_t kHelloRetryMs = 1000;  // 未握手则每秒重发 hello（含重连重握手）

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
  // ts_ms 用 device uptime（无 RTC）；echo_ts_ms 回显 ping.ts_ms
  if (protocol.serializePong(buf, sizeof(buf), static_cast<uint32_t>(millis()), echoTsMs))
    transport.sendLine(buf);
}

static void sendError(const char* code, const char* msg = nullptr) {
  char buf[160];
  if (protocol.serializeError(buf, sizeof(buf), code, millis(), msg)) transport.sendLine(buf);
}

static void drawSplash() {
  auto &d = M5.Display;
  d.clear(TFT_BLACK);
  d.setTextDatum(middle_center);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setTextSize(2);
  d.drawString("Claude Code", d.width() / 2, d.height() / 2 - 16);
  d.drawString("Buddy", d.width() / 2, d.height() / 2 + 8);
  d.setTextSize(1);
  d.setTextColor(TFT_CYAN, TFT_BLACK);
  d.drawString(String("fw ") + ccb::FW_VERSION, d.width() / 2, d.height() / 2 + 34);
}

// 屏幕底部状态行（握手状态可视反馈，真机可见）
static void drawStatusLine() {
  auto &d = M5.Display;
  d.setTextDatum(bottom_center);
  d.setTextSize(1);
  if (state.handshakeError) {
    d.setTextColor(TFT_PURPLE, TFT_BLACK);
    d.drawString("USB: err", d.width() / 2, d.height() - 2);
  } else if (state.handshakeDone) {
    d.setTextColor(TFT_GREEN, TFT_BLACK);
    d.drawString("USB: ok", d.width() / 2, d.height() - 2);
  } else {
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.drawString("USB: wait", d.width() / 2, d.height() - 2);
  }
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
      break;
    case ccb::FrameType::DeviceSnapshot:
      state.applyDeviceSnapshot(protocol.doc());
      sendAck(r.seq);
      break;
    case ccb::FrameType::SessionSnapshot:
      state.applySessionSnapshot(protocol.doc());
      sendAck(r.seq);
      break;
    case ccb::FrameType::Alert: {
      // 独立 alert 帧：更新 alert 信息 + ack（音色在 FW-P7）
      const JsonDocument& d = protocol.doc();
      state.alert.present = true;
      state.alert.kind = ccb::alertKindFromString(d["kind"] | "");
      state.alert.sound = d["sound"] | false;
      sendAck(r.seq);
      break;
    }
    case ccb::FrameType::Config:
      state.applyConfig(protocol.doc());
      sendAck(r.seq);
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
  drawSplash();
  drawStatusLine();

  // 启动发 hello（握手），muted 取自持久化状态
  sendHello();
  g_lastHelloMs = millis();
}

void loop() {
  M5.update();

  transport.poll();

  if (transport.consumeFrameTooLarge()) {
    sendError("frame_too_large");
  }

  std::string line;
  while (transport.recvLine(line)) {
    handleLine(line);
  }

  // 未握手则周期重发 hello（含断线重连后重握手，FW-P8-T04）
  if (!state.handshakeDone && !state.handshakeError &&
      millis() - g_lastHelloMs >= kHelloRetryMs) {
    sendHello();
    g_lastHelloMs = millis();
  }

  drawStatusLine();

  delay(2);  // 让出 CPU，保持非阻塞
}
