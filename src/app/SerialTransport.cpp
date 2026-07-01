// SerialTransport 实现 — FW-P1-T01（LineBuffer 已 inline 于头文件，FW-P8 native 可测）
#include "SerialTransport.h"

#include <Arduino.h>

namespace ccb {

void SerialTransport::begin() {
  // USB CDC：波特率仅符号意义，Serial 已由 ARDUINO_USB_CDC_ON_BOOT 映射为 HWCDC。
  // 默认 RX 缓冲 256B 易被大帧(device_snapshot ~360B)冲爆丢字节→帧损坏，扩到 2048。
  Serial.setRxBufferSize(2048);
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
