#pragma once
// Protocol — ccb-serial-v1 帧解析 / 校验 / 序列化
// FW-P1-T02：解析 type/protocol 字段；反序列化 host→device 帧；校验 protocol == ccb-serial-v1。
// FW-P1-T03/T04/T05：握手 hello/hello_ack、ack、pong 由本模块序列化，由 main 派发。
// 依据: protocol §4.2/§4.3/§4.4/§4.5/§4.6

#include <ArduinoJson.h>
#include <stdint.h>
#include <string>

#include "app/Config.h"
#include "app/Types.h"

namespace ccb {

enum class FrameType : uint8_t {
  Unknown = 0,
  // host → device
  HelloAck,
  DeviceSnapshot,
  SessionSnapshot,
  Alert,
  Config,
  Ping,
  // device → host（序列化用；host 不应发这些）
  Hello,
  Ack,
  Pong,
  Error,
  Button,
  Mute,
  Page,
};

// 单行解析结果（仅元数据；完整 JSON 保留在 Protocol 内部 doc_，供 FW-P2 AppState 取用）
struct ParseResult {
  bool ok = false;             // JSON 解析成功
  bool protocolOk = false;     // protocol == ccb-serial-v1
  bool requiredOk = true;      // 该类型必填字段齐全
  FrameType type = FrameType::Unknown;
  std::string typeStr;         // 原始 type 字符串（未知/上报时用）
  std::string error;           // 解析失败原因（ok=false 时填 json_parse_error 等）
  uint32_t seq = 0;            // 关键帧 seq
  bool ackOk = false;          // hello_ack.ok
  std::string adapterVersion;  // hello_ack.adapter_version
  int64_t tsMs = 0;            // ping.ts_ms（epoch ms）
};

class Protocol {
 public:
  // 解析一行（不含 '\n'）
  void parse(const std::string& line, ParseResult& out);
  // 访问最近一次解析的完整 JSON doc（供 AppState 取字段，FW-P2）
  const JsonDocument& doc() const { return doc_; }

  // 序列化 device → host 帧到 buf（不含 '\n'，由 SerialTransport 补）。返回是否成功。
  bool serializeHello(char* buf, size_t size, bool muted);
  bool serializeAck(char* buf, size_t size, uint32_t seq, uint32_t uptimeMs);
  // pong.ts_ms：device 无 RTC，用 uptime_ms 作 device 侧时间（待确认项，见计划）；
  // echo_ts_ms 回显 ping.ts_ms（epoch ms）。
  bool serializePong(char* buf, size_t size, uint32_t tsMs, int64_t echoTsMs);
  bool serializeError(char* buf, size_t size, const char* code, uint32_t uptimeMs,
                      const char* msg = nullptr);
  // FW-P6：button/page 上行帧
  bool serializeButton(char* buf, size_t size, const char* button, const char* action,
                       Page page, bool muted, uint32_t uptimeMs);
  bool serializePage(char* buf, size_t size, Page page, bool muted, uint32_t uptimeMs,
                     Page prevPage = Page::Unknown);
  bool serializeMute(char* buf, size_t size, bool muted, uint32_t uptimeMs);  // FW-P7

 private:
  JsonDocument doc_;  // 复用，避免反复堆分配
};

}  // namespace ccb
