// Native 单元测试 — ccb::AlertEdge 边沿触发（FW-P7-T04 / BR-009）
// AlertEdge 为 header-only 纯逻辑（无 Arduino），可本地编译。
// 编译：g++ -std=c++17 -Isrc tools/host_test/test_alert.cpp -o /tmp/test_alert

#include "app/AlertEdge.h"

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

  AlertEdge e;
  // 首次 = 新边沿
  CHECK(e.isNewEdge(AlertKind::Attention, "s1"), "first attention/s1 new");
  // 相同 kind+session 不重复（BR-009）
  CHECK(!e.isNewEdge(AlertKind::Attention, "s1"), "repeat attention/s1 not new");
  CHECK(!e.isNewEdge(AlertKind::Attention, "s1"), "repeat again not new");
  // 不同 session = 新
  CHECK(e.isNewEdge(AlertKind::Attention, "s2"), "attention/s2 new");
  // 不同 kind = 新
  CHECK(e.isNewEdge(AlertKind::Done, "s2"), "done/s2 new");
  // 回到 attention/s1（上次 done/s2）= 新
  CHECK(e.isNewEdge(AlertKind::Attention, "s1"), "back attention/s1 new");
  CHECK(!e.isNewEdge(AlertKind::Attention, "s1"), "repeat not new");
  // error 边沿
  CHECK(e.isNewEdge(AlertKind::Error, "s1"), "error/s1 new");
  CHECK(!e.isNewEdge(AlertKind::Error, "s1"), "repeat error not new");

  // null/空 session
  AlertEdge e2;
  CHECK(e2.isNewEdge(AlertKind::Attention, nullptr), "null session new");
  CHECK(!e2.isNewEdge(AlertKind::Attention, nullptr), "repeat null not new");
  CHECK(!e2.isNewEdge(AlertKind::Attention, ""), "empty == null, not new");  // 空等价 null，仍同一边沿
  CHECK(e2.isNewEdge(AlertKind::Attention, "x"), "null->x new");
  CHECK(e2.isNewEdge(AlertKind::Attention, nullptr), "x->null new");

  // reset
  e2.reset();
  CHECK(e2.isNewEdge(AlertKind::Attention, "x"), "after reset new");

  // kind 切换往返
  AlertEdge e3;
  CHECK(e3.isNewEdge(AlertKind::Attention, "s"), "attn new");
  CHECK(e3.isNewEdge(AlertKind::Done, "s"), "done new (kind change, same session)");
  CHECK(e3.isNewEdge(AlertKind::Attention, "s"), "attn new again (kind back)");

  if (failures == 0) {
    printf("\nALL TESTS PASSED\n");
    return 0;
  }
  printf("\n%d FAILURE(S)\n", failures);
  return 1;
}
