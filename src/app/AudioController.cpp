// AudioController 实现 — FW-P7
#include "AudioController.h"

#include <M5Unified.h>

namespace ccb {

namespace {
constexpr uint8_t kVolume = 200;  // 音量(~78%; 电池供电建议 ≤191(75%), USB 供电可更高, FW-P9 调优)
}

void AudioController::begin() {
  M5.Speaker.setVolume(kVolume);
}

bool AudioController::handleAlert(AlertKind kind, const char* sessionId, bool sound, bool muted) {
  bool isNew = edge_.isNewEdge(kind, sessionId);  // BR-009 边沿（无论是否发声都更新）
  bool played = isNew && sound && !muted;         // BR-010 静音抑制声音；sound=false 不发声
  if (played) playTone(kind);
  return played;
}

void AudioController::playTone(AlertKind kind) {
  switch (kind) {
    case AlertKind::Attention:                                      // 两声短提示
      M5.Speaker.tone(880, 80);
      delay(100);
      M5.Speaker.tone(880, 80);
      break;
    case AlertKind::Done:      M5.Speaker.tone(660, 80);  break;   // 一声轻提示
    case AlertKind::Error:     M5.Speaker.tone(220, 200); break;   // 低频错误
    case AlertKind::Connected: M5.Speaker.tone(990, 60);  break;   // 短上行音
    default: break;
  }
}

}  // namespace ccb
