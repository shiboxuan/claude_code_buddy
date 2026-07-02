// MascotRenderer 实现 — Claude 橙色像素小怪物动画引擎（首页重设计）
// 有状态：按 nowMs 步进位置/挤压/表情。小怪物恒橙，警示交给背景脉冲。
// 模式：Sleep(睡觉Zzz) / Idle(蹦跳·偷看·发呆) / Working(敲键盘) / Attention(挥手!) /
//       Error(发抖·汗滴·><) / Done(开心跳·火花)。事件到达一帧切回中间、丢弃 idle。
#include "MascotRenderer.h"

#include <M5Unified.h>
#include <math.h>

#include "assets/mascot_frames.h"
#include "ui/FrameBuffer.h"
#include "ui/Theme.h"

namespace ccb {
namespace {

using namespace ccb::mascot;

enum class Mode : uint8_t { Sleep, Idle, Working, Attention, Error, Done };
enum class Eyes : uint8_t { Open, Blink, Closed, Happy, Dizzy };

// idle 例程：一段段小动作串起来，跑完随机换下一段（含偷看 Peek）
enum class Routine : uint8_t { Hop, Look, Wiggle, Peek };

constexpr int kCx = theme::SCREEN_W / 2;  // 屏幕水平中心
constexpr float kHalfW = BODY_W / 2.0f + 2;

struct Anim {
  bool init = false;
  uint32_t lastMs = 0;
  Mode mode = Mode::Sleep;
  Mode prevMode = Mode::Sleep;

  // 位置（脚底点）
  float x = kCx;
  float feetY = GROUND_Y;

  // 当前动作段
  Routine routine = Routine::Look;
  int step = 0;
  int hopsLeft = 0;
  int side = -1;            // peek 方向：-1 左 / +1 右
  uint32_t segStart = 0;
  uint32_t segDur = 800;
  float sx0 = kCx, sx1 = kCx;  // x 线性插值端点
  float arcH = 0;              // 本段跳跃弧高（0=不跳）
  bool sqLand = false;         // 落地挤压
  bool wiggle = false;         // 原地抖动

