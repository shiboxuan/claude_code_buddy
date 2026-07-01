#pragma once
// FrameBuffer — 全屏 sprite 双缓冲，消除 ST7789 全屏 clear+重绘频闪（FW-P9 调优）
// initFrameBuffer() 须在 M5.begin 后、首次渲染前调用；fb() 返回 sprite 引用供渲染层绘制。
// 详见 system-design §小屏幕布局原则

#include <M5GFX.h>  // lgfx::LGFX_Sprite 完整类型

namespace ccb {

void initFrameBuffer();           // 创建 135×240 sprite（内部 RAM）
lgfx::LGFX_Sprite& fb();          // 取 sprite 引用（渲染层画到它，renderCurrentPage 末尾 pushSprite）

}  // namespace ccb
