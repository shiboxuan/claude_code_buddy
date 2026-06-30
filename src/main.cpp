// claude_code_buddy 固件入口 — M5Stack StickS3
// FW-P0: 闪屏 hello world + USB CDC 初始化
// 详见 docs/claude_code_buddy/plans/firmware-development-plan.md FW-P0-T04 / T05
//
// 目标: M5.begin() 后屏幕亮、显示 "Claude Code Buddy" + 版本号；
//       USB CDC serial 输出一行 hello，host 端 pio device monitor 可读到。

#include <Arduino.h>
#include <M5Unified.h>

#include "app/Config.h"

static void drawSplash() {
    auto &d = M5.Display;
    d.clear(TFT_BLACK);
    d.setTextDatum(middle_center);
    d.setTextColor(TFT_WHITE, TFT_BLACK);

    d.setTextSize(2);
    d.drawString("Claude Code", d.width() / 2, d.height() / 2 - 16);
    d.drawString("Buddy", d.width() / 2, d.height() / 2 + 8);

    d.setTextSize(1);
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.drawString(String("fw ") + ccb::FW_VERSION, d.width() / 2, d.height() / 2 + 34);
}

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;  // USB CDC 下波特率仅符号意义，保持与 monitor_speed 一致
    M5.begin(cfg);

    // 屏幕亮、显示闪屏（FW-P0-T04）
    drawSplash();

    // USB CDC hello（FW-P0-T05）：host 端 `pio device monitor` 应能读到以下输出
    Serial.println("[ccb] hello from device");
    Serial.printf("[ccb] device=%s fw_version=%s protocol=%s\n",
                  ccb::DEVICE_NAME, ccb::FW_VERSION, ccb::PROTOCOL_VERSION);

    // 按钮 / 扬声器由 M5.begin(cfg) 完成初始化（StickS3）。
    // 提示音音色、边沿触发等在 FW-P7 实现；此处不发声。
}

void loop() {
    M5.update();
    delay(10);
}
