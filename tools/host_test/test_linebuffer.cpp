// Native 单元测试 — ccb::LineBuffer 行缓冲/超限容错（FW-P8-T01/T03）
// LineBuffer 现为 header-only 纯逻辑（无 Arduino），可本地编译。
// 编译：g++ -std=c++17 -Isrc tools/host_test/test_linebuffer.cpp -o /tmp/test_linebuffer

#include "app/SerialTransport.h"

#include <cstdio>
#include <cstring>
#include <string>

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
  LineBuffer lb;
  std::string out;

  // 1. 多行一次喂入
  const char* d1 = "line1\nline2\nline3\n";
  CHECK(lb.feed(d1, std::strlen(d1)) == 3, "feed 3 lines returns 3");
  CHECK(lb.hasNext(), "hasNext after feed");
  CHECK(lb.next(out) && out == "line1", "next line1");
  CHECK(lb.next(out) && out == "line2", "next line2");
  CHECK(lb.next(out) && out == "line3", "next line3");
  CHECK(!lb.hasNext(), "empty after drain");

  // 2. 半行 + 续完
  CHECK(lb.feed("par", 3) == 0, "partial feed 0 lines");
  CHECK(!lb.hasNext(), "partial no complete line");
  CHECK(lb.feed("tial\n", 5) == 1, "complete partial returns 1");
  CHECK(lb.next(out) && out == "partial", "partial completed = partial");

  // 3. CRLF 兼容（\r 忽略）
  lb.feed("crlf\r\n", 6);
  CHECK(lb.next(out) && out == "crlf", "CRLF -> crlf (no \\r)");

  // 4. 空行
  lb.feed("\n", 1);
  CHECK(lb.next(out) && out == "", "empty line enqueued");

  // 5. 超限帧（>1024）→ 不入队 + frameTooLarge 标志 + 保持上一行（FW-P8-T03）
  LineBuffer lb2;
  lb2.feed("good\n", 5);
  std::string big(1100, 'x');  // 1100 字节，无 \n
  lb2.feed(big.c_str(), big.size());
  CHECK(lb2.consumeFrameTooLarge(), "oversize detected");
  CHECK(!lb2.consumeFrameTooLarge(), "flag consumed once (no re-fire)");
  CHECK(lb2.next(out) && out == "good", "prev line retained (frame_too_large keeps last)");
  CHECK(!lb2.hasNext(), "oversize line not enqueued");
  // 超限行需 \n 结束后才恢复
  lb2.feed("\n", 1);  // 结束超限行
  lb2.feed("after\n", 6);
  CHECK(lb2.next(out) && out == "after", "recover after oversize line end");

  // 6. 恰好 1024 不超限，1025 超限
  LineBuffer lb3;
  std::string exact(1024, 'y');
  lb3.feed(exact.c_str(), exact.size());
  lb3.feed("\n", 1);
  CHECK(!lb3.consumeFrameTooLarge(), "1024 bytes not oversize");
  CHECK(lb3.next(out) && out.length() == 1024, "1024-byte line enqueued");

  std::string over(1025, 'z');
  lb3.feed(over.c_str(), over.size());
  lb3.feed("\n", 1);
  CHECK(lb3.consumeFrameTooLarge(), "1025 bytes oversize");

  // 7. 多个超限行（bool 标志：消费一次后复位，不按行计数）
  LineBuffer lb4;
  lb4.feed(big.c_str(), big.size());  // 超限
  lb4.feed("\n", 1);
  lb4.feed(big.c_str(), big.size());  // 再次超限
  lb4.feed("\n", 1);
  CHECK(lb4.consumeFrameTooLarge(), "oversize detected (any since last consume)");
  CHECK(!lb4.consumeFrameTooLarge(), "flag consumed once (no re-fire)");

  if (failures == 0) {
    printf("\nALL TESTS PASSED\n");
    return 0;
  }
  printf("\n%d FAILURE(S)\n", failures);
  return 1;
}
