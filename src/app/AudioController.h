#pragma once
// AudioController — 状态提示音 + 静音 + 边沿触发（FW-P7）
// 边沿逻辑见 AlertEdge.h（可 native 测试）；本类用 M5.Speaker 发声（.cpp，ESP32）。
// 依据: protocol §5.8、system-design §声音策略 / BR-009 / BR-010

#include "app/AlertEdge.h"
#include "app/Types.h"

namespace ccb {

class AudioController {
 public:
  // 音量保守上限（FW-P7-T05）
  void begin();
  // 处理 alert：边沿(BR-009) + sound + 静音(BR-010) 判定后播放对应音色。返回是否实际播放。
  bool handleAlert(AlertKind kind, const char* sessionId, bool sound, bool muted);

 private:
  AlertEdge edge_;
  void playTone(AlertKind kind);
};

}  // namespace ccb
