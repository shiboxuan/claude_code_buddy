#pragma once
// SerialTransport — USB CDC 串口传输层
// FW-P1-T01：按行读 JSON Lines（'\n' 结束）、非阻塞、坏行/超限帧处理。
// 依据: protocol §4.1（传输层：USB CDC、UTF-8、JSON Lines、'\n'、单帧 ≤1024 bytes、无 BOM）
//
// 设计：LineBuffer 为纯 C++（无 Arduino 依赖），便于后续 host 端单元测试；
//       SerialTransport 在其上包一层 USB CDC I/O。

#include <stddef.h>
#include <stdint.h>
#include <queue>
#include <string>

#include "app/Config.h"

namespace ccb {

// 纯 C++ 行缓冲：累积字节、按 '\n' 切行；单行超过 FRAME_MAX_BYTES 标记超限并丢弃该行。
class LineBuffer {
 public:
  // 喂入若干字节（可能含多行或半行）。返回新增完整行数。
  size_t feed(const char* data, size_t len);
  // 是否有完整行可取
  bool hasNext() const { return !lines_.empty(); }
  // 取下一行（不含 '\n'）；无行返回 false
  bool next(std::string& out);
  // 消费「超限行」标志（出现过则返回 true 并复位）
  bool consumeFrameTooLarge();

 private:
  std::string cur_;                    // 当前未完成行
  std::queue<std::string> lines_;      // 已完成行队列
  bool frameTooLarge_ = false;         // 自上次消费以来出现过超限行
  bool discardUntilNl_ = false;        // 当前超限行，丢弃到下一个 '\n'
};

// USB CDC 串口传输：非阻塞按行读、按行写。
class SerialTransport {
 public:
  void begin();
  // 非阻塞：从 Serial 读可用字节喂入 LineBuffer
  void poll();
  // 取下一完整行（不含 '\n'）
  bool recvLine(std::string& out) { return line_.next(out); }
  // 写一行（自动补 '\n'）
  void sendLine(const char* s);
  void sendLine(const std::string& s) { sendLine(s.c_str()); }
  // 消费「超限行」标志
  bool consumeFrameTooLarge() { return line_.consumeFrameTooLarge(); }

 private:
  LineBuffer line_;
};

}  // namespace ccb
