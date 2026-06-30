#pragma once
// AppState — 设备运行态（FW-P2-T01）
// 存储 global_state / focus_session / counts / alert / detail / config / muted / page / 握手态。
// FW-P2-T02：apply* 方法从解析后的 JSON doc 更新字段；FW-P2-T04：muted 持久化 NVS；
// FW-P2-T05：config.privacy_mode 字段通路（MVP 不实装隐藏，仅存储）。
// 依据: protocol §4.5.2/§4.5.3/§4.5.5

#include <ArduinoJson.h>
#include <string>

#include "app/Types.h"

namespace ccb {

// device_snapshot.focus_session（§4.5.2）
struct FocusSession {
  bool present = false;
  std::string id, label, repo, cwd, title, line1, line2;
  SessionState state = SessionState::Unknown;
  bool hasProgress = false;
  float progress = 0.0f;
};

struct Counts {
  int sessions = 0;
  int working = 0;
  int attention = 0;
  int error = 0;
};

struct AlertInfo {
  bool present = false;
  AlertKind kind = AlertKind::Unknown;
  bool sound = false;
};

// session_snapshot.session（§4.5.3）→ detail 页数据
struct DetailSession {
  bool present = false;
  std::string idShort, repo, cwdLabel, title, line1, line2;
  SessionState state = SessionState::Unknown;
  Color color = Color::Unknown;
  bool hasProgress = false;
  float progress = 0.0f;
  int ageSec = 0;
};

struct ConfigValues {
  bool hasSoundEnabled = false;
  bool soundEnabled = true;
  bool hasPrivacyMode = false;  // FW-P2-T05 扩展位
  bool privacyMode = false;
  bool hasBrightness = false;
  int brightness = 60;
  bool hasDoneTtl = false;
  int doneTtlMs = 5000;
};

class AppState {
 public:
  GlobalState globalState = GlobalState::DeviceDisconnected;
  Color color = Color::Gray;
  Counts counts;
  FocusSession focus;
  AlertInfo alert;
  DetailSession detail;
  ConfigValues config;
  bool muted = false;
  Page page = Page::Mascot;
  bool handshakeDone = false;
  bool handshakeError = false;
  uint32_t lastSeq = 0;

  // 帧应用（FW-P2-T02）：从解析后的 JSON doc 读取并更新对应字段
  void applyDeviceSnapshot(const JsonDocument& doc);
  void applySessionSnapshot(const JsonDocument& doc);
  void applyConfig(const JsonDocument& doc);
  void applyHelloAck(bool ok);

  // 静音持久化（NVS，FW-P2-T04 / FR-019）
  void loadMuted();
  void saveMuted();
  void setMuted(bool m);

  // 重置为断线态（FW-P8 用，此处先提供）
  void resetConnection();
};

}  // namespace ccb
