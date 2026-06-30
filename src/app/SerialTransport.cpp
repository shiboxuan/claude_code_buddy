// SerialTransport 实现 — FW-P1-T01
#include "SerialTransport.h"

#include <Arduino.h>

namespace ccb {

size_t LineBuffer::feed(const char* data, size_t len) {
  size_t newLines = 0;
  for (size_t i = 0; i < len; ++i) {
    char c = data[i];
    if (c == '\n') {
      if (discardUntilNl_) {
        // 超限行的结尾，丢弃并恢复
        discardUntilNl_ = false;
        cur_.clear();
      } else {
        lines_.push(std::move(cur_));
        cur_.clear();
        ++newLines;
      }
    } else if (c == '\r') {
      // 忽略 CR（兼容 CRLF）
      continue;
    } else {
      if (discardUntilNl_) continue;  // 超限行内部字节，丢弃
      cur_.push_back(c);
      if (cur_.size() > FRAME_MAX_BYTES) {
        // 单行超限：标记、丢弃当前行，并忽略到行尾
        frameTooLarge_ = true;
        discardUntilNl_ = true;
        cur_.clear();
      }
    }
  }
  return newLines;
}

bool LineBuffer::next(std::string& out) {
  if (lines_.empty()) return false;
  out = std::move(lines_.front());
  lines_.pop();
  return true;
}

bool LineBuffer::consumeFrameTooLarge() {
  bool f = frameTooLarge_;
  frameTooLarge_ = false;
  return f;
}

void SerialTransport::begin() {
  // USB CDC：波特率仅符号意义，Serial 已由 ARDUINO_USB_CDC_ON_BOOT 映射为 HWCDC
  Serial.begin(115200);
}

void SerialTransport::poll() {
  int avail = Serial.available();
  if (avail <= 0) return;
  char buf[128];
  int n = 0;
  while (avail-- > 0 && n < static_cast<int>(sizeof(buf))) {
    int c = Serial.read();
    if (c < 0) break;
    buf[n++] = static_cast<char>(c);
  }
  if (n > 0) line_.feed(buf, static_cast<size_t>(n));
}

void SerialTransport::sendLine(const char* s) {
  Serial.write(s, strlen(s));
  Serial.write('\n');
  Serial.flush();  // CDC：确保发出
}

}  // namespace ccb
