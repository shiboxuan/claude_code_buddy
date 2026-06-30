#pragma once
// ButtonController — A/B 按钮短/长按检测 + 动作派发 + 上报（FW-P6）
// A 短按=下一页 / A 长按=回 mascot / B 长按=重发 hello(重握手)；上报 button 帧。
// （B 短按=静音见 FW-P7）。按钮仅本地+上报，无 Claude Code 控制通路（FR-013）。
// 纯状态机见 ButtonLogic.h（可 native 测试）；本头含 Arduino 依赖，不宜 native 测试。

#include "app/AppState.h"
#include "app/ButtonLogic.h"
#include "app/Protocol.h"
#include "app/SerialTransport.h"

namespace ccb {

class ButtonController {
 public:
  // 轮询按钮：执行动作 + 上报 button 帧。返回 true 表示有 UI 变化需重绘。
  bool poll(uint32_t now, AppState& s, Protocol& p, SerialTransport& t);

 private:
  ButtonLogic a_, b_;
};

}  // namespace ccb
