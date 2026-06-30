#pragma once
// MascotRenderer — M5GFX 基础几何 mascot 绘制与三态动画（FW-P4）
// frameIndex 为纯逻辑（可 native 测试）；render 使用 M5.Display（.cpp，ESP32）。
// 依据: protocol §5.2、system-design §Mascot 动画

#include <stdint.h>

#include "app/Types.h"

namespace ccb {

class MascotRenderer {
 public:
  // 当前动画帧索引（按 fps 循环 4 帧）。纯逻辑，header-only 可 native 单测。
  static uint8_t frameIndex(GlobalState state, uint32_t nowMs, uint8_t fps = 10) {
    (void)state;
    if (fps == 0) fps = 10;
    const uint8_t kFrames = 4;       // idle/working/attention 各 4 帧
    uint32_t period = 1000u / fps;   // ms/帧
    if (period == 0) period = 1;
    return static_cast<uint8_t>((nowMs / period) % kFrames);
  }
  // 渲染 mascot 到 M5.Display（几何：身体圆 + 眼睛 + 三态点缀，颜色随状态）。
  static void render(GlobalState state, uint32_t nowMs);
};

}  // namespace ccb
