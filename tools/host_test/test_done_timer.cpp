// Native 单元测试 — ccb::DoneTimer done 超时逻辑（FW-P5-T06 / BR-007）
// DoneTimer 为 AppState.h 内 header-only 纯逻辑，可本地编译。
// 编译：
//   g++ -std=c++17 -Isrc -I.pio/libdeps/m5stack-sticks3/ArduinoJson/src \
//       tools/host_test/test_done_timer.cpp -o /tmp/test_done_timer

#include "app/AppState.h"

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
  ccb::DoneTimer t;

  // 未启动不超时
  CHECK(!t.expired(9999, 5000), "not started -> not expired");

  // 启动后未到 ttl 不超时
  t.start(100);
  CHECK(t.active, "active after start");
  CHECK(!t.expired(5099, 5000), "not expired @5099 (start=100, ttl=5000)");

  // 到 ttl 超时，且消费后变 inactive
  CHECK(t.expired(5100, 5000), "expired @5100");
  CHECK(!t.active, "inactive after expire");

  // 超时后不再重复触发
  CHECK(!t.expired(9999, 5000), "no re-expire after consumed");

  // cancel
  t.start(0);
  t.cancel();
  CHECK(!t.active, "cancelled -> inactive");
  CHECK(!t.expired(5000, 5000), "cancelled -> not expired");

  // 重新 start 可再次工作
  t.start(200);
  CHECK(!t.expired(200, 5000), "restarted, not expired @200");
  CHECK(t.expired(5200, 5000), "restarted, expired @5200");

  // 边界：恰好 ttl
  t.start(0);
  CHECK(!t.expired(4999, 5000), "boundary not expired @4999");
  CHECK(t.expired(5000, 5000), "boundary expired @5000");

  if (failures == 0) {
    printf("\nALL TESTS PASSED\n");
    return 0;
  }
  printf("\n%d FAILURE(S)\n", failures);
  return 1;
}
