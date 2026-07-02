// UiRouter 实现 — FW-P3/P5：状态条 + 6 页渲染（M5GFX）
#include "UiRouter.h"

#include <M5Unified.h>

#include "app/Config.h"
#include "ui/FrameBuffer.h"
#include "ui/MascotRenderer.h"
#include "ui/Theme.h"

namespace ccb {

namespace {
Color stateColor(const AppState& s) {
  return (s.color != Color::Unknown) ? s.color : theme::colorFor(s.globalState);
}

void renderStatusbar(const AppState& s) {
  auto& d = ccb::fb();
  uint16_t col = theme::rgb565(stateColor(s));
  d.fillRect(0, 0, theme::SCREEN_W, theme::STATUS_BAR_H, col);
  d.setTextDatum(top_left);
  d.setTextColor(TFT_WHITE, col);
  d.setTextSize(1);
  d.drawString(toString(s.page), 2, 3);
  d.setTextDatum(top_right);
  d.drawString(String(s.counts.sessions), theme::SCREEN_W - 2, 3);
}

// UTF-8 感知的像素宽度截断（FR-018 长文本不溢出）
String truncateToWidth(const char* s, int maxW) {
  auto& d = ccb::fb();
  if (!s || !*s) return String("");
  String str(s);
  if (d.textWidth(str) <= maxW) return str;
  int ellW = d.textWidth("…");
  String out;
  int i = 0;
  while (i < str.length()) {
    uint8_t c = static_cast<uint8_t>(str[i]);
    int clen = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
    if (i + clen > str.length()) clen = 1;
    out += str.substring(i, i + clen);
    i += clen;
    if (d.textWidth(out) > maxW - ellW) {
      int backTo = out.length() - clen;
      if (backTo > 0) out.remove(backTo);
      out += "…";
      break;
    }
  }
  return out;
}

void renderMascotPage(const AppState& s, uint32_t nowMs) {
  auto& d = ccb::fb();
  // 首页彻底产品化：全屏 = 背景呼吸脉冲 + 橙色小怪物。
  // 无状态条、无文字；警示只体现在背景（idle 绿→黑无脉冲，其余状态色呼吸）。
  d.clear(theme::mascotBg(stateColor(s), nowMs));
  MascotRenderer::render(s.globalState, stateColor(s), nowMs);
}

void renderStatusPage(const AppState& s) {
  auto& d = ccb::fb();
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
  if (s.focus.present) {
    d.drawString(truncateToWidth(s.focus.repo.c_str(), theme::SCREEN_W - 4), 2, y);
    y += 14;
    d.drawString(truncateToWidth(s.focus.line1.c_str(), theme::SCREEN_W - 4), 2, y);
    y += 12;
    d.drawString(truncateToWidth(s.focus.line2.c_str(), theme::SCREEN_W - 4), 2, y);
  } else {
    d.drawString("(no session)", 2, y);
  }
}

void renderDetailPage(const AppState& s) {
  auto& d = ccb::fb();
  d.clear(TFT_BLACK);
  renderStatusbar(s);
  int y = theme::STATUS_BAR_H + 4;
  d.setTextDatum(top_left);
  d.setTextSize(1);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  if (s.detail.present) {
    d.drawString(truncateToWidth(s.detail.repo.c_str(), theme::SCREEN_W - 4), 2, y); y += 12;
    d.drawString(truncateToWidth(s.detail.cwdLabel.c_str(), theme::SCREEN_W - 4), 2, y); y += 12;
    d.setTextColor(theme::rgb565(s.detail.color != Color::Unknown ? s.detail.color : Color::Gray), TFT_BLACK);
    d.drawString(toString(s.detail.state), 2, y); y += 12;
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.drawString(truncateToWidth(s.detail.title.c_str(), theme::SCREEN_W - 4), 2, y); y += 12;
    d.drawString(truncateToWidth(s.detail.line1.c_str(), theme::SCREEN_W - 4), 2, y); y += 12;
    d.drawString(truncateToWidth(s.detail.line2.c_str(), theme::SCREEN_W - 4), 2, y); y += 12;
    if (s.detail.hasProgress) {
      int bw = theme::SCREEN_W - 4;
      d.drawRect(2, y, bw, 8, TFT_WHITE);
      int pw = static_cast<int>(bw * s.detail.progress / 100.0f);
      if (pw > 0)
        d.fillRect(2, y, pw, 8, theme::rgb565(s.detail.color != Color::Unknown ? s.detail.color : Color::Green));
    }
  } else {
    d.drawString("(no detail session)", 2, y);
  }
}

void renderSessionListPage(const AppState& s) {
  auto& d = ccb::fb();
  d.clear(TFT_BLACK);
  renderStatusbar(s);
  int y = theme::STATUS_BAR_H + 4;
  d.setTextDatum(top_left);
  d.setTextSize(1);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.drawString("sessions: " + String(s.counts.sessions), 2, y);
  y += 14;
  d.fillCircle(5, y + 4, 3, theme::rgb565(Color::Red));
  d.drawString("work:" + String(s.counts.working), 14, y); y += 12;
  d.fillCircle(5, y + 4, 3, theme::rgb565(Color::Yellow));
  d.drawString("attn:" + String(s.counts.attention), 14, y); y += 12;
  d.fillCircle(5, y + 4, 3, theme::rgb565(Color::Purple));
  d.drawString("err:" + String(s.counts.error), 14, y); y += 14;
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.drawString("focus:", 2, y); y += 12;
  if (s.focus.present) {
    d.fillCircle(5, y + 4, 3, theme::rgb565(stateColor(s)));
    d.drawString(truncateToWidth(s.focus.repo.c_str(), theme::SCREEN_W - 16), 14, y);
  } else {
    d.drawString("-", 14, y);
  }
}

void renderSettingsPage(const AppState& s) {
  auto& d = ccb::fb();
  d.clear(TFT_BLACK);
  renderStatusbar(s);
  int y = theme::STATUS_BAR_H + 4;
  d.setTextDatum(top_left);
  d.setTextSize(1);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.drawString("mute: " + String(s.muted ? "on" : "off"), 2, y); y += 12;
  d.drawString("proto: " + String(PROTOCOL_VERSION), 2, y); y += 12;
  const char* adp = s.handshakeDone ? "conn" : (s.handshakeError ? "err" : "wait");
  d.drawString("adapter: " + String(adp), 2, y); y += 12;
  d.drawString("fw: " + String(FW_VERSION), 2, y); y += 12;
  if (s.config.hasBrightness) {
    d.drawString("bright: " + String(s.config.brightness), 2, y); y += 12;
  }
}

void renderErrorPage(const AppState& s) {
  auto& d = ccb::fb();
  d.clear(TFT_BLACK);
  renderStatusbar(s);
  d.setTextDatum(middle_center);
  d.setTextColor(theme::rgb565(Color::Purple), TFT_BLACK);
  d.setTextSize(2);
  d.drawString("ERROR", theme::SCREEN_W / 2, theme::SCREEN_H / 2 - 14);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setTextSize(1);
  d.drawString(s.errorCode.empty() ? "unknown" : s.errorCode.c_str(), theme::SCREEN_W / 2, theme::SCREEN_H / 2 + 10);
}
}  // namespace

void renderCurrentPage(const AppState& s, uint32_t nowMs) {
  switch (s.page) {
    case Page::Mascot: renderMascotPage(s, nowMs); break;
    case Page::Status: renderStatusPage(s); break;
    case Page::Detail: renderDetailPage(s); break;
    case Page::Sessions: renderSessionListPage(s); break;
    case Page::Settings: renderSettingsPage(s); break;
    case Page::Error: renderErrorPage(s); break;
    default: renderMascotPage(s, nowMs); break;
  }
  ccb::fb().pushSprite(0, 0);  // 双缓冲：离屏绘制后一次性推屏，消除频闪
}

}  // namespace ccb
