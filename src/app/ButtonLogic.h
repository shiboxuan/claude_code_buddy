#pragma once
// ButtonLogic — 短按/长按检测纯状态机（FW-P6-T01）
// 无 Arduino 依赖，header-only，可 native 单测。ButtonController 在其上包 M5Unified。
// 长按在按住达到阈值时触发一次；短按在释放时（未达阈值）触发。

#include <stdint.h>

namespace ccb {

enum class ButtonEvent : uint8_t { None, ShortPress, LongPress };

class ButtonLogic {
 public:
  // 喂入当前按下状态 + 时间(ms) + 长按阈值，返回本次事件
  ButtonEvent update(bool pressed, uint32_t now, uint32_t longMs) {
    ButtonEvent ev = ButtonEvent::None;
    if (pressed) {
      if (state_ == Idle) {
        state_ = Pressed;
        pressStart_ = now;
        longFired_ = false;
      } else if (state_ == Pressed && !longFired_ && (now - pressStart_ >= longMs)) {
        longFired_ = true;
        ev = ButtonEvent::LongPress;
      }
    } else {
      if (state_ == Pressed) {
        if (!longFired_) ev = ButtonEvent::ShortPress;
        state_ = Idle;
        longFired_ = false;
      }
    }
    return ev;
  }

 private:
  enum State : uint8_t { Idle, Pressed };
  State state_ = Idle;
  uint32_t pressStart_ = 0;
  bool longFired_ = false;
};

}  // namespace ccb
