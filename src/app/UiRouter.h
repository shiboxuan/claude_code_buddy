#pragma once
// UiRouter — 页面路由 + 渲染调度（FW-P3）
// 纯页面逻辑（nextOf/nextPage/backToMascot/setPage）为 header-only，可 native 单测；
// renderCurrentPage 使用 M5.Display（实现见 .cpp，ESP32），故头文件不引入 M5GFX。
// 依据: protocol §5.4、system-design §UI 页面

#include "app/AppState.h"
#include "app/Types.h"

namespace ccb {

// 页面循环顺序：mascot→status→detail→sessions→settings→mascot（error 不在循环内）
inline Page nextOf(Page p) {
  switch (p) {
    case Page::Mascot: return Page::Status;
    case Page::Status: return Page::Detail;
    case Page::Detail: return Page::Sessions;
    case Page::Sessions: return Page::Settings;
    case Page::Settings: return Page::Mascot;
    default: return Page::Mascot;  // Error/Unknown → mascot
  }
}

inline void nextPage(AppState& s) { s.page = nextOf(s.page); }
inline void backToMascot(AppState& s) { s.page = Page::Mascot; }
inline void setPage(AppState& s, Page p) { s.page = p; }

// 渲染当前页（使用 M5.Display，实现见 .cpp）。nowMs 驱动 mascot 动画（FW-P4）。
// detail/sessions/settings/error 占位（FW-P5 补内容）。
void renderCurrentPage(const AppState& s, uint32_t nowMs);

}  // namespace ccb
