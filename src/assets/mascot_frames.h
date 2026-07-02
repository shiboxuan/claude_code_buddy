#pragma once
// mascot_frames — Claude Code 小怪物调色板 + 站立基准几何（首页重设计）
// 参考：Claude Code 官方橙色像素小怪物（方块身体 + 深色竖眼 + 三条短腿缝）。
// 绘制层（MascotRenderer.cpp）用这些常量程序化绘制，可整体缩放 / 位移 / 挤压拉伸，
// 便于做 idle 蹦跳/偷看 + working/attention/error/done/sleep 各态动画。
// 颜色恒为橙色（不随状态变）；状态警示交给背景脉冲（见 Theme::mascotBg）。

#include <stdint.h>

namespace ccb {
namespace mascot {

// —— 调色板（HEX，绘制时经 theme::hexToRgb565 转 565）——
constexpr uint32_t HEX_BODY     = 0xC15F3C;  // 主体黏土橙
constexpr uint32_t HEX_BODY_HI  = 0xDA7A50;  // 顶部高光/亮面
constexpr uint32_t HEX_BODY_SH  = 0x8F4227;  // 底部暗面/腿缝阴影
constexpr uint32_t HEX_DARK     = 0x2B1A12;  // 眼睛/嘴 近黑棕
constexpr uint32_t HEX_ACCENT   = 0xF5E9E1;  // 高光米白（Zzz / ! / 火花 / 键帽亮）
constexpr uint32_t HEX_KEY      = 0x3A4046;  // 键盘暗键
constexpr uint32_t HEX_DROP     = 0x6FB7E8;  // 汗滴/泪滴蓝

// —— 站立基准几何（px；GROUND_Y 为脚底基线，screen 135×240 竖屏）——
constexpr int BODY_W   = 62;   // 身体宽
constexpr int BODY_H   = 54;   // 身体高
constexpr int GROUND_Y = 168;  // 脚底基线（上方留白给 ! / Zzz，四周留白给背景脉冲）

}  // namespace mascot
}  // namespace ccb
