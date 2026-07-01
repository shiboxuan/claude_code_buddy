// FrameBuffer 实现 — 全屏 sprite 双缓冲
#include "FrameBuffer.h"

#include <M5Unified.h>

#include "ui/Theme.h"

namespace ccb {

static lgfx::LGFX_Sprite* g_fb = nullptr;

void initFrameBuffer() {
  if (g_fb) return;
  g_fb = new lgfx::LGFX_Sprite(&M5.Display);
  g_fb->setPsram(false);  // 用内部 RAM（63KB，无 PSRAM 亦可）
  g_fb->createSprite(theme::SCREEN_W, theme::SCREEN_H);
}

lgfx::LGFX_Sprite& fb() { return *g_fb; }

}  // namespace ccb
