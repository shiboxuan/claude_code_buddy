#pragma once
// claude_code_buddy 固件全局常量 — 协议 / 版本 / 硬件
// 依据: docs/claude_code_buddy/claude-code-buddy-api-protocol.md (§4)
//
// 本头文件被各模块引用，集中维护协议版本字符串、固件版本、设备标识与帧上限，
// 避免散落硬编码。后续 phase 实现 Protocol/SerialTransport 时复用此处常量。

#include <stddef.h>

namespace ccb {

// 协议版本字符串：每帧 protocol 字段固定为此值（protocol §4）
constexpr const char* PROTOCOL_VERSION = "ccb-serial-v1";

// 固件版本（hello 帧 fw_version，与 protocol 示例一致）
constexpr const char* FW_VERSION = "0.1.0";

// 设备标识（hello 帧 device）
constexpr const char* DEVICE_NAME = "m5stick-s3";

// hello 帧 features 上报项
constexpr const char* FEATURES_LCD = "lcd";
constexpr const char* FEATURES_SPEAKER = "speaker";
constexpr const char* FEATURES_BUTTONS = "buttons";

// 单帧大小上限（bytes）：超限帧拒绝、保持上一帧、上报 frame_too_large（protocol §4.1/§6）
constexpr size_t FRAME_MAX_BYTES = 1024;

// 行缓冲上限（含末尾 \n），略大于帧上限以容纳分隔符
constexpr size_t LINE_BUFFER_SIZE = 1100;

}  // namespace ccb
