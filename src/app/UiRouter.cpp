// UiRouter 实现 — FW-P3：状态条 + mascot/status 占位页渲染（M5GFX）
#include "UiRouter.h"

#include <M5Unified.h>

#include "app/Config.h"
#include "ui/MascotRenderer.h"
#include "ui/Theme.h"

namespace ccb {

namespace {
Color stateColor(const AppState& s) {
  return (s.color != Color::Unknown) ? s.color : theme::colorFor(s.globalState);
}

void renderStatusbar(const AppState& s) {
  auto& d = M5.Display;
  uint16_t col = theme::rgb565(stateColor(s));
  d.fillRect(0, 0, theme::SCREEN_W, theme::STATUS_BAR_H, col);
  d.setTextDatum(top_left);
  d.setTextColor(TFT_WHITE, col);
  d.setTextSize(1);
  d.drawString(toString(s.page), 2, 3);
  d.setTextDatum(top_right);
  d.drawString(String(s.counts.sessions), theme::SCREEN_W - 2, 3);
}

void renderMascotPage(const AppState& s, uint32_t nowMs) {
  auto& d = M5.Display;
  d.clear(TFT_BLACK);
  renderStatusbar(s);
  MascotRenderer::render(s.globalState, nowMs);  // 三态动画 mascot
  // 品牌底部
  d.setTextDatum(bottom_center);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setTextSize(1);
  d.drawString("Claude Code Buddy", theme::SCREEN_W / 2, theme::SCREEN_H - 12);
  d.setTextColor(TFT_CYAN, TFT_BLACK);
  d.drawString(String("fw ") + FW_VERSION, theme::SCREEN_W / 2, theme::SCREEN_H - 2);
}

void renderStatusPage(const AppState& s) {
  auto& d = M5.Display;
  d.clear(TFT_BLACK);
  renderStatusbar(s);
  int y = theme::STATUS_BAR_H + 4;
  d.setTextDatum(top_left);
  d.setTextColor(theme::rgb565(stateColor(s)), TFT_BLACK);
  d.setTextSize(2);
  d.drawString(toString(s.globalState), 2, y);
  y += 26;
  d.setTextSize(1);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.drawString(s.focus.present ? s.focus.repo.c_str() : "(no session)", 2, y);
  y += 14;
  d.drawString(s.focus.present ? s.focus.line1.c_str() : "", 2, y);
  y += 12;
  d.drawString(s.focus.present ? s.focus.line2.c_str() : "", 2, y);
}

void renderPlaceholderPage(const AppState& s, const char* name) {
  auto& d = M5.Display;
  d.clear(TFT_BLACK);
  renderStatusbar(s);
  d.setTextDatum(middle_center);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setTextSize(1);
  d.drawString(String(name) + " (FW-P5)", theme::SCREEN_W / 2, theme::SCREEN_H / 2);
}
}  // namespace

void renderCurrentPage(const AppState& s, uint32_t nowMs) {
  switch (s.page) {
    case Page::Mascot: renderMascotPage(s, nowMs); break;
    case Page::Status: renderStatusPage(s); break;
    case Page::Detail: renderPlaceholderPage(s, "detail"); break;
    case Page::Sessions: renderPlaceholderPage(s, "sessions"); break;
    case Page::Settings: renderPlaceholderPage(s, "settings"); break;
    case Page::Error: renderPlaceholderPage(s, "error"); break;
    default: renderMascotPage(s, nowMs); break;
  }
}

}  // namespace ccb
