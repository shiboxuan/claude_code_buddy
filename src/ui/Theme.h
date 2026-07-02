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

// —— 首页背景呼吸脉冲（小怪物恒橙，警示只体现在背景）——
// 把 HEX 按亮度百分比压暗（0..100）。纯整数运算。
inline uint32_t dimHex(uint32_t hex, uint8_t pct) {
  uint32_t r = ((hex >> 16) & 0xFF) * pct / 100;
  uint32_t g = ((hex >> 8) & 0xFF) * pct / 100;
  uint32_t b = (hex & 0xFF) * pct / 100;
  return (r << 16) | (g << 8) | b;
}

// 三角波呼吸：nowMs 在 periodMs 内从 minPct 线性升到 maxPct 再降回。整数运算，无浮点。
inline uint8_t triPulse(uint32_t nowMs, uint32_t periodMs, uint8_t minPct, uint8_t maxPct) {
  if (periodMs == 0) periodMs = 1000;
  uint32_t t = nowMs % periodMs;
  uint32_t half = periodMs / 2;
  if (half == 0) half = 1;
  uint32_t up = (t < half) ? t : (periodMs - t);  // 0..half
  return static_cast<uint8_t>(minPct + (maxPct - minPct) * up / half);
}

// 首页背景色：按权威 Color 返回暗色调呼吸脉冲背景。
//   Green(idle)  → 纯黑，无脉冲（安静）
//   Red(working) → 慢节奏呼吸    Yellow(attention) → 快节奏呼吸
//   Purple(error)→ 急促呼吸      Blue(done/conn)   → 柔和呼吸
inline uint16_t mascotBg(Color c, uint32_t nowMs) {
  uint32_t base;
  uint8_t pct;
  switch (c) {
    case Color::Red:      base = HEX_RED;    pct = triPulse(nowMs, 2600, 10, 34); break;
    case Color::RedFlash: base = HEX_RED;    pct = triPulse(nowMs,  700, 16, 46); break;
    case Color::Yellow:   base = HEX_YELLOW; pct = triPulse(nowMs,  900, 12, 38); break;
    case Color::Purple:   base = HEX_PURPLE; pct = triPulse(nowMs,  750, 14, 44); break;
    case Color::Blue:     base = HEX_BLUE;   pct = triPulse(nowMs, 2000, 10, 30); break;
    default:              return 0x0000;  // Green / Gray / Unknown → 黑，无脉冲
  }
  return hexToRgb565(dimHex(base, pct));
}

}  // namespace theme
}  // namespace ccb
