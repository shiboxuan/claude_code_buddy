// MascotRenderer 实现 — FW-P4：基础几何 mascot + idle/working/attention 三态动画
#include "MascotRenderer.h"

#include <M5Unified.h>

#include "ui/FrameBuffer.h"
#include "ui/Theme.h"

namespace ccb {

void MascotRenderer::render(GlobalState state, uint32_t nowMs) {
  auto& d = ccb::fb();
  uint8_t f = frameIndex(state, nowMs);
  uint16_t col = theme::rgb565(theme::colorFor(state));
  int cx = theme::SCREEN_W / 2;
  int cy = theme::STATUS_BAR_H + 46;
  int r = 18;
  int yoff = 0;

  switch (state) {
    case GlobalState::Idle:
      r = 18 + (f == 1 ? 2 : (f == 3 ? -2 : 0));  // 呼吸
      break;
    case GlobalState::Attention:
      yoff = (f % 2 == 0) ? -4 : 0;  // 跳动
      break;
    case GlobalState::Working:
    default:
      break;  // 静态身体
  }

  // 身体
  d.fillCircle(cx, cy + yoff, r, col);
  // 眼睛（idle f==2 眨眼）
  if (state == GlobalState::Idle && f == 2) {
    d.drawLine(cx - 7, cy + yoff - 4, cx - 3, cy + yoff - 4, TFT_WHITE);
    d.drawLine(cx + 3, cy + yoff - 4, cx + 7, cy + yoff - 4, TFT_WHITE);
  } else {
    d.fillCircle(cx - 5, cy + yoff - 4, 2, TFT_WHITE);
    d.fillCircle(cx + 5, cy + yoff - 4, 2, TFT_WHITE);
  }

  // 三态点缀
  if (state == GlobalState::Working) {
    // 键盘：3 方块，高亮随帧
    for (int i = 0; i < 3; ++i) {
      int kx = cx - 13 + i * 10;
      uint16_t kc = (i == f % 3) ? TFT_WHITE : theme::rgb565(Color::Gray);
      d.drawRect(kx, cy + 24, 8, 6, kc);
    }
  } else if (state == GlobalState::Attention) {
    // "!" 上方
    d.setTextDatum(bottom_center);
    d.setTextColor(col, TFT_BLACK);
    d.setTextSize(2);
    d.drawString("!", cx, cy + yoff - r - 1);
  }
}

}  // namespace ccb
