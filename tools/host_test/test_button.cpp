// Native 单元测试 — ccb::ButtonLogic 短/长按状态机（FW-P6-T01）
// ButtonLogic 为 header-only 纯逻辑（无 Arduino），可本地编译。
// 编译：g++ -std=c++17 -Isrc tools/host_test/test_button.cpp -o /tmp/test_button

#include "app/ButtonLogic.h"

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
  const uint32_t L = 600;

  // 空闲
  ButtonLogic b;
  CHECK(b.update(false, 0, L) == ButtonEvent::None, "idle none");

  // 短按：按下→持握未达阈值→释放 = ShortPress
  CHECK(b.update(true, 100, L) == ButtonEvent::None, "press @100 none");
  CHECK(b.update(true, 500, L) == ButtonEvent::None, "hold @500 none (<L)");
  CHECK(b.update(false, 550, L) == ButtonEvent::ShortPress, "release @550 short");
  CHECK(b.update(false, 600, L) == ButtonEvent::None, "idle after release");

  // 长按：按下→持握达阈值 = LongPress（一次），之后继续持握不再触发；释放不再 Short
  CHECK(b.update(true, 1000, L) == ButtonEvent::None, "press @1000");
  CHECK(b.update(true, 1599, L) == ButtonEvent::None, "hold @1599 none");
  CHECK(b.update(true, 1600, L) == ButtonEvent::LongPress, "hold @1600 long");
  CHECK(b.update(true, 1700, L) == ButtonEvent::None, "continued hold no re-fire");
  CHECK(b.update(false, 1800, L) == ButtonEvent::None, "release after long -> none");

  // 边界：恰好 now-start == L → LongPress
  ButtonLogic b2;
  CHECK(b2.update(true, 0, L) == ButtonEvent::None, "b2 press @0");
  CHECK(b2.update(true, L, L) == ButtonEvent::LongPress, "b2 @L long (boundary)");

  // 边界：L-1 释放 → ShortPress
  ButtonLogic b3;
  b3.update(true, 0, L);
  CHECK(b3.update(false, L - 1, L) == ButtonEvent::ShortPress, "b3 release @L-1 short");

  // 连续短按两次
  ButtonLogic b4;
  b4.update(true, 0, L);
  CHECK(b4.update(false, 100, L) == ButtonEvent::ShortPress, "b4 first short");
  b4.update(true, 200, L);
  CHECK(b4.update(false, 300, L) == ButtonEvent::ShortPress, "b4 second short");

  // 抖动：按下后短暂释放再按下（视为短按 + 新按下）
  ButtonLogic b5;
  b5.update(true, 0, L);
  CHECK(b5.update(false, 50, L) == ButtonEvent::ShortPress, "b5 quick release short");
  CHECK(b5.update(true, 60, L) == ButtonEvent::None, "b5 re-press none");
  CHECK(b5.update(true, 60 + L, L) == ButtonEvent::LongPress, "b5 long after re-press");

  if (failures == 0) {
    printf("\nALL TESTS PASSED\n");
    return 0;
  }
  printf("\n%d FAILURE(S)\n", failures);
  return 1;
}
