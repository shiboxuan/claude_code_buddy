#pragma once
// Theme — 颜色 / 字体 / 布局常量（仅头文件，FW-P2-T03）
// 依据: protocol §5.3（颜色 HEX）、system-design §小屏幕布局原则
//
// 屏幕 135×240 竖屏；状态条 12-18px；字体层级 title 16-20 / body 10-14 / metadata 8-10。

#include <stdint.h>

#include "app/Types.h"

namespace ccb {
namespace theme {

// 屏幕（135×240 竖屏）
constexpr int SCREEN_W = 135;
constexpr int SCREEN_H = 240;
constexpr int STATUS_BAR_H = 16;  // 12-18px 范围

// 字体大小
constexpr int FONT_TITLE = 18;    // 16-20
constexpr int FONT_BODY = 12;     // 10-14
constexpr int FONT_METADATA = 9;  // 8-10

// 颜色 HEX（protocol §5.3）
constexpr uint32_t HEX_GREEN = 0x20C997;
constexpr uint32_t HEX_RED = 0xFF4D4F;
constexpr uint32_t HEX_YELLOW = 0xFFD43B;
constexpr uint32_t HEX_BLUE = 0x4DABF7;
constexpr uint32_t HEX_PURPLE = 0xAE3EC9;
constexpr uint32_t HEX_RED_FLASH = 0xE03131;
constexpr uint32_t HEX_GRAY = 0x868E96;

// HEX -> RGB565
inline uint16_t hexToRgb565(uint32_t hex) {
  uint8_t r = (hex >> 16) & 0xFF;
  uint8_t g = (hex >> 8) & 0xFF;
  uint8_t b = hex & 0xFF;
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Color 枚举 -> RGB565
inline uint16_t rgb565(Color c) {
  switch (c) {
    case Color::Green: return hexToRgb565(HEX_GREEN);
    case Color::Red: return hexToRgb565(HEX_RED);
    case Color::Yellow: return hexToRgb565(HEX_YELLOW);
    case Color::Blue: return hexToRgb565(HEX_BLUE);
    case Color::Purple: return hexToRgb565(HEX_PURPLE);
    case Color::RedFlash: return hexToRgb565(HEX_RED_FLASH);
    case Color::Gray: return hexToRgb565(HEX_GRAY);
    default: return hexToRgb565(HEX_GRAY);
  }
}

// global_state -> Color（状态条颜色；device_snapshot 的 color 字段为权威，此为按状态推导的兜底）
inline Color colorFor(GlobalState s) {
  switch (s) {
    case GlobalState::Idle: return Color::Green;
    case GlobalState::Working: return Color::Red;
    case GlobalState::Attention: return Color::Yellow;
    case GlobalState::Error: return Color::Purple;
    case GlobalState::AdapterConnected: return Color::Blue;
    default: return Color::Gray;
  }
}

inline uint16_t rgb565For(GlobalState s) { return rgb565(colorFor(s)); }

}  // namespace theme
}  // namespace ccb
