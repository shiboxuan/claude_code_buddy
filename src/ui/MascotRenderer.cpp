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

// idle 例程：一段段小动作串起来，跑完随机换下一段（含偷看 Peek / 灵感 Idea）
enum class Routine : uint8_t { Hop, Look, Wiggle, Peek, Idea };

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
  bool ideaSeg = false;        // 本段头顶冒灯泡（灵感彩蛋）

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
  A.ideaSeg = false;
  A.sx0 = A.x;

  switch (A.routine) {
    case Routine::Hop: {
      if (A.hopsLeft <= 0) {  // 换新例程
        int r = irand(0, 99);
        A.routine = (r < 30) ? Routine::Peek : (r < 56) ? Routine::Hop
                             : (r < 74) ? Routine::Look
                             : (r < 88) ? Routine::Wiggle : Routine::Idea;
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
    case Routine::Idea: {  // 原地停顿，头顶冒灯泡
      A.sx1 = A.x;
      A.ideaSeg = true;
      A.segDur = irand(1300, 1700);
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
// 方块黑眼（官方 mascot 招牌）：Open 方块 / Blink·Closed 一横 / Happy ^^ / Dizzy ><
void drawEyes(Eyes e, float cx, float ey, float scale) {
  auto& d = ccb::fb();
  uint16_t dk = C(HEX_DARK);
  int s = static_cast<int>(EYE * scale);
  if (s < 4) s = 4;
  int gap = static_cast<int>(EYE_GAP * scale);
  int lx = static_cast<int>(cx) - gap / 2 - s;
  int rx = static_cast<int>(cx) + gap / 2;
  int cy = static_cast<int>(ey);
  switch (e) {
    case Eyes::Open:
      d.fillRect(lx, cy, s, s, dk);
      d.fillRect(rx, cy, s, s, dk);
      // 左上角一点高光
      d.fillRect(lx + 2, cy + 2, 3, 3, C(HEX_ACCENT));
      d.fillRect(rx + 2, cy + 2, 3, 3, C(HEX_ACCENT));
      break;
    case Eyes::Blink:
    case Eyes::Closed:
      d.fillRect(lx, cy + s / 2 - 2, s, 4, dk);
      d.fillRect(rx, cy + s / 2 - 2, s, 4, dk);
      break;
    case Eyes::Happy: {  // ^ ^
      int my = cy + s;  // 底沿
      for (int t = 0; t < 2; ++t) {  // 2px 粗
        d.drawLine(lx, my + t, lx + s / 2, cy + t, dk);
        d.drawLine(lx + s / 2, cy + t, lx + s, my + t, dk);
        d.drawLine(rx, my + t, rx + s / 2, cy + t, dk);
        d.drawLine(rx + s / 2, cy + t, rx + s, my + t, dk);
      }
      break;
    }
    case Eyes::Dizzy:  // > <
      for (int t = 0; t < 2; ++t) {
        d.drawLine(lx, cy + t, lx + s, cy + s / 2 + t, dk);
        d.drawLine(lx + s, cy + s / 2 + t, lx, cy + s + t, dk);
        d.drawLine(rx + s, cy + t, rx, cy + s / 2 + t, dk);
        d.drawLine(rx, cy + s / 2 + t, rx + s, cy + s + t, dk);
      }
      break;
  }
}

// 身体：脚底基线 (fx,ffeetY)=腿底，squashY 竖向挤压系数（<1 更扁更宽）。
// 轮廓 = 三条短腿 + 方块躯干 + 眼睛高度两侧外伸的小方块短臂。
void drawBody(float fx, float ffeetY, float squashY, Eyes eyes) {
  auto& d = ccb::fb();
  uint16_t body = C(HEX_BODY), hi = C(HEX_BODY_HI), sh = C(HEX_BODY_SH);

  int th = static_cast<int>(BODY_H * squashY);                             // 躯干高（含挤压）
  int w = static_cast<int>(BODY_W * (1.0f + (1.0f - squashY) * 0.30f));    // 挤压时略变宽（克制，别拉胖）
  int legH = static_cast<int>(LEG_H * clampf(squashY, 0.5f, 1.0f));        // 挤压时腿也压短
  int feet = static_cast<int>(ffeetY);
  int torsoBot = feet - legH;
  int top = torsoBot - th;
  int left = static_cast<int>(fx - w / 2.0f);

  // 三条短腿（先画，上沿被躯干盖住）
  int span = w - LEG_W;  // 左右两腿中心跨度
  for (int i = 0; i < 3; ++i) {
    int cxl = left + LEG_W / 2 + span * i / 2;
    d.fillRect(cxl - LEG_W / 2, torsoBot, LEG_W, legH, body);
    d.fillRect(cxl - LEG_W / 2, feet - 2, LEG_W, 2, sh);  // 脚底暗面
  }
  // 侧短臂（眼睛高度，往外伸；+2 与躯干重叠避免缝）
  int armY = top + static_cast<int>(th * 0.28f);
  int armH = static_cast<int>(ARM_H * clampf(squashY, 0.6f, 1.2f));
  d.fillRect(left - ARM_W, armY, ARM_W + 2, armH, body);
  d.fillRect(left + w - 2, armY, ARM_W + 2, armH, body);
  // 躯干（方块，硬边像素感）
  d.fillRect(left, top, w, th, body);
  d.fillRect(left + 3, top + 2, w - 6, th / 6, hi);        // 顶部高光带
  d.fillRect(left + 2, torsoBot - 3, w - 4, 3, sh);        // 躯干底暗面
  // 眼睛（随挤压略降）
  drawEyes(eyes, fx, top + th * 0.32f, squashY);
}

// 头顶灯泡（idle 灵感彩蛋）：暖黄玻璃 + 灰灯座 + 闪烁光芒。
void drawBulb(int cx, int cy, uint32_t now) {
  auto& d = ccb::fb();
  d.fillCircle(cx, cy, 8, C(HEX_BULB));
  d.fillCircle(cx - 2, cy - 2, 3, C(HEX_BULB_HI));  // 高光
  d.fillRect(cx - 4, cy + 6, 8, 4, C(HEX_BULB_BASE));  // 灯座
  d.fillRect(cx - 3, cy + 10, 6, 2, C(HEX_BULB_BASE));
  // 四道闪烁光芒
  const int rx[4] = {cx - 13, cx + 13, cx - 9, cx + 9};
  const int ry[4] = {cy, cy, cy - 11, cy - 11};
  for (int i = 0; i < 4; ++i) {
    if (((now / 200) + i) % 2 == 0) continue;
    d.drawLine(cx, cy, rx[i], ry[i], C(HEX_BULB_HI));
  }
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

  // 灵感彩蛋：头顶灯泡（前 ~180ms 内不画，模拟"啪"地亮起后浮动）
  if (A.ideaSeg && u > 0.12f) {
    int bulbX = static_cast<int>(bx);
    int bulbY = static_cast<int>(A.feetY) - BODY_FULL_H - 14 + static_cast<int>(sinf(now * 0.006f) * 2.0f);
    drawBulb(bulbX, bulbY, now);
  }
}

void drawSleep(uint32_t now) {
  A.x = kCx;
  A.feetY = GROUND_Y;
  drawBody(kCx, GROUND_Y, 0.66f, Eyes::Closed);  // 趴扁
  int bodyTop = GROUND_Y - static_cast<int>(BODY_FULL_H * 0.66f);
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

  // 蓝色头戴式耳机（盖住两侧短臂：头梁 + 侧连杆 + 耳罩）
  {
    auto& d = ccb::fb();
    uint16_t ph = C(HEX_PHONE), phs = C(HEX_PHONE_SH);
    int torsoTop = static_cast<int>(feet) - BODY_FULL_H;
    int tl = kCx - BODY_W / 2, tr = kCx + BODY_W / 2;
    int cupW = 12, cupH = 22, cupY = torsoTop + 10;
    d.fillRoundRect(tl - 2, torsoTop - 7, BODY_W + 4, 8, 4, ph);  // 头梁横条
    d.fillRect(tl - 5, torsoTop - 3, 5, 14, ph);                  // 左连杆
    d.fillRect(tr, torsoTop - 3, 5, 14, ph);                      // 右连杆
    d.fillRoundRect(tl - 9, cupY, cupW, cupH, 4, ph);             // 左耳罩
    d.fillRoundRect(tr - 3, cupY, cupW, cupH, 4, ph);             // 右耳罩
    d.fillRect(tl - 6, cupY + cupH - 4, cupW - 6, 3, phs);        // 耳罩暗面
    d.fillRect(tr, cupY + cupH - 4, cupW - 6, 3, phs);
  }

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
  int bodyTop = static_cast<int>(feet) - BODY_FULL_H;
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
  int bodyTop = GROUND_Y - static_cast<int>(BODY_FULL_H * 0.94f);
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
  int bodyTop = static_cast<int>(feet) - BODY_FULL_H;
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
