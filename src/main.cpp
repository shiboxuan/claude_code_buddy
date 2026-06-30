// claude_code_buddy 固件入口 — M5Stack StickS3
// FW-P0: 闪屏 + USB CDC
// FW-P1: serial transport + 握手（hello/hello_ack）、ack、pong，非阻塞主循环
// 详见 docs/claude_code_buddy/plans/firmware-development-plan.md

#include <Arduino.h>
#include <M5Unified.h>
#include <string>

#include "app/Config.h"
#include "app/Protocol.h"
#include "app/SerialTransport.h"

static ccb::SerialTransport transport;
static ccb::Protocol protocol;

// 连接状态（FW-P2 起并入 AppState）
static bool g_handshakeDone = false;
static bool g_handshakeError = false;
static uint32_t g_lastHelloMs = 0;

static const uint32_t kHelloRetryMs = 1000;  // 未握手则每秒重发 hello（含重连重握手）

static void sendHello() {
  char buf[256];
  if (protocol.serializeHello(buf, sizeof(buf), false)) transport.sendLine(buf);
}

static void sendAck(uint32_t seq) {
  char buf[128];
  if (protocol.serializeAck(buf, sizeof(buf), seq, millis())) transport.sendLine(buf);
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
  if (g_handshakeError) {
    d.setTextColor(TFT_PURPLE, TFT_BLACK);
    d.drawString("USB: err", d.width() / 2, d.height() - 2);
  } else if (g_handshakeDone) {
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
      g_handshakeDone = r.ackOk;
      g_handshakeError = !r.ackOk;
      sendAck(r.seq);  // hello_ack 是关键帧（带 seq），回 ack
      break;
    case ccb::FrameType::DeviceSnapshot:
    case ccb::FrameType::SessionSnapshot:
    case ccb::FrameType::Alert:
    case ccb::FrameType::Config:
      sendAck(r.seq);  // FW-P2 起更新 AppState
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
  drawSplash();
  drawStatusLine();

  // 启动发 hello（握手）
  sendHello();
  g_lastHelloMs = millis();
}

void loop() {
  M5.update();

  transport.poll();

  // 超限帧
  if (transport.consumeFrameTooLarge()) {
    sendError("frame_too_large");
  }

  // 处理完整行
  std::string line;
  while (transport.recvLine(line)) {
    handleLine(line);
  }

  // 未握手则周期重发 hello（含断线重连后重握手，FW-P8-T04）
  if (!g_handshakeDone && !g_handshakeError &&
      millis() - g_lastHelloMs >= kHelloRetryMs) {
    sendHello();
    g_lastHelloMs = millis();
  }

  // 状态行仅在变化时重绘（避免每帧刷屏闪烁）
  drawStatusLine();

  delay(2);  // 让出 CPU，保持非阻塞
}