  // 眨眼（独立计时）
  uint32_t nextBlink = 1800;
  bool blinking = false;
  uint32_t blinkEnd = 0;
} A;

// ——工具——
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerp(float a, float b, float u) { return a + (b - a) * u; }
inline int irand(int a, int b) { return a + static_cast<int>(random(b - a + 1)); }  // [a,b]
inline uint16_t C(uint32_t hex) { return theme::hexToRgb565(hex); }

Mode modeFor(GlobalState g, Color c) {
  switch (g) {
    case GlobalState::Working:   return Mode::Working;
    case GlobalState::Attention: return Mode::Attention;
    case GlobalState::Error:     return Mode::Error;
    case GlobalState::Idle:      return (c == Color::Blue) ? Mode::Done : Mode::Idle;
    default:                     return Mode::Sleep;  // disconnected / adapter / unknown
  }
}

// 配置一段 idle 动作。routine/step 决定几何；跑完 advance 换段。
void advance(uint32_t now) {
  A.segStart = now;
  A.arcH = 0;
  A.sqLand = false;
  A.wiggle = false;
  A.sx0 = A.x;

  switch (A.routine) {
    case Routine::Hop: {
      if (A.hopsLeft <= 0) {  // 换新例程
        int r = irand(0, 99);
        A.routine = (r < 34) ? Routine::Peek : (r < 64) ? Routine::Hop
                             : (r < 84) ? Routine::Look : Routine::Wiggle;
        A.step = 0;
        A.hopsLeft = irand(2, 4);
        advance(now);
        return;
      }
      // 朝随机方向蹦一小步
      float target = clampf(A.x + irand(-46, 46), 30.0f, theme::SCREEN_W - 30.0f);
      A.sx1 = target;
      A.arcH = irand(20, 30);
      A.sqLand = true;
      A.segDur = irand(360, 460);
      A.hopsLeft--;
      break;
    }
    case Routine::Look: {
      A.sx1 = A.x;
      A.segDur = irand(1500, 2600);
      A.routine = Routine::Hop;  // 看完回到蹦跳选择
      A.hopsLeft = 0;
      break;
    }
    case Routine::Wiggle: {
      A.sx1 = A.x;
      A.wiggle = true;
      A.segDur = irand(900, 1300);
      A.routine = Routine::Hop;
      A.hopsLeft = 0;
      break;
    }
    case Routine::Peek: {
      // step0 蹿出屏幕外藏起 → 1 藏着停顿 → 2 探出小半身 → 3 偷看停顿 → 4 缩回中间
      A.side = (A.step == 0) ? (irand(0, 1) ? 1 : -1) : A.side;
      const float hiddenL = -(kHalfW + 14), hiddenR = theme::SCREEN_W + kHalfW + 14;
      const float peekL = -(kHalfW - 16), peekR = theme::SCREEN_W + kHalfW - 16;
      const float hidden = (A.side < 0) ? hiddenL : hiddenR;
      const float peek = (A.side < 0) ? peekL : peekR;
      switch (A.step) {
        case 0: A.sx1 = hidden; A.arcH = 18; A.sqLand = true; A.segDur = 520; break;
        case 1: A.sx1 = hidden; A.segDur = irand(450, 750); break;         // 藏着
        case 2: A.sx1 = peek;   A.segDur = 340; break;                     // 探头
        case 3: A.sx1 = peek;   A.segDur = irand(1100, 1700); break;       // 偷看
        case 4: A.sx1 = kCx;    A.arcH = 22; A.sqLand = true; A.segDur = 480; break;  // 蹦回
      }
      A.step++;
      if (A.step > 4) { A.routine = Routine::Hop; A.step = 0; A.hopsLeft = 0; }
      break;
    }
  }
}

void resetIdle(uint32_t now) {
  A.routine = Routine::Hop;
  A.step = 0;
  A.hopsLeft = 0;
  A.x = kCx;
  advance(now);
}

// ——绘制原语——
void drawEyes(Eyes e, float cx, float ey, float scale) {
  auto& d = ccb::fb();
  uint16_t dk = C(HEX_DARK);
  int ew = static_cast<int>(9 * scale), eh = static_cast<int>(19 * scale);
  int gap = static_cast<int>(15 * scale);
  int lx = static_cast<int>(cx) - gap / 2 - ew;
  int rx = static_cast<int>(cx) + gap / 2;
  int cy = static_cast<int>(ey);
  switch (e) {
    case Eyes::Open:
      d.fillRoundRect(lx, cy, ew, eh, 3, dk);
      d.fillRoundRect(rx, cy, ew, eh, 3, dk);
      // 眼里一点高光
      d.fillRect(lx + 2, cy + 3, 2, 3, C(HEX_ACCENT));
      d.fillRect(rx + 2, cy + 3, 2, 3, C(HEX_ACCENT));
      break;
    case Eyes::Blink:
    case Eyes::Closed:
      d.fillRoundRect(lx, cy + eh / 2 - 2, ew, 4, 2, dk);
      d.fillRoundRect(rx, cy + eh / 2 - 2, ew, 4, 2, dk);
      break;
    case Eyes::Happy: {  // ^ ^
      int my = cy + eh / 2;
      for (int t = 0; t < 2; ++t) {  // 2px 粗
        d.drawLine(lx, my + t, lx + ew / 2, my - 6 + t, dk);
        d.drawLine(lx + ew / 2, my - 6 + t, lx + ew, my + t, dk);
        d.drawLine(rx, my + t, rx + ew / 2, my - 6 + t, dk);
        d.drawLine(rx + ew / 2, my - 6 + t, rx + ew, my + t, dk);
      }
      break;
    }
    case Eyes::Dizzy:  // > <
      for (int t = 0; t < 2; ++t) {
        d.drawLine(lx, cy + t, lx + ew, cy + eh / 2 + t, dk);
        d.drawLine(lx + ew, cy + eh / 2 + t, lx, cy + eh + t, dk);
        d.drawLine(rx + ew, cy + t, rx, cy + eh / 2 + t, dk);
        d.drawLine(rx, cy + eh / 2 + t, rx + ew, cy + eh + t, dk);
      }
      break;
  }
}

// 身体：脚底 (fx,ffeetY)，squashY 竖向挤压系数（<1 更扁更宽）。
void drawBody(float fx, float ffeetY, float squashY, Eyes eyes) {
  auto& d = ccb::fb();
  int h = static_cast<int>(BODY_H * squashY);
  int w = static_cast<int>(BODY_W * (1.0f + (1.0f - squashY) * 0.55f));  // 挤压时变宽
  int left = static_cast<int>(fx - w / 2.0f);
  int top = static_cast<int>(ffeetY) - h;
  int r = 9;

  d.fillRoundRect(left, top, w, h, r, C(HEX_BODY));
  d.fillRoundRect(left + 3, top + 3, w - 6, h / 5, r / 2, C(HEX_BODY_HI));  // 顶部高光带
  // 三条短腿：底部两条暗缝
  int legH = h / 4, notch = 6;
  d.fillRect(left + w / 3 - notch / 2, static_cast<int>(ffeetY) - legH, notch, legH, C(HEX_BODY_SH));
  d.fillRect(left + 2 * w / 3 - notch / 2, static_cast<int>(ffeetY) - legH, notch, legH, C(HEX_BODY_SH));
  // 眼睛（随挤压略降）
  drawEyes(eyes, fx, top + h * 0.40f, squashY);
}

// 一只爪子（小圆角橙块）
void drawPaw(float px, float py, int sz) {
  ccb::fb().fillRoundRect(static_cast<int>(px - sz / 2), static_cast<int>(py - sz / 2), sz, sz, sz / 2, C(HEX_BODY_HI));
}

void drawText(const char* s, int x, int y, uint32_t hex, uint8_t size) {
  auto& d = ccb::fb();
  d.setTextDatum(middle_center);
  d.setTextColor(C(hex));
  d.setTextSize(size);
  d.drawString(s, x, y);
  d.setTextSize(1);
}

// ——各模式绘制——
void drawIdle(uint32_t now) {
  // 段推进
  if (now - A.segStart >= A.segDur) advance(now);
  float u = clampf(static_cast<float>(now - A.segStart) / (A.segDur ? A.segDur : 1), 0.0f, 1.0f);
  A.x = lerp(A.sx0, A.sx1, u);
  float hop = -4.0f * A.arcH * u * (1.0f - u);  // 抛物线，负=上
  A.feetY = GROUND_Y + hop;

  float sq = 1.0f;
  if (A.arcH > 0) {
    if (u < 0.14f) sq = 0.88f;          // 起跳蓄力
    else if (u > 0.86f && A.sqLand) sq = 0.82f;  // 落地压扁
    else sq = 1.06f;                    // 空中拉伸
  }
  float bx = A.x;
  if (A.wiggle) {
    bx += sinf(now * 0.02f) * 4.0f;
    sq = 1.0f + sinf(now * 0.02f) * 0.08f;
  }

  Eyes e = A.blinking ? Eyes::Blink : Eyes::Open;
  drawBody(bx, A.feetY, sq, e);
}

void drawSleep(uint32_t now) {
  A.x = kCx;
  A.feetY = GROUND_Y;
  drawBody(kCx, GROUND_Y, 0.66f, Eyes::Closed);  // 趴扁
  int bodyTop = GROUND_Y - static_cast<int>(BODY_H * 0.66f);
  // Zzz 冒起
  const char* z[3] = {"z", "z", "Z"};
  for (int i = 0; i < 3; ++i) {
    float ph = fmodf(now * 0.0009f + i * 0.34f, 1.0f);
    if (ph > 0.86f) continue;
    int zx = kCx + 22 + i * 9;
    int zy = bodyTop - 2 - static_cast<int>(ph * 34);
    drawText(z[i], zx, zy, HEX_ACCENT, 1 + i);
  }
}

void drawWorking(uint32_t now) {
  A.x = kCx;
  float bob = sinf(now * 0.006f) * 2.0f;
  float feet = GROUND_Y + bob;
  drawBody(kCx, feet, 1.0f, Eyes::Open);

  // 键盘（身前）
  int kw = BODY_W + 20, kh = 14;
  int kx = kCx - kw / 2, ky = static_cast<int>(feet) - 6;
  ccb::fb().fillRoundRect(kx, ky, kw, kh, 3, C(0x22262A));
  int lit = (now / 130) % 4;
  for (int i = 0; i < 4; ++i) {
    int cxk = kx + 6 + i * (kw - 12) / 4;
    ccb::fb().fillRoundRect(cxk, ky + 4, (kw - 12) / 4 - 3, 6, 1, (i == lit) ? C(HEX_ACCENT) : C(HEX_KEY));
  }
  // 双爪交替敲击
  bool leftDown = ((now / 130) % 2) == 0;
  drawPaw(kCx - 14, ky - 2 - (leftDown ? 0 : 5), 12);
  drawPaw(kCx + 14, ky - 2 - (leftDown ? 5 : 0), 12);
}

void drawAttention(uint32_t now) {
  A.x = kCx;
  float bounce = fabsf(sinf(now * 0.012f)) * 14.0f;
  float feet = GROUND_Y - bounce;
  int bodyTop = static_cast<int>(feet) - BODY_H;
  drawBody(kCx, feet, 1.0f, Eyes::Open);

  // 双臂挥动
  float wave = sinf(now * 0.02f) * 6.0f;
  drawPaw(kCx - BODY_W / 2 - 4, bodyTop + 10 - wave, 12);
  drawPaw(kCx + BODY_W / 2 + 4, bodyTop + 10 + wave, 12);

  // 头顶 "!" 气泡
  float ybob = sinf(now * 0.02f) * 3.0f;
  int by = bodyTop - 16 + static_cast<int>(ybob);
  ccb::fb().fillRoundRect(kCx - 9, by - 12, 18, 24, 5, C(HEX_ACCENT));
  ccb::fb().fillRect(kCx - 2, by - 8, 4, 11, C(HEX_DARK));
  ccb::fb().fillRect(kCx - 2, by + 6, 4, 4, C(HEX_DARK));
}

void drawError(uint32_t now) {
  float shiver = sinf(now * 0.05f) * 3.0f;  // 发抖
  A.x = kCx + shiver;
  drawBody(A.x, GROUND_Y, 0.94f, Eyes::Dizzy);
  int bodyTop = GROUND_Y - static_cast<int>(BODY_H * 0.94f);
  // 小嘴（愁）
  ccb::fb().fillRect(static_cast<int>(A.x) - 5, bodyTop + static_cast<int>(BODY_H * 0.62f), 10, 3, C(HEX_DARK));
  // 蓝汗滴滑落（右脸）
  float ph = fmodf(now * 0.0012f, 1.0f);
  int dx = static_cast<int>(A.x) + BODY_W / 2 - 6;
  int dy = bodyTop + 6 + static_cast<int>(ph * (BODY_H * 0.7f));
  ccb::fb().fillCircle(dx, dy, 3, C(HEX_DROP));
  ccb::fb().fillTriangle(dx - 3, dy - 1, dx + 3, dy - 1, dx, dy - 7, C(HEX_DROP));
}

void drawDone(uint32_t now) {
  A.x = kCx;
  float jump = fabsf(sinf(now * 0.011f)) * 18.0f;
  float feet = GROUND_Y - jump;
  int bodyTop = static_cast<int>(feet) - BODY_H;
  drawBody(kCx, feet, 1.0f, Eyes::Happy);
  // 火花闪烁
  const int sx[4] = {kCx - 34, kCx + 34, kCx - 24, kCx + 26};
  const int sy[4] = {bodyTop + 4, bodyTop + 8, bodyTop - 12, bodyTop - 10};
  for (int i = 0; i < 4; ++i) {
    if (((now / 180) + i) % 3 == 0) continue;  // 交替闪
    int s = 3;
    ccb::fb().drawLine(sx[i] - s, sy[i], sx[i] + s, sy[i], C(HEX_ACCENT));
    ccb::fb().drawLine(sx[i], sy[i] - s, sx[i], sy[i] + s, C(HEX_ACCENT));
  }
}

}  // namespace

void MascotRenderer::render(GlobalState state, Color color, uint32_t nowMs) {
  // 初始化 + dt
  if (!A.init) {
    A.init = true;
    A.lastMs = nowMs;
    randomSeed(nowMs ^ 0x9E3779B9u);
    A.nextBlink = nowMs + irand(1500, 3500);
    resetIdle(nowMs);
  }

  Mode m = modeFor(state, color);

  // 事件到达：一帧切回中间、丢弃 idle（模式变化沿）
  if (m != A.prevMode) {
    A.x = kCx;
    A.feetY = GROUND_Y;
    if (m == Mode::Idle) resetIdle(nowMs);
    A.prevMode = m;
  }
  A.mode = m;

  // 眨眼计时（idle/清醒态用）
  if (!A.blinking && nowMs >= A.nextBlink) {
    A.blinking = true;
    A.blinkEnd = nowMs + 120;
  }
  if (A.blinking && nowMs >= A.blinkEnd) {
    A.blinking = false;
    A.nextBlink = nowMs + irand(1600, 4000);
  }

  switch (m) {
    case Mode::Sleep:     drawSleep(nowMs); break;
    case Mode::Idle:      drawIdle(nowMs); break;
    case Mode::Working:   drawWorking(nowMs); break;
    case Mode::Attention: drawAttention(nowMs); break;
    case Mode::Error:     drawError(nowMs); break;
    case Mode::Done:      drawDone(nowMs); break;
  }

  A.lastMs = nowMs;
}

}  // namespace ccb
