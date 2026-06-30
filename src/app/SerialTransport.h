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
#include <utility>

#include "app/Config.h"

namespace ccb {

// 纯 C++ 行缓冲（header-only，无 Arduino 依赖，可 native 单测）：
// 累积字节、按 '\n' 切行；单行超过 FRAME_MAX_BYTES 标记超限并丢弃该行（保持上一行）。
class LineBuffer {
 public:
  // 喂入若干字节（可能含多行或半行）。返回新增完整行数。
  size_t feed(const char* data, size_t len) {
    size_t newLines = 0;
    for (size_t i = 0; i < len; ++i) {
      char c = data[i];
      if (c == '\n') {
        if (discardUntilNl_) {
          discardUntilNl_ = false;  // 超限行结尾，丢弃并恢复
          cur_.clear();
        } else {
          lines_.push(std::move(cur_));
          cur_.clear();
          ++newLines;
        }
      } else if (c == '\r') {
        continue;  // 忽略 CR（兼容 CRLF）
      } else {
        if (discardUntilNl_) continue;  // 超限行内部字节，丢弃
        cur_.push_back(c);
        if (cur_.size() > FRAME_MAX_BYTES) {
          frameTooLarge_ = true;  // 单行超限：标记、丢弃、忽略到行尾
          discardUntilNl_ = true;
          cur_.clear();
        }
      }
    }
    return newLines;
  }
  bool hasNext() const { return !lines_.empty(); }
  bool next(std::string& out) {
    if (lines_.empty()) return false;
    out = std::move(lines_.front());
    lines_.pop();
    return true;
  }
  bool consumeFrameTooLarge() {
    bool f = frameTooLarge_;
    frameTooLarge_ = false;
    return f;
  }

 private:
  std::string cur_;
  std::queue<std::string> lines_;
  bool frameTooLarge_ = false;
  bool discardUntilNl_ = false;
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
