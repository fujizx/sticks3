#include "Pet.h"

#include <M5Unified.h>

namespace Pet {
namespace {
constexpr uint32_t kFrameMs = 67;
constexpr uint16_t kBg = 0x0000;
constexpr uint16_t kBgLift = 0x0841;
constexpr uint16_t kCyanHi = 0xDFFF;
constexpr uint16_t kCyanMid = 0x6F7F;
constexpr uint16_t kCyanLow = 0x1397;
constexpr uint16_t kDimHi = 0x6B6D;
constexpr uint16_t kDimMid = 0x31A6;
constexpr uint16_t kAmberHi = 0xFFF1;
constexpr uint16_t kAmberMid = 0xFDA0;
constexpr uint16_t kRedHi = 0xFBEF;
constexpr uint16_t kRedMid = 0xF986;
constexpr uint16_t kGreenHi = 0xDFF7;
constexpr uint16_t kGreenMid = 0x47E8;
constexpr uint16_t kVioletHi = 0xF3FF;
constexpr uint16_t kVioletMid = 0xA25F;
constexpr uint16_t kPinkHi = 0xFC9F;
constexpr uint16_t kPinkMid = 0xF81F;

enum class Mood : uint8_t {
  Idle,
  BlinkHigh,
  BlinkLow,
  Happy,
  Glee,
  Curious,
  Sleepy,
  Sleeping,
  Hungry,
  Excited,
  Dizzy,
  LowBattery,
  Charging,
  Proud,
  Grumpy,
  Skeptic,
  Worried,
  Love,
  Annoyed,
  Scared,
};

struct EyeShape {
  int width;
  int height;
  int y;
  int topCut;
  int bottomCut;
  int tilt;
  int gazeX;
  int gazeY;
  int pupilRadius;
  bool star;
  bool spiral;
  bool heart;
};

struct FacePose {
  EyeShape left;
  EyeShape right;
  int gap;
};

struct MoodInfo {
  Mood mood;
  const char *en;
  const char *zh;
};

constexpr MoodInfo kMoodList[] = {
    {Mood::Idle, "Idle", u8"\u5E73\u9759"},
    {Mood::BlinkHigh, "Blink High", u8"\u9AD8\u4F4D\u7728\u773C"},
    {Mood::BlinkLow, "Blink Low", u8"\u4F4E\u4F4D\u7728\u773C"},
    {Mood::Happy, "Happy", u8"\u5F00\u5FC3"},
    {Mood::Glee, "Glee", u8"\u96C0\u8DC3"},
    {Mood::Curious, "Curious", u8"\u597D\u5947"},
    {Mood::Sleepy, "Sleepy", u8"\u72AF\u56F0"},
    {Mood::Sleeping, "Sleeping", u8"\u7761\u7740"},
    {Mood::Hungry, "Hungry", u8"\u671F\u5F85"},
    {Mood::Excited, "Excited", u8"\u5174\u594B"},
    {Mood::Dizzy, "Dizzy", u8"\u6655\u4E4E"},
    {Mood::LowBattery, "Low Battery", u8"\u4F4E\u7535\u91CF"},
    {Mood::Charging, "Charging", u8"\u5145\u7535\u4E2D"},
    {Mood::Proud, "Proud", u8"\u5F97\u610F"},
    {Mood::Grumpy, "Grumpy", u8"\u4E0D\u723D"},
    {Mood::Skeptic, "Skeptic", u8"\u6000\u7591"},
    {Mood::Worried, "Worried", u8"\u62C5\u5FC3"},
    {Mood::Love, "Love", u8"\u559C\u6B22"},
    {Mood::Annoyed, "Annoyed", u8"\u70E6\u8E81"},
    {Mood::Scared, "Scared", u8"\u5BB3\u6015"},
};
constexpr int kMoodCount = sizeof(kMoodList) / sizeof(kMoodList[0]);

M5Canvas canvas(&M5.Display);
Mood mood = Mood::Idle;
int moodIndex = 0;
uint32_t lastDrawMs = 0;
uint8_t frame = 0;
bool active = false;

EyeShape eye(int width, int height, int y, int topCut, int bottomCut, int tilt,
             int gazeX = 0, int gazeY = 0, int pupilRadius = 0,
             bool star = false, bool spiral = false, bool heart = false) {
  return {width, height, y, topCut, bottomCut, tilt, gazeX, gazeY, pupilRadius, star, spiral, heart};
}

FacePose poseFor(Mood value) {
  switch (value) {
    case Mood::BlinkHigh:
      return {eye(82, 66, 56, 0, 0, 0, -3, 0, 12), eye(82, 66, 56, 0, 0, 0, 3, 0, 12), 18};
    case Mood::BlinkLow:
      return {eye(82, 66, 56, 0, 0, 0, -3, 0, 12), eye(82, 66, 56, 0, 0, 0, 3, 0, 12), 18};
    case Mood::Happy:
      return {eye(82, 66, 56, 0, 0, 0, -3, 0, 12), eye(82, 66, 56, 0, 0, 0, 3, 0, 12), 18};
    case Mood::Glee:
      return {eye(88, 70, 54, 0, 0, 0, -4, -2, 12, true), eye(88, 70, 54, 0, 0, 0, 4, -2, 12, true), 12};
    case Mood::Curious:
      return {eye(82, 66, 56, 0, 0, 0, -2, 0, 12), eye(82, 66, 56, 0, 0, 0, 2, 0, 12), 18};
    case Mood::Sleepy:
      return {eye(84, 30, 61, 24, 5, -3, 0, 3, 8), eye(84, 30, 61, 24, 5, 3, 0, 3, 8), 17};
    case Mood::Sleeping:
      return {eye(84, 8, 60, 0, 0, 0), eye(84, 8, 60, 0, 0, 0), 18};
    case Mood::Hungry:
      return {eye(78, 43, 60, 12, 23, -4, 1, 8, 10), eye(78, 43, 60, 12, 23, 4, -1, 8, 10), 19};
    case Mood::Excited:
      return {eye(88, 78, 54, 0, 0, 0, -4, -2, 12, true), eye(88, 78, 54, 0, 0, 0, 4, -2, 12, true), 12};
    case Mood::Dizzy:
      return {eye(84, 66, 56, 2, 2, 0, 0, 0, 0, false, true), eye(84, 66, 56, 2, 2, 0, 0, 0, 0, false, true), 16};
    case Mood::LowBattery:
      return {eye(82, 28, 66, 21, 9, -7, 0, 4, 0), eye(82, 28, 66, 21, 9, 7, 0, 4, 0), 18};
    case Mood::Charging:
      return {eye(84, 70, 55, 0, 0, 0, 0, 0, 11, true), eye(84, 70, 55, 0, 0, 0, 0, 0, 11, true), 16};
    case Mood::Proud:
      return {eye(84, 45, 56, 22, 0, -3, -1, -1, 9, true), eye(84, 45, 56, 22, 0, 3, 1, -1, 9, true), 14};
    case Mood::Grumpy:
      return {eye(82, 44, 57, 18, 5, 13, 4, 1, 10), eye(82, 44, 57, 18, 5, -13, -4, 1, 10), 18};
    case Mood::Skeptic:
      return {eye(82, 32, 63, 18, 5, -7, 4, 0, 9), eye(74, 56, 55, 4, 10, 9, -4, 0, 11), 20};
    case Mood::Worried:
      return {eye(72, 54, 61, 8, 20, -12, 5, 3, 9), eye(72, 54, 61, 8, 20, 12, -5, 3, 9), 24};
    case Mood::Love:
      return {eye(84, 68, 55, 0, 0, 0, 0, 0, 0, false, false, true), eye(84, 68, 55, 0, 0, 0, 0, 0, 0, false, false, true), 16};
    case Mood::Annoyed:
      return {eye(82, 34, 61, 20, 8, 7, 5, 0, 9), eye(78, 28, 63, 16, 7, -5, -5, 0, 8), 22};
    case Mood::Scared:
      return {eye(74, 64, 58, 6, 0, -14, 6, 3, 10), eye(74, 64, 58, 6, 0, 14, -6, 3, 10), 24};
    default:
      return {eye(82, 66, 56, 0, 0, 0, -3, 0, 12), eye(82, 66, 56, 0, 0, 0, 3, 0, 12), 18};
  }
}

void paletteFor(Mood value, uint16_t &hi, uint16_t &mid, uint16_t &low) {
  switch (value) {
    case Mood::LowBattery:
      hi = 0xFBEF;
      mid = 0xB104;
      low = 0x3000;
      return;
    case Mood::Charging:
      hi = kGreenHi;
      mid = kGreenMid;
      low = 0x0343;
      return;
    case Mood::Love:
      hi = 0xFFFF;
      mid = kPinkHi;
      low = kPinkMid;
      return;
    case Mood::Sleepy:
    case Mood::Sleeping:
    case Mood::Worried:
      hi = kDimHi;
      mid = kDimMid;
      low = 0x1083;
      return;
    default:
      hi = kCyanHi;
      mid = kCyanMid;
      low = kCyanLow;
      return;
  }
}

void ensureCanvas() {
  if (canvas.width() == M5.Display.width() && canvas.height() == M5.Display.height()) return;
  canvas.deleteSprite();
  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
}

void drawBackground() {
  canvas.fillScreen(kBg);
  const int w = canvas.width();
  const int h = canvas.height();
  for (int y = 0; y < h; y += 3) {
    canvas.drawFastHLine(0, y, w, y < h / 2 ? kBgLift : kBg);
  }
  canvas.drawRoundRect(3, 3, w - 6, h - 6, 13, 0x1082);
  canvas.drawFastHLine(10, 101, w - 20, 0x0841);
}

void drawFaceplate() {
  const int x = 7;
  const int y = 15;
  const int w = canvas.width() - 14;
  const int h = 84;
  canvas.fillRoundRect(x + 2, y + 4, w, h, 18, 0x0000);
  canvas.fillRoundRect(x, y, w, h, 18, 0x0020);
  canvas.drawRoundRect(x, y, w, h, 18, 0x39E7);
  canvas.drawRoundRect(x + 2, y + 2, w - 4, h - 4, 16, 0x1082);
  canvas.drawRoundRect(x + 4, y + 4, w - 8, h - 8, 14, 0x0841);
  canvas.drawFastHLine(x + 18, y + 8, w - 36, 0x7BEF);
  canvas.drawFastHLine(x + 22, y + 10, w - 52, 0x2104);
  canvas.fillCircle(x + 19, y + 15, 4, 0xFFFF);
  canvas.fillCircle(x + 25, y + 12, 2, 0x7BEF);
  canvas.drawFastHLine(x + 25, y + h - 9, w - 50, 0x0841);
}

void cutLid(int cx, int cy, int w, int h, int cut, int tilt, bool top) {
  if (cut <= 0) return;
  const int left = cx - w / 2 - 3;
  const int right = cx + w / 2 + 3;
  if (top) {
    const int base = cy - h / 2 - 3;
    const int l = base + cut + tilt;
    const int r = base + cut - tilt;
    canvas.fillTriangle(left, base, right, base, left, l, kBg);
    canvas.fillTriangle(right, base, left, l, right, r, kBg);
  } else {
    const int base = cy + h / 2 + 3;
    const int l = base - cut + tilt;
    const int r = base - cut - tilt;
    canvas.fillTriangle(left, base, right, base, left, l, kBg);
    canvas.fillTriangle(right, base, left, l, right, r, kBg);
  }
}

void drawStar(int x, int y, uint16_t color) {
  canvas.drawFastHLine(x - 7, y, 15, color);
  canvas.drawFastVLine(x, y - 7, 15, color);
  canvas.drawLine(x - 5, y - 5, x + 5, y + 5, color);
  canvas.drawLine(x + 5, y - 5, x - 5, y + 5, color);
}

void drawThickLine(int x0, int y0, int x1, int y1, uint16_t color) {
  canvas.drawLine(x0, y0, x1, y1, color);
  canvas.drawLine(x0 + 1, y0, x1 + 1, y1, color);
  canvas.drawLine(x0 - 1, y0, x1 - 1, y1, color);
  canvas.drawLine(x0, y0 + 1, x1, y1 + 1, color);
  canvas.drawLine(x0, y0 - 1, x1, y1 - 1, color);
}

void drawSpiral(int x, int y, uint16_t color) {
  const int wobble = (frame / 4) % 2;
  for (int i = 0; i < 2; ++i) {
    canvas.drawRect(x - 14 + i, y - 14 + i + wobble, 28 - i * 2, 28 - i * 2, color);
    canvas.drawRect(x - 9 + i, y - 9 + i + wobble, 18 - i * 2, 18 - i * 2, color);
  }
  drawThickLine(x + 9, y - 9 + wobble, x + 9, y + 4 + wobble, color);
  drawThickLine(x + 9, y + 4 + wobble, x - 3, y + 4 + wobble, color);
  canvas.fillCircle(x, y + wobble, 2, color);
}

void drawHeart(int x, int y, int size, uint16_t color) {
  const int r = max(2, size / 4);
  canvas.fillCircle(x - r, y - r, r, color);
  canvas.fillCircle(x + r, y - r, r, color);
  canvas.fillTriangle(x - size / 2, y - r + 1, x + size / 2, y - r + 1, x, y + size / 2, color);
  canvas.fillCircle(x - r / 2, y - r, max(1, r / 3), 0xFFFF);
}

void drawPupil(int x, int y, int radius, uint16_t hi) {
  canvas.fillCircle(x, y, radius + 4, 0x0000);
  canvas.fillCircle(x, y, radius, 0x0861);
  canvas.fillCircle(x - radius / 2, y - radius / 2, max(2, radius / 3), hi);
  canvas.fillCircle(x + radius / 3, y + radius / 3, 2, 0x39E7);
}

void drawHappyArcEye(int cx, int cy, int w, uint16_t hi, uint16_t mid, uint16_t low) {
  const int left = cx - w / 2 + 13;
  const int right = cx + w / 2 - 13;
  const int x1 = cx - 24;
  const int x2 = cx - 10;
  const int x3 = cx + 10;
  const int x4 = cx + 24;
  const int y0 = cy + 7;
  const int y1 = cy - 5;
  const int y2 = cy - 10;

  for (int offset = 4; offset >= 0; --offset) {
    const uint16_t color = offset > 1 ? low : mid;
    drawThickLine(left, y0 + offset, x1, y1 + offset / 2, color);
    drawThickLine(x1, y1 + offset / 2, x2, y2 + offset / 3, color);
    drawThickLine(x2, y2 + offset / 3, x3, y2 + offset / 3, color);
    drawThickLine(x3, y2 + offset / 3, x4, y1 + offset / 2, color);
    drawThickLine(x4, y1 + offset / 2, right, y0 + offset, color);
  }
  drawThickLine(left, y0, x1, y1, hi);
  drawThickLine(x1, y1, x2, y2, hi);
  drawThickLine(x2, y2, x3, y2, hi);
  drawThickLine(x3, y2, x4, y1, hi);
  drawThickLine(x4, y1, right, y0, hi);
}

void drawEyeCore(int cx, const EyeShape &shape, bool left, uint16_t hi, uint16_t mid, uint16_t low) {
  const int w = shape.width;
  const int h = shape.height;
  const int cy = shape.y;
  const int r = min(24, max(7, h / 3));
  const int tilt = left ? shape.tilt : -shape.tilt;
  const int x = cx - w / 2;
  const int y = cy - h / 2;

  if ((mood == Mood::Happy || mood == Mood::Glee) && shape.pupilRadius == 0) {
    drawHappyArcEye(cx, cy, w, hi, mid, low);
    if (shape.star) drawStar(cx + w / 2 - 13, cy - 18, 0xFFFF);
    return;
  }

  canvas.fillRoundRect(x + 4, y + 6, w, h, r, 0x0020);
  canvas.fillRoundRect(x - 3, y - 3, w + 6, h + 6, r + 3, low);
  canvas.fillRoundRect(x - 1, y - 1, w + 2, h + 2, r + 1, mid);
  canvas.fillRoundRect(x + 3, y + 4, w - 6, h - 8, max(4, r - 3), mid);

  const int topH = max(4, h / 4);
  const int bottomH = max(4, h / 3);
  if (h > 14) {
    canvas.fillRoundRect(x + 8, y + 6, w - 16, topH, max(3, topH / 2), hi);
    canvas.fillRoundRect(x + 6, y + h - bottomH - 2, w - 12, bottomH, max(4, bottomH / 2), low);
    canvas.drawRoundRect(x + 2, y + 2, w - 4, h - 4, max(5, r - 2), hi);
  }

  cutLid(cx, cy, w + 8, h + 8, shape.topCut, tilt, true);
  cutLid(cx, cy, w + 8, h + 8, shape.bottomCut, -tilt / 2, false);

  if (mood == Mood::Sleeping || h <= 12) {
    canvas.drawFastHLine(cx - w / 2 + 8, cy, w - 16, hi);
    canvas.drawFastHLine(cx - w / 2 + 8, cy + 1, w - 16, low);
    return;
  }

  if (shape.spiral) {
    drawSpiral(cx, cy, kBg);
  } else if (shape.heart) {
    const int beat = ((frame / 4) % 2 == 0) ? 3 : 0;
    drawHeart(cx, cy + 2, min(w, h) / 2 + beat, kPinkMid);
  } else if (shape.pupilRadius > 0) {
    drawPupil(cx + shape.gazeX, cy + shape.gazeY, shape.pupilRadius, hi);
  }

  if (h > 18) {
    canvas.fillCircle(cx - w / 2 + 18, cy - h / 2 + 13, 5, 0xFFFF);
    canvas.fillCircle(cx - w / 2 + 27, cy - h / 2 + 20, 2, 0xFFFF);
  }
  if (shape.star) {
    drawStar(cx + w / 2 - 17, cy - h / 2 + 16, 0xFFFF);
  }
}

void drawChargingAccent(uint16_t hi) {
  const uint16_t color = ((frame / 5) % 2 == 0) ? hi : kGreenMid;
  drawThickLine(120, 88, 112, 103, color);
  drawThickLine(112, 103, 125, 100, color);
  drawThickLine(125, 100, 117, 113, color);
  if ((frame / 6) % 2 == 0) {
    canvas.fillCircle(143, 86, 1, color);
    canvas.fillCircle(97, 111, 1, color);
  }
}

void drawLowBatteryAccent(uint16_t hi) {
  const uint16_t dim = ((frame / 10) % 2 == 0) ? hi : 0x6800;
  canvas.drawRect(102, 85, 36, 16, dim);
  canvas.fillRect(138, 90, 3, 6, dim);
  canvas.fillRect(106, 89, 5, 8, dim);
  canvas.drawFastHLine(116, 93, 13, 0x3000);
}

void drawSleepAccent(uint16_t hi) {
  if ((frame / 12) % 2 != 0) return;
  canvas.drawLine(110, 90, 122, 90, hi);
  canvas.drawLine(122, 90, 110, 101, hi);
  canvas.drawLine(110, 101, 124, 101, hi);
}

void animateEyePose(FacePose &pose) {
  const int step = (frame / 6) % 4;
  const int wave4[4] = {0, 1, 3, 1};
  switch (mood) {
    case Mood::Idle: {
      const int lookX[4] = {-12, 0, 12, 0};
      const int lookY[4] = {0, -2, 0, 3};
      pose.left.gazeX += lookX[step];
      pose.right.gazeX += lookX[step];
      pose.left.gazeY += lookY[step];
      pose.right.gazeY += lookY[step];
      break;
    }
    case Mood::BlinkHigh:
    case Mood::BlinkLow: {
      const int close[4] = {0, 32, 58, 32};
      const int amount = close[step];
      const int targetY = (mood == Mood::BlinkHigh) ? 49 : 67;
      pose.left.height = max(8, 66 - amount);
      pose.right.height = pose.left.height;
      pose.left.y = 56 + ((targetY - 56) * amount) / 58;
      pose.right.y = pose.left.y;
      pose.left.topCut = amount / 3;
      pose.right.topCut = amount / 3;
      pose.left.bottomCut = amount / 4;
      pose.right.bottomCut = amount / 4;
      if (amount > 40) {
        pose.left.pupilRadius = 0;
        pose.right.pupilRadius = 0;
      }
      break;
    }
    case Mood::Happy: {
      const int height[4] = {66, 50, 14, 50};
      const int y[4] = {56, 54, 55, 54};
      const int topCut[4] = {0, 12, 0, 12};
      const int bottomCut[4] = {0, 1, 0, 1};
      const int pupil[4] = {12, 9, 0, 9};
      pose.left.height = height[step];
      pose.right.height = height[step];
      pose.left.y = y[step];
      pose.right.y = y[step];
      pose.left.topCut = topCut[step];
      pose.right.topCut = topCut[step];
      pose.left.bottomCut = bottomCut[step];
      pose.right.bottomCut = bottomCut[step];
      pose.left.pupilRadius = pupil[step];
      pose.right.pupilRadius = pupil[step];
      pose.left.gazeY -= step == 1 ? 2 : 0;
      pose.right.gazeY -= step == 1 ? 2 : 0;
      break;
    }
    case Mood::Glee: {
      const int height[4] = {70, 76, 16, 64};
      const int y[4] = {54, 49, 52, 53};
      const int pupil[4] = {12, 10, 0, 9};
      pose.left.height = height[step];
      pose.right.height = height[step];
      pose.left.y = y[step];
      pose.right.y = y[step];
      pose.left.topCut = step == 3 ? 4 : 0;
      pose.right.topCut = pose.left.topCut;
      pose.left.bottomCut = 0;
      pose.right.bottomCut = 0;
      pose.left.pupilRadius = pupil[step];
      pose.right.pupilRadius = pupil[step];
      pose.left.gazeY -= step == 1 ? 4 : 0;
      pose.right.gazeY -= step == 1 ? 4 : 0;
      break;
    }
    case Mood::Curious: {
      const int width[4] = {82, 88, 96, 90};
      const int height[4] = {66, 72, 82, 74};
      const int y[4] = {56, 53, 50, 52};
      const int gazeX[4] = {0, 0, 4, 8};
      const int gazeY[4] = {0, -4, -6, -2};
      const int pupil[4] = {12, 13, 15, 14};
      pose.left.width = width[step];
      pose.right.width = width[step];
      pose.left.height = height[step];
      pose.right.height = height[step];
      pose.left.y = y[step];
      pose.right.y = y[step];
      pose.left.topCut = 0;
      pose.right.topCut = 0;
      pose.left.bottomCut = 0;
      pose.right.bottomCut = 0;
      pose.left.tilt = step == 3 ? -2 : 0;
      pose.right.tilt = step == 3 ? 2 : 0;
      pose.left.gazeX = gazeX[step] - 2;
      pose.right.gazeX = gazeX[step] + 2;
      pose.left.gazeY = gazeY[step];
      pose.right.gazeY = gazeY[step];
      pose.left.pupilRadius = pupil[step];
      pose.right.pupilRadius = pupil[step];
      break;
    }
    case Mood::Sleepy: {
      const int droop[4] = {1, 3, 5, 3};
      pose.left.topCut += droop[step];
      pose.right.topCut += droop[step];
      pose.left.height -= droop[step];
      pose.right.height -= droop[step];
      pose.left.y += droop[step] / 2;
      pose.right.y += droop[step] / 2;
      break;
    }
    case Mood::Sleeping: {
      const int y[4] = {0, 1, 0, -1};
      pose.left.y += y[step];
      pose.right.y += y[step];
      pose.left.height = 8 + (step == 1 ? 1 : 0);
      pose.right.height = pose.left.height;
      break;
    }
    case Mood::Hungry: {
      const int grow[4] = {0, 4, 7, 2};
      const int inward[4] = {0, 1, 6, 2};
      pose.left.width += grow[step];
      pose.right.width += grow[step];
      pose.left.height += grow[step];
      pose.right.height += grow[step];
      pose.left.gazeX += inward[step];
      pose.right.gazeX -= inward[step];
      pose.left.gazeY -= step == 2 ? 2 : 0;
      pose.right.gazeY -= step == 2 ? 2 : 0;
      pose.left.bottomCut += step == 3 ? 6 : 0;
      pose.right.bottomCut += step == 3 ? 6 : 0;
      break;
    }
    case Mood::Worried: {
      const int tremble[4] = {-2, 1, 2, -1};
      const int rounder[4] = {0, 1, 7, 2};
      pose.left.gazeX += 4 + tremble[step];
      pose.right.gazeX -= 4 + tremble[step];
      pose.left.width += rounder[step];
      pose.right.width += rounder[step];
      pose.left.height += rounder[step];
      pose.right.height += rounder[step];
      pose.left.topCut -= min(pose.left.topCut, step == 2 ? 4 : 0);
      pose.right.topCut -= min(pose.right.topCut, step == 2 ? 4 : 0);
      break;
    }
    case Mood::Scared: {
      const int wide[4] = {0, 4, 10, 3};
      const int tremble[4] = {0, -2, 3, -1};
      pose.left.width += wide[step];
      pose.right.width += wide[step];
      pose.left.height += wide[step];
      pose.right.height += wide[step];
      pose.left.gazeX += 4 + tremble[step];
      pose.right.gazeX -= 4 + tremble[step];
      pose.left.gazeY += step == 2 ? 3 : 0;
      pose.right.gazeY += step == 2 ? 3 : 0;
      break;
    }
    case Mood::Excited: {
      const int bounce[4] = {0, -3, 1, -2};
      pose.left.y += bounce[step];
      pose.right.y += bounce[step];
      pose.left.height += wave4[step] * 2;
      pose.right.height += wave4[step] * 2;
      pose.left.gazeY -= wave4[step];
      pose.right.gazeY -= wave4[step];
      break;
    }
    case Mood::Dizzy: {
      pose.left.width += wave4[step] * 2;
      pose.right.width += wave4[(step + 2) % 4] * 2;
      pose.left.height += wave4[(step + 1) % 4] * 2;
      pose.right.height += wave4[(step + 3) % 4] * 2;
      pose.left.y += step - 1;
      pose.right.y -= step - 1;
      break;
    }
    case Mood::LowBattery: {
      const int sag[4] = {1, 2, 4, 2};
      pose.left.y += sag[step];
      pose.right.y += sag[step];
      pose.left.height -= sag[step] / 2;
      pose.right.height -= sag[step] / 2;
      pose.left.topCut += sag[step];
      pose.right.topCut += sag[step];
      break;
    }
    case Mood::Charging: {
      const int charge[4] = {0, 3, 6, 3};
      pose.left.height += charge[step];
      pose.right.height += charge[step];
      pose.left.gazeY -= charge[step] / 2;
      pose.right.gazeY -= charge[step] / 2;
      break;
    }
    case Mood::Love: {
      const int beat[4] = {0, 2, 5, 2};
      pose.left.width += beat[step];
      pose.right.width += beat[step];
      pose.left.height += beat[step];
      pose.right.height += beat[step];
      pose.left.y -= beat[step] / 2;
      pose.right.y -= beat[step] / 2;
      break;
    }
    case Mood::Proud: {
      const int lift[4] = {0, 2, 5, 2};
      pose.left.topCut += lift[step];
      pose.right.topCut += step == 2 ? lift[step] + 4 : lift[step];
      pose.left.tilt -= lift[step];
      pose.right.tilt += lift[step];
      pose.left.gazeY -= 1 + lift[step] / 2;
      pose.right.gazeY -= 1 + lift[step] / 2;
      break;
    }
    case Mood::Grumpy: {
      const int press[4] = {0, 3, 7, 4};
      pose.left.topCut += press[step];
      pose.right.topCut += press[step];
      pose.left.height -= press[step] / 2;
      pose.right.height -= press[step] / 2;
      pose.left.gazeX += step == 2 ? 6 : 2;
      pose.right.gazeX -= step == 2 ? 6 : 2;
      break;
    }
    case Mood::Skeptic: {
      const int squint[4] = {0, 3, 6, 1};
      pose.left.topCut += squint[step];
      pose.right.bottomCut += squint[step];
      pose.left.gazeX += step == 2 ? 5 : 1;
      pose.right.gazeX += step == 2 ? 5 : 1;
      pose.left.y += step == 3 ? 2 : 0;
      pose.right.y -= step == 3 ? 2 : 0;
      break;
    }
    case Mood::Annoyed: {
      const int side[4] = {0, 2, 7, 3};
      pose.left.topCut += step == 1 ? 2 : 0;
      pose.right.topCut += step == 2 ? 7 : 2;
      pose.left.height -= step == 2 ? 3 : 0;
      pose.right.height -= side[step] / 2;
      pose.left.gazeX += side[step];
      pose.right.gazeX += side[step];
      break;
    }
    default:
      break;
  }
}

void drawLabel() {
  const MoodInfo &info = kMoodList[moodIndex];
  canvas.setTextSize(1);
  canvas.setTextDatum(top_center);
  canvas.setFont(&fonts::Font0);
  canvas.setTextColor(TFT_LIGHTGREY, kBg);
  canvas.drawString(info.en, canvas.width() / 2, 105);
  canvas.setFont(&fonts::efontCN_12);
  canvas.setTextColor(TFT_DARKGREY, kBg);
  canvas.drawString(info.zh, canvas.width() / 2, 121);
  canvas.setFont(&fonts::Font0);
}

void drawScene() {
  M5.Display.setRotation(1);
  ensureCanvas();

  uint16_t hi = kCyanHi;
  uint16_t mid = kCyanMid;
  uint16_t low = kCyanLow;
  paletteFor(mood, hi, mid, low);
  FacePose pose = poseFor(mood);
  int pulse = 0;
  if (mood == Mood::Excited || mood == Mood::Charging || mood == Mood::Love) {
    pulse = ((frame % 6) < 3 ? -2 : 1);
  } else if (mood == Mood::LowBattery) {
    pulse = ((frame / 14) % 2 == 0 ? 1 : 3);
  }
  pose.left.y += pulse;
  pose.right.y += pulse;
  animateEyePose(pose);

  const int w = canvas.width();
  const int leftX = w / 2 - pose.left.width / 2 - pose.gap / 2;
  const int rightX = w / 2 + pose.right.width / 2 + pose.gap / 2;

  drawBackground();
  drawFaceplate();
  drawEyeCore(leftX, pose.left, true, hi, mid, low);
  drawEyeCore(rightX, pose.right, false, hi, mid, low);
  if (mood == Mood::Charging) drawChargingAccent(hi);
  else if (mood == Mood::LowBattery) drawLowBatteryAccent(hi);
  else if (mood == Mood::Sleeping || mood == Mood::Sleepy) drawSleepAccent(hi);
  drawLabel();
  canvas.pushSprite(0, 0);
}
}  // namespace

void begin() {
  active = true;
  M5.Display.setRotation(1);
  ensureCanvas();
  moodIndex = 0;
  mood = kMoodList[moodIndex].mood;
  lastDrawMs = 0;
  frame = 0;
  draw(true);
}

void draw(bool force) {
  if (!active) return;
  const uint32_t now = millis();
  if (!force && now - lastDrawMs < kFrameMs) return;
  ++frame;
  lastDrawMs = now;
  drawScene();
}

void interact() {
  moodIndex = (moodIndex + 1) % kMoodCount;
  mood = kMoodList[moodIndex].mood;
  lastDrawMs = 0;
  frame = 0;
  draw(true);
}

void stop() {
  active = false;
}

}  // namespace Pet