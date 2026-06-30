// Native 单元测试 — ccb::MascotRenderer::frameIndex 动画帧逻辑（FW-P4-T05）
// frameIndex 为纯逻辑（无 M5GFX），可本地编译（仅需 Types.h）。
// 编译：g++ -std=c++17 -Isrc tools/host_test/test_mascot.cpp -o /tmp/test_mascot

#include "ui/MascotRenderer.h"

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

  // fps=10 → period=100ms，4 帧循环
  CHECK(MascotRenderer::frameIndex(GlobalState::Idle, 0) == 0, "f@0=0");
  CHECK(MascotRenderer::frameIndex(GlobalState::Idle, 99) == 0, "f@99=0");
  CHECK(MascotRenderer::frameIndex(GlobalState::Idle, 100) == 1, "f@100=1");
  CHECK(MascotRenderer::frameIndex(GlobalState::Idle, 200) == 2, "f@200=2");
  CHECK(MascotRenderer::frameIndex(GlobalState::Idle, 300) == 3, "f@300=3");
  CHECK(MascotRenderer::frameIndex(GlobalState::Idle, 400) == 0, "f@400=0 wrap");
  CHECK(MascotRenderer::frameIndex(GlobalState::Idle, 999) == 1, "f@999=1 (999/100=9, 9%4=1)");

  // 不同状态共用同一时间轴
  CHECK(MascotRenderer::frameIndex(GlobalState::Working, 100) == 1, "working f@100=1");
  CHECK(MascotRenderer::frameIndex(GlobalState::Attention, 300) == 3, "attention f@300=3");

  // fps 参数：fps=5 → period=200ms
  CHECK(MascotRenderer::frameIndex(GlobalState::Idle, 0, 5) == 0, "fps5 f@0=0");
  CHECK(MascotRenderer::frameIndex(GlobalState::Idle, 199, 5) == 0, "fps5 f@199=0");
  CHECK(MascotRenderer::frameIndex(GlobalState::Idle, 200, 5) == 1, "fps5 f@200=1");
  CHECK(MascotRenderer::frameIndex(GlobalState::Idle, 800, 5) == 0, "fps5 f@800=0 wrap");

  // 帧始终在 0..3
  bool inRange = true;
  for (uint32_t t = 0; t < 5000; t += 37) {
    uint8_t f = MascotRenderer::frameIndex(GlobalState::Idle, t);
    if (f > 3) { inRange = false; break; }
  }
  CHECK(inRange, "frames always 0..3 over time");

  if (failures == 0) {
    printf("\nALL TESTS PASSED\n");
    return 0;
  }
  printf("\n%d FAILURE(S)\n", failures);
  return 1;
}
