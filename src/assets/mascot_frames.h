#pragma once
// mascot_frames — Claude Code 小怪物调色板 + 站立基准几何（首页重设计 v2）
// 参考：Claude Code 官方 "Welcome, Claw'd" 像素小怪物 —— 方块躯干 + 两个小方块黑眼 +
//       眼睛高度左右各伸出的小方块短臂 + 底部三条往下伸的短腿。
// 绘制层（MascotRenderer.cpp）用这些常量程序化绘制，可整体缩放 / 位移 / 挤压拉伸，
// 便于做 idle 蹦跳/偷看/灵感 + working(戴耳机)/attention/error/done/sleep 各态动画。
// 颜色恒为橙色（不随状态变）；状态警示交给背景脉冲（见 Theme::mascotBg）。

#include <stdint.h>

namespace ccb {
namespace mascot {

// —— 调色板（HEX，绘制时经 theme::hexToRgb565 转 565）——
constexpr uint32_t HEX_BODY     = 0xD97757;  // 主体黏土珊瑚橙（Claude 官方 mascot 橙）
constexpr uint32_t HEX_BODY_HI  = 0xE79B7F;  // 顶部高光/亮面
constexpr uint32_t HEX_BODY_SH  = 0xB0512F;  // 底部暗面/腿缝阴影
constexpr uint32_t HEX_DARK     = 0x241109;  // 眼睛/嘴 近黑
constexpr uint32_t HEX_ACCENT   = 0xF5E9E1;  // 高光米白（Zzz / ! / 火花 / 键帽亮）
constexpr uint32_t HEX_KEY      = 0x3A4046;  // 键盘暗键
constexpr uint32_t HEX_DROP     = 0x6FB7E8;  // 汗滴/泪滴蓝

// —— 耳机（Working；参考戴耳机的社区形象，蓝色）——
constexpr uint32_t HEX_PHONE    = 0x2F6DB4;  // 耳机头梁/耳罩 蓝
constexpr uint32_t HEX_PHONE_SH = 0x1E4C82;  // 耳罩暗面

// —— 灯泡（idle 灵感彩蛋）——
constexpr uint32_t HEX_BULB     = 0xFFD24D;  // 灯泡玻璃暖黄
constexpr uint32_t HEX_BULB_HI  = 0xFFF0B4;  // 灯泡高光
constexpr uint32_t HEX_BULB_BASE= 0x9AA0A6;  // 灯座灰

// —— 站立基准几何（px；screen 135×240 竖屏）——
// 轮廓自上而下：躯干(BODY_H) + 腿(LEG_H)。GROUND_Y 为腿底基线。
constexpr int BODY_W    = 50;   // 躯干宽（收窄，别为填满屏幕拉胖，保持官方瘦长比例）
constexpr int BODY_H    = 44;   // 躯干高（不含腿）
constexpr int LEG_W     = 9;    // 单腿宽
constexpr int LEG_H     = 12;   // 腿高
constexpr int ARM_W     = 8;    // 侧短臂外伸宽度（小 nub，不外扩成一坨）
constexpr int ARM_H     = 14;   // 侧短臂高
constexpr int EYE       = 11;   // 方眼边长
constexpr int EYE_GAP   = 12;   // 两眼间距
constexpr int BODY_FULL_H = BODY_H + LEG_H;  // 整体轮廓高（供上方 ! / Zzz / 灯泡定位）
constexpr int GROUND_Y  = 168;  // 腿底基线（上方留白给 ! / Zzz / 灯泡，四周留白给背景脉冲）

}  // namespace mascot
}  // namespace ccb
