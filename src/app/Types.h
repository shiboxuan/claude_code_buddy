#pragma once
// Types — ccb-serial-v1 协议枚举与字符串转换（header-only）
// FW-P2: AppState / Theme / UI 共用的状态 / 颜色 / 页面 / 提示枚举。
// 依据: protocol §5.1(session_state) / §5.2(global_state) / §5.3(color) / §5.4(page) / §5.8(alert kind)

#include <cstdint>

namespace ccb {

enum class GlobalState : uint8_t {
  DeviceDisconnected, AdapterConnected, Idle, Working, Attention, Error, Unknown
};

enum class SessionState : uint8_t {
  Unknown, Idle, Working, Attention, Plan, DoneRecent, Error, Ended
};

enum class Color : uint8_t {
  Green, Red, Yellow, Blue, Purple, RedFlash, Gray, Unknown
};

enum class AlertKind : uint8_t {
  Connected, Attention, Done, Error, Unknown
};

enum class Page : uint8_t {
  Mascot, Status, Detail, Sessions, Settings, Error, Unknown
};

inline GlobalState globalStateFromString(const char* s) {
  if (!s) return GlobalState::Unknown;
  if (strcmp(s, "device_disconnected") == 0) return GlobalState::DeviceDisconnected;
  if (strcmp(s, "adapter_connected") == 0) return GlobalState::AdapterConnected;
  if (strcmp(s, "idle") == 0) return GlobalState::Idle;
  if (strcmp(s, "working") == 0) return GlobalState::Working;
  if (strcmp(s, "attention") == 0) return GlobalState::Attention;
  if (strcmp(s, "error") == 0) return GlobalState::Error;
  return GlobalState::Unknown;
}

inline SessionState sessionStateFromString(const char* s) {
  if (!s) return SessionState::Unknown;
  if (strcmp(s, "unknown") == 0) return SessionState::Unknown;
  if (strcmp(s, "idle") == 0) return SessionState::Idle;
  if (strcmp(s, "working") == 0) return SessionState::Working;
  if (strcmp(s, "attention") == 0) return SessionState::Attention;
  if (strcmp(s, "plan") == 0) return SessionState::Plan;
  if (strcmp(s, "done_recent") == 0) return SessionState::DoneRecent;
  if (strcmp(s, "error") == 0) return SessionState::Error;
  if (strcmp(s, "ended") == 0) return SessionState::Ended;
  return SessionState::Unknown;
}

inline Color colorFromString(const char* s) {
  if (!s) return Color::Unknown;
  if (strcmp(s, "green") == 0) return Color::Green;
  if (strcmp(s, "red") == 0) return Color::Red;
  if (strcmp(s, "yellow") == 0) return Color::Yellow;
  if (strcmp(s, "blue") == 0) return Color::Blue;
  if (strcmp(s, "purple") == 0) return Color::Purple;
  if (strcmp(s, "red_flash") == 0) return Color::RedFlash;
  if (strcmp(s, "gray") == 0) return Color::Gray;
  return Color::Unknown;
}

inline AlertKind alertKindFromString(const char* s) {
  if (!s) return AlertKind::Unknown;
  if (strcmp(s, "connected") == 0) return AlertKind::Connected;
  if (strcmp(s, "attention") == 0) return AlertKind::Attention;
  if (strcmp(s, "done") == 0) return AlertKind::Done;
  if (strcmp(s, "error") == 0) return AlertKind::Error;
  return AlertKind::Unknown;
}

inline Page pageFromString(const char* s) {
  if (!s) return Page::Unknown;
  if (strcmp(s, "mascot") == 0) return Page::Mascot;
  if (strcmp(s, "status") == 0) return Page::Status;
  if (strcmp(s, "detail") == 0) return Page::Detail;
  if (strcmp(s, "sessions") == 0) return Page::Sessions;
  if (strcmp(s, "settings") == 0) return Page::Settings;
  if (strcmp(s, "error") == 0) return Page::Error;
  return Page::Unknown;
}

inline const char* toString(Page p) {
  switch (p) {
    case Page::Mascot: return "mascot";
    case Page::Status: return "status";
    case Page::Detail: return "detail";
    case Page::Sessions: return "sessions";
    case Page::Settings: return "settings";
    case Page::Error: return "error";
    default: return "unknown";
  }
}

inline const char* toString(GlobalState s) {
  switch (s) {
    case GlobalState::DeviceDisconnected: return "disc";
    case GlobalState::AdapterConnected: return "conn";
    case GlobalState::Idle: return "idle";
    case GlobalState::Working: return "work";
    case GlobalState::Attention: return "attn";
    case GlobalState::Error: return "error";
    default: return "unknown";
  }
}

}  // namespace ccb
