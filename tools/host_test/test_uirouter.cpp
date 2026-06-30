// Native 单元测试 — ccb::UiRouter 纯页面路由逻辑（FW-P3-T01）
// nextOf/nextPage/backToMascot/setPage 为 header-only（无 M5GFX），可本地编译。
// 编译：
//   g++ -std=c++17 -Isrc -I.pio/libdeps/m5stack-sticks3/ArduinoJson/src \
//       tools/host_test/test_uirouter.cpp -o /tmp/test_uirouter

#include "app/UiRouter.h"

#include <cstdio>

static int failures = 0;
#define CHECK(cond, msg)                                     \
  do {                                                       \
    if (!(cond)) {                                           \
      printf("  FAIL: %s\n", msg);                           \
      ++failures;                                            \
    } else {                                                 \
      printf("  pass: %s\n", msg);                           \
    }                                                        \
  } while (0)

int main() {
  using namespace ccb;

  // nextOf 循环顺序
  CHECK(nextOf(Page::Mascot) == Page::Status, "nextOf mascot->status");
  CHECK(nextOf(Page::Status) == Page::Detail, "nextOf status->detail");
  CHECK(nextOf(Page::Detail) == Page::Sessions, "nextOf detail->sessions");
  CHECK(nextOf(Page::Sessions) == Page::Settings, "nextOf sessions->settings");
  CHECK(nextOf(Page::Settings) == Page::Mascot, "nextOf settings->mascot (wrap)");
  CHECK(nextOf(Page::Error) == Page::Mascot, "nextOf error->mascot");

  // nextPage 改 AppState.page
  AppState s;
  s.page = Page::Mascot;
  nextPage(s);
  CHECK(s.page == Page::Status, "nextPage mascot->status");
  nextPage(s);  // detail
  nextPage(s);  // sessions
  nextPage(s);  // settings
  nextPage(s);  // wrap -> mascot
  CHECK(s.page == Page::Mascot, "nextPage cycle back to mascot");

  // backToMascot
  s.page = Page::Detail;
  backToMascot(s);
  CHECK(s.page == Page::Mascot, "backToMascot");

  // setPage
  setPage(s, Page::Sessions);
  CHECK(s.page == Page::Sessions, "setPage sessions");

  // 完整一轮 5 步回到 mascot
  s.page = Page::Mascot;
  int steps = 0;
  do { nextPage(s); ++steps; } while (s.page != Page::Mascot && steps < 10);
  CHECK(s.page == Page::Mascot && steps == 5, "full cycle = 5 steps");

  if (failures == 0) {
    printf("\nALL TESTS PASSED\n");
    return 0;
  }
  printf("\n%d FAILURE(S)\n", failures);
  return 1;
}
