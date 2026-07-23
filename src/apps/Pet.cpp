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
      return {eye(82, 8, 49, 0, 0, 0), eye(82, 8, 49, 0, 0, 0), 18};
    case Mood::BlinkLow:
      return {eye(82, 8, 67, 0, 0, 0), eye(82, 8, 67, 0, 0, 0), 18};
    case Mood::Happy:
      return {eye(82, 38, 58, 24, 0, 0, 0, 0, 0, true), eye(82, 38, 58, 24, 0, 0, 0, 0, 0, true), 18};
    case Mood::Glee:
      return {eye(82, 34, 55, 27, 0, 0, 0, 0, 0, true), eye(82, 34, 55, 27, 0, 0, 0, 0, 0, true), 16};
    case Mood::Curious:
      return {eye(68, 58, 59, 3, 0, -5, -5, 1, 11), eye(86, 74, 54, 0, 0, 4, 6, -1, 14), 20};
    case Mood::Sleepy:
      return {eye(84, 30, 61, 24, 5, -3, 0, 3, 8), eye(84, 30, 61, 24, 5, 3, 0, 3, 8), 17};
    case Mood::Sleeping:
      return {eye(84, 8, 60, 0, 0, 0), eye(84, 8, 60, 0, 0, 0), 18};
    case Mood::Hungry:
      return {eye(78, 43, 60, 12, 23, -4, 1, 8, 10), eye(78, 43, 60, 12, 23, 4, -1, 8, 10), 19};
    case Mood::Excited:
      return {eye(88, 78, 54, 0, 0, 0, -4, -2, 12, true), eye(88, 78, 54, 0, 0, 0, 4, -2, 12, true), 12};
    case Mood::Dizzy:
      return {eye(76, 60, 57, 3, 3, 0, 0, 0, 0, false, true), eye(76, 60, 57, 3, 3, 0, 0, 0, 0, false, true), 20};
    case Mood::LowBattery:
      return {eye(80, 39, 61, 17, 24, -10, 0, 6, 0), eye(80, 39, 61, 17, 24, 10, 0, 6, 0), 18};
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
      hi = kRedHi;
      mid = kRedMid;
      low = 0x6000;
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

void drawSpiral(int x, int y, uint16_t color) {
  canvas.drawRect(x - 13, y - 13, 26, 26, color);
  canvas.drawRect(x - 8, y - 8, 17, 17, color);
  canvas.drawLine(x + 8, y - 8, x + 8, y + 4, color);
  canvas.drawLine(x + 8, y + 4, x - 2, y + 4, color);
  canvas.drawPixel(x, y, color);
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

void drawEyeCore(int cx, const EyeShape &shape, bool left, uint16_t hi, uint16_t mid, uint16_t low) {
  const int w = shape.width;
  const int h = shape.height;
  const int cy = shape.y;
  const int r = min(24, max(7, h / 3));
  const int tilt = left ? shape.tilt : -shape.tilt;

  canvas.fillRoundRect(cx - w / 2 + 4, cy - h / 2 + 7, w, h, r, 0x0020);
  canvas.fillRoundRect(cx - w / 2 - 2, cy - h / 2 - 2, w + 4, h + 4, r + 2, low);
  canvas.fillRoundRect(cx - w / 2, cy - h / 2, w, h, r, mid);

  for (int i = 0; i < h / 2; i += 3) {
    const int inset = 4 + i / 5;
    canvas.drawRoundRect(cx - w / 2 + inset, cy - h / 2 + 3 + i,
                         w - inset * 2, max(5, h - i * 2), max(5, r - i / 4), hi);
  }
  canvas.fillRoundRect(cx - w / 2 + 11, cy - h / 2 + 7, w - 22, max(5, h / 4), r / 2, hi);
  canvas.drawFastHLine(cx - w / 2 + 14, cy + h / 2 - 7, w - 28, low);

  cutLid(cx, cy, w + 6, h + 6, shape.topCut, tilt, true);
  cutLid(cx, cy, w + 6, h + 6, shape.bottomCut, -tilt / 2, false);

  if (mood == Mood::Sleeping || mood == Mood::BlinkHigh || mood == Mood::BlinkLow) {
    canvas.drawFastHLine(cx - w / 2 + 8, cy, w - 16, hi);
    canvas.drawFastHLine(cx - w / 2 + 8, cy + 1, w - 16, low);
    return;
  }

  if (shape.spiral) {
    drawSpiral(cx, cy, kBg);
  } else if (shape.heart) {
    drawHeart(cx, cy + 2, min(w, h) / 2, kPinkMid);
  } else if (shape.pupilRadius > 0) {
    drawPupil(cx + shape.gazeX, cy + shape.gazeY, shape.pupilRadius, hi);
  }

  canvas.fillCircle(cx - w / 2 + 21, cy - h / 2 + 16, 5, 0xFFFF);
  canvas.fillCircle(cx - w / 2 + 29, cy - h / 2 + 23, 2, 0xFFFF);
  if (shape.star) {
    drawStar(cx + w / 2 - 17, cy - h / 2 + 16, 0xFFFF);
  }
}

void drawChargingAccent(uint16_t hi) {
  canvas.drawLine(120, 88, 112, 103, hi);
  canvas.drawLine(112, 103, 125, 100, hi);
  canvas.drawLine(125, 100, 117, 113, hi);
}

void drawLowBatteryAccent(uint16_t hi) {
  canvas.drawRect(106, 91, 28, 12, hi);
  canvas.fillRect(134, 95, 3, 4, hi);
  canvas.fillRect(109, 94, 6, 6, hi);
}

void drawSleepAccent(uint16_t hi) {
  canvas.drawLine(110, 90, 122, 90, hi);
  canvas.drawLine(122, 90, 110, 101, hi);
  canvas.drawLine(110, 101, 124, 101, hi);
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
  const int pulse = (mood == Mood::Excited || mood == Mood::Charging || mood == Mood::Love)
                        ? ((frame % 6) < 3 ? -2 : 1)
                        : 0;
  pose.left.y += pulse;
  pose.right.y += pulse;

  const int w = canvas.width();
  const int leftX = w / 2 - pose.left.width / 2 - pose.gap / 2;
  const int rightX = w / 2 + pose.right.width / 2 + pose.gap / 2;

  drawBackground();
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