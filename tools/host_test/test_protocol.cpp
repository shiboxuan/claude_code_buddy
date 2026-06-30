// Native 单元测试 — ccb::Protocol 解析/序列化逻辑（FW-P1-T02/T03/T04/T05）
// 无需硬件/Arduino：Protocol.cpp 为纯 C++，链接 ArduinoJson 头文件即可本地编译。
// 编译：
//   g++ -std=c++17 -Isrc -I.pio/libdeps/m5stack-sticks3/ArduinoJson/src \
//       tools/host_test/test_protocol.cpp src/app/Protocol.cpp -o /tmp/test_proto
// 运行：/tmp/test_proto

#include "app/Protocol.h"

#include <cstdio>
#include <cstring>
#include <string>

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
  ccb::Protocol p;
  ccb::ParseResult r;

  // 1. hello_ack ok=true（握手成功）
  p.parse(R"({"type":"hello_ack","protocol":"ccb-serial-v1","adapter_version":"0.1.0","seq":1,"ok":true})", r);
  CHECK(r.ok, "hello_ack json ok");
  CHECK(r.protocolOk, "hello_ack protocol ok");
  CHECK(r.type == ccb::FrameType::HelloAck, "hello_ack type");
  CHECK(r.requiredOk, "hello_ack required ok");
  CHECK(r.ackOk, "hello_ack ackOk=true");
  CHECK(r.seq == 1, "hello_ack seq=1");
  CHECK(r.adapterVersion == "0.1.0", "hello_ack adapter_version");

  // 2. hello_ack ok=false（版本不兼容 → 握手失败）
  p.parse(R"({"type":"hello_ack","protocol":"ccb-serial-v1","adapter_version":"0.1.0","seq":1,"ok":false})", r);
  CHECK(r.ok && r.type == ccb::FrameType::HelloAck && !r.ackOk, "hello_ack ok=false");

  // 3. device_snapshot 完整
  p.parse(R"({"type":"device_snapshot","protocol":"ccb-serial-v1","seq":2,"global_state":"working","color":"red","counts":{"sessions":2,"working":1,"attention":0,"error":0},"focus_session":null,"alert":null})", r);
  CHECK(r.type == ccb::FrameType::DeviceSnapshot && r.seq == 2 && r.requiredOk, "device_snapshot parse");

  // 3b. device_snapshot 缺 counts → requiredOk=false
  p.parse(R"({"type":"device_snapshot","protocol":"ccb-serial-v1","seq":2,"global_state":"working","color":"red"})", r);
  CHECK(r.ok && !r.requiredOk, "device_snapshot missing counts -> requiredOk=false");

  // 4. session_snapshot
  p.parse(R"({"type":"session_snapshot","protocol":"ccb-serial-v1","seq":3,"session":{"session_id_short":"abc123","state":"working","color":"red"}})", r);
  CHECK(r.type == ccb::FrameType::SessionSnapshot && r.seq == 3 && r.requiredOk, "session_snapshot parse");

  // 5. alert
  p.parse(R"({"type":"alert","protocol":"ccb-serial-v1","seq":4,"kind":"attention","sound":true,"session_id":"abc123"})", r);
  CHECK(r.type == ccb::FrameType::Alert && r.seq == 4 && r.requiredOk, "alert parse");

  // 6. config
  p.parse(R"({"type":"config","protocol":"ccb-serial-v1","seq":5,"sound_enabled":true,"brightness":60,"done_ttl_ms":5000})", r);
  CHECK(r.type == ccb::FrameType::Config && r.seq == 5 && r.requiredOk, "config parse");

  // 7. ping（epoch ms，大整数 int64）
  p.parse(R"({"type":"ping","protocol":"ccb-serial-v1","ts_ms":1782837733976})", r);
  CHECK(r.type == ccb::FrameType::Ping, "ping type");
  CHECK(r.tsMs == 1782837733976LL, "ping ts_ms int64");
  CHECK(r.requiredOk, "ping required ok");

  // 8. 未知 type
  p.parse(R"({"type":"mystery","protocol":"ccb-serial-v1"})", r);
  CHECK(r.ok && r.protocolOk && r.type == ccb::FrameType::Unknown, "unknown type -> Unknown");

  // 9. 坏 JSON
  p.parse("not json at all", r);
  CHECK(!r.ok && r.error == "json_parse_error", "bad json -> json_parse_error");

  // 10. 空行
  p.parse("", r);
  CHECK(!r.ok, "empty line -> not ok");

  // 11. 协议版本不匹配
  p.parse(R"({"type":"ping","protocol":"ccb-serial-v2","ts_ms":1})", r);
  CHECK(r.ok && !r.protocolOk, "wrong protocol -> protocolOk=false");

  // 12. hello 序列化 + 字段
  char buf[256];
  CHECK(p.serializeHello(buf, sizeof(buf), false), "serializeHello");
  CHECK(strstr(buf, "\"device\":\"m5stick-s3\""), "hello device field");
  CHECK(strstr(buf, "\"fw_version\":\"0.1.0\""), "hello fw_version field");
  CHECK(strstr(buf, "\"muted\":false"), "hello muted field");
  CHECK(strstr(buf, "lcd") && strstr(buf, "speaker") && strstr(buf, "buttons"), "hello features");
  // 回环：序列化的 hello 能被 parse 识别
  p.parse(buf, r);
  CHECK(r.type == ccb::FrameType::Hello && r.protocolOk, "hello round-trip");

  // 13. ack 序列化 + 回环
  CHECK(p.serializeAck(buf, sizeof(buf), 7, 12345), "serializeAck");
  p.parse(buf, r);
  CHECK(r.type == ccb::FrameType::Ack && r.seq == 7, "ack round-trip seq=7");

  // 14. pong 序列化 + echo_ts_ms
  CHECK(p.serializePong(buf, sizeof(buf), 999, 1782837733976LL), "serializePong");
  CHECK(strstr(buf, "\"echo_ts_ms\":1782837733976"), "pong echo_ts_ms field");
  p.parse(buf, r);
  CHECK(r.type == ccb::FrameType::Pong, "pong round-trip type");

  // 15. error 序列化
  CHECK(p.serializeError(buf, sizeof(buf), "frame_too_large", 100, "msg"), "serializeError");
  CHECK(strstr(buf, "\"code\":\"frame_too_large\""), "error code field");
  p.parse(buf, r);
  CHECK(r.type == ccb::FrameType::Error, "error round-trip type");

  if (failures == 0) {
    printf("\nALL TESTS PASSED\n");
    return 0;
  }
  printf("\n%d FAILURE(S)\n", failures);
  return 1;
}
