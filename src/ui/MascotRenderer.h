#pragma once
// MascotRenderer — Claude 橙色像素小怪物：idle 蹦跳/偷看 + 事件态动画（首页重设计）
// frameIndex 为纯逻辑（保留供 native 测试）；render 为有状态动画引擎（.cpp，ESP32）。
// 小怪物恒为橙色，状态警示交给背景脉冲（见 theme::mascotBg）。
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
  // 渲染小怪物（有状态动画引擎，内部按 nowMs 步进位置/挤压/表情）。
  // color 为权威状态色：Idle+Blue 视为 done(开心跳)，其余按 state 决定动画模式。
  static void render(GlobalState state, Color color, uint32_t nowMs);
};

}  // namespace ccb
