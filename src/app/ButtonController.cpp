// ButtonController 实现 — FW-P6
#include "ButtonController.h"

#include <Arduino.h>
#include <M5Unified.h>

#include "app/UiRouter.h"  // nextPage / backToMascot

namespace ccb {

namespace {
constexpr uint32_t kLongPressMs = 600;  // 长按阈值(ms)，待 FW-P9 实测校准
}

bool ButtonController::poll(uint32_t now, AppState& s, Protocol& p, SerialTransport& t) {
  bool redraw = false;
  ButtonEvent ea = a_.update(M5.BtnA.isPressed(), now, kLongPressMs);
  ButtonEvent eb = b_.update(M5.BtnB.isPressed(), now, kLongPressMs);

  auto sendButton = [&](const char* btn, const char* act) {
    char buf[160];
    if (p.serializeButton(buf, sizeof(buf), btn, act, s.page, s.muted, millis()))
      t.sendLine(buf);
  };

  // A：短按下一页 / 长按回 mascot
  if (ea == ButtonEvent::ShortPress) {
    nextPage(s);
    sendButton("A", "short_press");
    redraw = true;
  } else if (ea == ButtonEvent::LongPress) {
    backToMascot(s);
    sendButton("A", "long_press");
    redraw = true;
  }

  // B：长按重发 hello（重握手）。B 短按静音见 FW-P7。
  if (eb == ButtonEvent::LongPress) {
    char buf[256];
    if (p.serializeHello(buf, sizeof(buf), s.muted)) t.sendLine(buf);
    sendButton("B", "long_press");
    redraw = true;
  }

  return redraw;
}

}  // namespace ccb
