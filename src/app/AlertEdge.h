#pragma once
// AlertEdge — alert 边沿触发判定（FW-P7-T04 / BR-009）
// 纯逻辑：相同 (kind, sessionId) 不重复；kind 或 session 变化视为新边沿。
// header-only，可 native 单测。

#include <string>

#include "app/Types.h"

namespace ccb {

class AlertEdge {
 public:
  // 是否为新边沿（与上次 kind+session 不同）。若是则更新内部状态。
  bool isNewEdge(AlertKind kind, const char* sessionId) {
    bool isNew = (kind != lastKind_) || diffSession(sessionId);
    if (isNew) {
      lastKind_ = kind;
      lastSession_ = (sessionId ? sessionId : "");
    }
    return isNew;
  }
  void reset() {
    lastKind_ = AlertKind::Unknown;
    lastSession_.clear();
  }

 private:
  bool diffSession(const char* s) const {
    if (s == nullptr || s[0] == '\0') return !lastSession_.empty();
    return lastSession_ != s;
  }
  AlertKind lastKind_ = AlertKind::Unknown;
  std::string lastSession_;
};

}  // namespace ccb
