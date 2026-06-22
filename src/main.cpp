#include <Arduino.h>
#include <M5Unified.h>

#include "core/AppConfig.h"
#include "core/AppLog.h"
#include "core/BatteryMeter.h"
#include "core/NetClient.h"
#include "core/PomodoroHistory.h"
#include "core/TimeSync.h"
#include "core/WifiPortal.h"

namespace {
constexpr uint32_t kRollCooldownMs = 650;
constexpr uint32_t kClockRefreshMs = 1000;
constexpr uint32_t kUiFrameDelayMs = 20;
constexpr uint32_t kPomodoroFrameMs = 60;
constexpr uint8_t kDisplayBrightness = 90;
constexpr uint8_t kPomodoroDoneVolume = 28;
constexpr float kShakeThreshold = 1.6f;
constexpr float kInvertThreshold = -0.70f;
constexpr float kLeanDeadzone = 0.08f;
constexpr int kHourglassTopY = 24;
constexpr int kHourglassBottomY = 232;
constexpr int kGrainCell = 2;
constexpr int kGrainW = 63;
constexpr int kGrainH = 98;
constexpr int kGrainMid = kGrainH / 2;
constexpr int kGrainX0 = 4;
constexpr int kGrainY0 = 30;
constexpr int kGrainCenter = kGrainW / 2;
constexpr int kHourglassSpriteX = 0;
constexpr int kHourglassSpriteY = 18;
constexpr int kHourglassSpriteW = 135;
constexpr int kHourglassSpriteH = 222;
constexpr int kFallingGrainCount = 12;

enum class Screen {
  Menu,
  Clock,
  PomodoroMenu,
  PomodoroSelect,
  PomodoroRecords,
  PomodoroRun,
  PomodoroPrompt,
  Dice,
};

constexpr const char *kMenuItems[] = {
    "Clock",
    "Pomodoro",
    "Dice",
};
constexpr int kMenuCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);
constexpr uint16_t kPomodoroMinutes[] = {15, 25, 50};
constexpr int kPomodoroCount = sizeof(kPomodoroMinutes) / sizeof(kPomodoroMinutes[0]);
constexpr const char *kPomodoroMenuItems[] = {"Start", "Records", "Exit"};
constexpr int kPomodoroMenuCount = sizeof(kPomodoroMenuItems) / sizeof(kPomodoroMenuItems[0]);
constexpr const char *kPromptItems[] = {"Reset", "Exit"};

Screen screen = Screen::Menu;
int menuIndex = 0;
int pomodoroMenuIndex = 0;
int pomodoroIndex = 1;
int promptIndex = 0;
int recordPage = 0;
uint32_t lastRollMs = 0;
uint32_t lastClockDrawMs = 0;
uint32_t lastPomodoroDrawMs = 0;
uint32_t pomodoroStartMs = 0;
uint32_t pomodoroDurationMs = 25UL * 60UL * 1000UL;
uint32_t bootMs = 0;
int dieValue = 1;
bool imuReady = false;
bool pomodoroInverted = false;
bool pomodoroActive = false;
bool pomodoroFinishedRecorded = false;
bool pomodoroGravityReady = false;
float pomodoroBaseAx = 0.0f;
float pomodoroBaseAy = 1.0f;
float pomodoroBaseAz = 0.0f;
float liquidLeanX = 0.0f;
float liquidLeanY = 0.0f;
bool grainGrid[kGrainH][kGrainW] = {};
int grainCount = 0;
int grainCapacity = 0;
struct FallingGrain {
  float x = 0.0f;
  float y = 0.0f;
  float speed = 0.0f;
  bool active = false;
};
FallingGrain fallingGrains[kFallingGrainCount];
String lastClockText;
AppConfig appConfig;
WifiPortal wifiPortal;
TimeSync timeSync;
BatteryMeter battery;
HttpClient httpClient;
WsClient wsClient;
PomodoroHistory pomodoroHistory;
M5Canvas hourglassCanvas(&M5.Display);

void drawPomodoroPrompt();
void returnToMenu();

void drawFooter(const char *left, const char *right) {
  auto &display = M5.Display;
  const int w = display.width();
  const int h = display.height();

  display.setTextDatum(bottom_left);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString(left, 6, h - 4);

  display.setTextDatum(bottom_right);
  display.drawString(right, w - 6, h - 4);
}

void drawWifiIcon(int x, int y, bool connected) {
  auto &display = M5.Display;
  const uint16_t color = connected ? TFT_CYAN : TFT_DARKGREY;
  display.fillRect(x - 1, y - 1, 19, 16, TFT_BLACK);
  display.drawArc(x + 8, y + 13, 12, 10, 220, 320, color);
  display.drawArc(x + 8, y + 13, 8, 6, 225, 315, color);
  display.fillCircle(x + 8, y + 12, 2, color);
  if (!connected) {
    display.drawLine(x + 2, y + 2, x + 15, y + 14, TFT_RED);
  }
}

void drawTopStatus() {
  auto &display = M5.Display;
  const int w = display.width();
  battery.loop();
  display.fillRect(0, 0, w, 17, TFT_BLACK);
  drawWifiIcon(5, 1, wifiPortal.connected());
  display.setTextDatum(top_right);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString(battery.text(), w - 5, 3);
}

void drawStatusBar() {
  auto &display = M5.Display;
  const int w = display.width();

  battery.loop();
  display.fillRect(0, 0, w, 13, TFT_BLACK);
  drawWifiIcon(4, 0, wifiPortal.connected());
  display.setTextDatum(top_right);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString(battery.text(), w - 4, 2);
}

void drawMenu() {
  M5.Display.setRotation(0);
  auto &display = M5.Display;
  const int w = display.width();
  const int h = display.height();

  display.fillScreen(TFT_BLACK);
  drawTopStatus();
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString("STICK S3", w / 2, 22);

  display.setTextSize(1);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.drawString("Menu", w / 2, 48);

  for (int i = 0; i < kMenuCount; ++i) {
    const int y = 74 + i * 42;
    const bool selected = i == menuIndex;
    const uint16_t bg = selected ? TFT_CYAN : TFT_BLACK;
    const uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;

    display.fillRoundRect(16, y, w - 32, 32, 6, bg);
    display.setTextDatum(middle_center);
    display.setTextColor(fg, bg);
    display.setTextSize(2);
    display.drawString(kMenuItems[i], w / 2, y + 16);
  }

  display.setTextDatum(bottom_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString("B: Next  A: OK", w / 2, h - 8);
}

bool readAccel(float &ax, float &ay, float &az) {
  if (!imuReady) return false;
  if (!M5.Imu.update()) return false;

  const auto data = M5.Imu.getImuData();
  ax = data.accel.x;
  ay = data.accel.y;
  az = data.accel.z;
  return true;
}

void drawDieFace(int n) {
  auto &display = M5.Display;
  const int w = display.width();
  const int h = display.height();
  const int size = min(w, h) - 34;
  const int x = (w - size) / 2;
  const int y = (h - size) / 2 + 6;
  const int r = max(5, size / 12);
  const int inset = size / 4;
  const int mid = size / 2;

  display.fillScreen(TFT_BLACK);
  drawStatusBar();
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(1);
  display.drawString("Shake or press A", w / 2, 16);

  display.fillRoundRect(x, y, size, size, 10, TFT_WHITE);

  auto pip = [&](int px, int py) {
    display.fillCircle(x + px, y + py, r, TFT_BLACK);
  };

  if (n == 1 || n == 3 || n == 5) pip(mid, mid);
  if (n >= 2) {
    pip(inset, inset);
    pip(size - inset, size - inset);
  }
  if (n >= 4) {
    pip(size - inset, inset);
    pip(inset, size - inset);
  }
  if (n == 6) {
    pip(inset, mid);
    pip(size - inset, mid);
  }

  drawFooter("B: Menu", "");
}

void rollDie() {
  const uint32_t now = millis();
  if (now - lastRollMs < kRollCooldownMs) return;

  lastRollMs = now;
  dieValue = random(1, 7);
  LOGI("dice", "roll=%d", dieValue);
  drawDieFace(dieValue);
}

bool didShake() {
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  if (!readAccel(ax, ay, az)) return false;

  const float magnitude = sqrtf(ax * ax + ay * ay + az * az);
  return fabsf(magnitude - 1.0f) > kShakeThreshold;
}

void drawFlipCard(int x, int y, int width, int height, const char *value) {
  auto &display = M5.Display;
  constexpr uint16_t kCardTop = 0x3186;
  constexpr uint16_t kCardBottom = 0x18C3;
  constexpr uint16_t kCardBorder = 0x5AEB;

  display.fillRoundRect(x, y, width, height, 8, kCardBottom);
  display.fillRoundRect(x, y, width, height / 2, 8, kCardTop);
  display.drawRoundRect(x, y, width, height, 8, kCardBorder);
  display.drawFastHLine(x + 4, y + height / 2, width - 8, TFT_BLACK);
  display.drawFastHLine(x + 4, y + height / 2 + 1, width - 8, kCardBorder);

  display.setTextDatum(middle_center);
  display.setTextColor(TFT_WHITE, kCardTop);
  display.setTextSize(4);
  display.drawString(value, x + width / 2, y + height / 2 + 1);
}

void animateFlipCard(int x, int y, int width, int height, const String &fromValue,
                     const char *toValue) {
  auto &display = M5.Display;

  drawFlipCard(x, y, width, height, fromValue.c_str());
  for (int i = 0; i < 4; ++i) {
    const int fold = (height / 2) - i * 6;
    display.fillRect(x + 4, y + 4, width - 8, height / 2 - fold, TFT_BLACK);
    display.drawFastHLine(x + 4, y + height / 2 - i * 2, width - 8, TFT_DARKGREY);
    delay(18);
  }

  drawFlipCard(x, y, width, height, toValue);
  for (int i = 0; i < 3; ++i) {
    display.drawFastHLine(x + 4, y + height / 2 + i * 2, width - 8, TFT_DARKGREY);
    delay(16);
  }
}

void drawClockChrome(const String &subtitle) {
  M5.Display.setRotation(1);
  auto &display = M5.Display;
  const int w = display.width();

  display.fillScreen(TFT_BLACK);
  drawStatusBar();
  display.setTextDatum(top_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString(subtitle, w / 2, 18);
  drawFooter("B: Menu", "");
}

void drawPomodoroSelect() {
  M5.Display.setRotation(0);
  auto &display = M5.Display;
  const int w = display.width();
  const int h = display.height();

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString("Duration", w / 2, 18);

  for (int i = 0; i < kPomodoroCount; ++i) {
    const int y = 58 + i * 44;
    const bool selected = i == pomodoroIndex;
    const uint16_t bg = selected ? TFT_CYAN : TFT_DARKGREY;
    const uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;
    display.fillRoundRect(18, y, w - 36, 34, 7, bg);
    display.setTextDatum(middle_center);
    display.setTextColor(fg, bg);
    display.setTextSize(2);
    display.drawString(String(kPomodoroMinutes[i]) + " min", w / 2, y + 17);
  }

  display.setTextDatum(bottom_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString("B: Next  A: Start", w / 2, h - 8);
}

void drawPomodoroMenu() {
  M5.Display.setRotation(0);
  auto &display = M5.Display;
  const int w = display.width();
  const int h = display.height();

  display.fillScreen(TFT_BLACK);
  drawTopStatus();
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString("Pomodoro", w / 2, 20);

  for (int i = 0; i < kPomodoroMenuCount; ++i) {
    const int y = 74 + i * 42;
    const bool selected = i == pomodoroMenuIndex;
    const uint16_t bg = selected ? TFT_CYAN : TFT_BLACK;
    const uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;
    display.fillRoundRect(16, y, w - 32, 32, 6, bg);
    display.setTextDatum(middle_center);
    display.setTextColor(fg, bg);
    display.setTextSize(2);
    display.drawString(kPomodoroMenuItems[i], w / 2, y + 16);
  }

  display.setTextDatum(bottom_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString("B: Next  A: OK", w / 2, h - 8);
}

String recordTimeText(uint32_t epoch) {
  if (epoch < 100000) return "time unknown";
  time_t raw = epoch;
  struct tm info;
  localtime_r(&raw, &info);
  char buffer[18];
  strftime(buffer, sizeof(buffer), "%m-%d %H:%M", &info);
  return String(buffer);
}

void drawPomodoroRecords() {
  M5.Display.setRotation(0);
  auto &display = M5.Display;
  const int w = display.width();
  const int h = display.height();

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString("Records", w / 2, 18);

  const uint8_t count = pomodoroHistory.count();
  display.setTextSize(1);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  if (count == 0) {
    display.drawString("No records yet", w / 2, 82);
  } else {
    const int pageSize = 4;
    const int maxPage = (count - 1) / pageSize;
    if (recordPage > maxPage) recordPage = 0;
    for (int i = 0; i < pageSize; ++i) {
      const int idx = recordPage * pageSize + i;
      PomodoroRecord record;
      if (!pomodoroHistory.get(idx, record)) break;

      const int y = 52 + i * 38;
      const uint16_t color = record.completed ? TFT_GREEN : TFT_ORANGE;
      display.setTextDatum(top_left);
      display.setTextColor(color, TFT_BLACK);
      display.drawString(record.completed ? "DONE" : "STOP", 10, y);

      display.setTextColor(TFT_WHITE, TFT_BLACK);
      display.drawString(String(record.plannedMinutes) + "m", 10, y + 12);
      display.drawString(String(record.actualSeconds / 60) + "m" +
                         String(record.actualSeconds % 60) + "s", 50, y + 12);

      display.setTextColor(TFT_DARKGREY, TFT_BLACK);
      display.drawString(recordTimeText(record.startedAt), 10, y + 24);
    }
    display.setTextDatum(top_right);
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.drawString(String(recordPage + 1) + "/" + String(maxPage + 1), w - 8, 42);
  }

  display.setTextDatum(bottom_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString("B: Page  A: Back", w / 2, h - 8);
}

void fillLiquidPolygon(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4,
                       uint16_t color) {
  auto &display = M5.Display;
  display.fillTriangle(x1, y1, x2, y2, x3, y3, color);
  display.fillTriangle(x1, y1, x3, y3, x4, y4, color);
}

int lerpInt(int a, int b, float t) {
  return a + static_cast<int>((b - a) * t);
}

float applyLeanDeadzone(float value) {
  if (fabsf(value) < kLeanDeadzone) return 0.0f;
  const float sign = value > 0.0f ? 1.0f : -1.0f;
  return sign * ((fabsf(value) - kLeanDeadzone) / (1.0f - kLeanDeadzone));
}

int grainHalfWidth(int y) {
  if (y < kGrainMid) {
    return map(y, 0, kGrainMid - 1, 29, 2);
  }
  return map(y, kGrainMid, kGrainH - 1, 2, 29);
}

bool grainInside(int x, int y) {
  if (x < 0 || x >= kGrainW || y < 0 || y >= kGrainH) return false;
  return abs(x - kGrainCenter) <= grainHalfWidth(y);
}

bool hourglassInsideSprite(int x, int y, int inset = 0) {
  const int topY = kHourglassTopY - kHourglassSpriteY;
  const int bottomY = kHourglassBottomY - kHourglassSpriteY;
  const int waistY = (topY + bottomY) / 2;
  const int centerX = kHourglassSpriteW / 2;
  if (y < topY + inset || y > bottomY - inset) return false;

  int half = 0;
  if (y <= waistY) {
    half = map(y, topY, waistY, 62, 5);
  } else {
    half = map(y, waistY, bottomY, 5, 62);
  }
  half = max(0, half - inset);
  return x >= centerX - half && x <= centerX + half;
}

void clearGrains() {
  memset(grainGrid, 0, sizeof(grainGrid));
  grainCount = 0;
  grainCapacity = 0;
  for (auto &grain : fallingGrains) {
    grain.active = false;
  }
  for (int y = kGrainMid; y < kGrainH; ++y) {
    for (int x = 0; x < kGrainW; ++x) {
      if (grainInside(x, y)) ++grainCapacity;
    }
  }
}

int activeFallingGrainCount() {
  int count = 0;
  for (const auto &grain : fallingGrains) {
    if (grain.active) ++count;
  }
  return count;
}

void spawnFallingGrain(float leanX) {
  const int waistY = kGrainY0 + kGrainMid * kGrainCell - kHourglassSpriteY;
  const int centerX = kHourglassSpriteW / 2;
  for (auto &grain : fallingGrains) {
    if (grain.active) continue;
    const int x = centerX + static_cast<int>(leanX * 7.0f) + random(-2, 3);
    const int y = waistY - random(1, 5);
    if (!hourglassInsideSprite(x, y, 3)) continue;
    grain.x = x;
    grain.y = y;
    grain.speed = random(26, 42) / 10.0f;
    grain.active = true;
    return;
  }
}

bool addBottomGrainNear(float spriteX, float leanX) {
  const int gridX = static_cast<int>((spriteX + kHourglassSpriteX - kGrainX0) / kGrainCell);
  const int leanOffset = static_cast<int>(leanX * 4.0f);
  for (int y = kGrainMid; y < kGrainH; ++y) {
    for (int radius = 0; radius <= 6; ++radius) {
      const int offsets[] = {0, radius, -radius};
      for (int offset : offsets) {
        const int x = gridX + leanOffset + offset;
        if (grainInside(x, y) && !grainGrid[y][x]) {
          grainGrid[y][x] = true;
          ++grainCount;
          return true;
        }
      }
    }
  }
  return false;
}

void updateFallingGrains(float leanX) {
  const int bottomY = kHourglassBottomY - kHourglassSpriteY;
  for (auto &grain : fallingGrains) {
    if (!grain.active) continue;
    grain.y += grain.speed;
    grain.x += leanX * 1.8f;
    if (grain.y >= bottomY - 8 ||
        !hourglassInsideSprite(static_cast<int>(grain.x), static_cast<int>(grain.y), 2)) {
      addBottomGrainNear(grain.x, leanX);
      grain.active = false;
    }
  }
}

void simulateGrains(float leanX) {
  const int preferred = leanX >= 0.0f ? 1 : -1;
  const int other = -preferred;
  const int gravityBias = constrain(static_cast<int>(fabsf(leanX) * 100.0f), 0, 100);

  for (int y = kGrainH - 2; y >= kGrainMid; --y) {
    const int xStart = preferred > 0 ? kGrainW - 1 : 0;
    const int xEnd = preferred > 0 ? -1 : kGrainW;
    const int xStep = preferred > 0 ? -1 : 1;
    for (int x = xStart; x != xEnd; x += xStep) {
      if (!grainGrid[y][x]) continue;

      const bool strongTilt = gravityBias > 18;
      const int candidates[7][2] = {
          {strongTilt ? preferred : 0, 1},
          {preferred, 1},
          {0, 1},
          {preferred, 0},
          {preferred, 0},
          {other, 1},
          {other, 0},
      };

      for (const auto &candidate : candidates) {
        const int nx = x + candidate[0];
        const int ny = y + candidate[1];
        if (grainInside(nx, ny) && !grainGrid[ny][nx]) {
          grainGrid[y][x] = false;
          grainGrid[ny][nx] = true;
          break;
        }
      }
    }
  }
}

template <typename Gfx>
void drawTopGrains(Gfx &gfx, int remainingGrains, float leanX) {
  constexpr uint16_t kBlue = 0x04FF;
  constexpr uint16_t kBlueLight = 0x5DFF;
  if (remainingGrains <= 0) return;

  int drawn = 0;
  for (int y = kGrainMid - 1; y >= 0 && drawn < remainingGrains; --y) {
    const int half = grainHalfWidth(y);
    const int rowShift = static_cast<int>((kGrainMid - y) * leanX * 0.22f);
    for (int dx = -half; dx <= half && drawn < remainingGrains; ++dx) {
      const int x = kGrainCenter + dx + rowShift;
      if (!grainInside(x, y)) continue;
      const int px = kGrainX0 + x * kGrainCell - kHourglassSpriteX;
      const int py = kGrainY0 + y * kGrainCell - kHourglassSpriteY;
      if (!hourglassInsideSprite(px + 1, py + 1, 3)) continue;
      gfx.fillRect(px, py, kGrainCell, kGrainCell,
                   ((x + y) % 7 == 0) ? kBlueLight : kBlue);
      ++drawn;
    }
  }
}

template <typename Gfx>
void drawBottomGrains(Gfx &gfx) {
  constexpr uint16_t kBlue = 0x04FF;
  constexpr uint16_t kBlueDark = 0x039B;
  for (int y = kGrainMid; y < kGrainH; ++y) {
    for (int x = 0; x < kGrainW; ++x) {
      if (!grainGrid[y][x]) continue;
      const int px = kGrainX0 + x * kGrainCell - kHourglassSpriteX;
      const int py = kGrainY0 + y * kGrainCell - kHourglassSpriteY;
      if (!hourglassInsideSprite(px + 1, py + 1, 3)) continue;
      gfx.fillRect(px, py, kGrainCell, kGrainCell,
                   ((x * 3 + y) % 8 == 0) ? kBlue : kBlueDark);
    }
  }
}

template <typename Gfx>
void drawFallingStream(Gfx &gfx, int centerX, float progress, float leanX) {
  (void)centerX;
  (void)progress;
  (void)leanX;
  constexpr uint16_t kBlueLight = 0x5DFF;
  for (const auto &grain : fallingGrains) {
    if (!grain.active) continue;
    const int x = static_cast<int>(grain.x);
    const int y = static_cast<int>(grain.y);
    if (!hourglassInsideSprite(x, y, 3)) continue;
    gfx.fillRect(x - 1, y - 1, kGrainCell, kGrainCell, kBlueLight);
  }
}

template <typename Gfx>
void drawHourglassFrame(Gfx &gfx, int centerX, int topY, int bottomY) {
  constexpr uint16_t kGlass = 0xBDF7;
  const int waistY = (topY + bottomY) / 2;
  const int topLeft = centerX - 62;
  const int topRight = centerX + 62;
  const int waistLeft = centerX - 5;
  const int waistRight = centerX + 5;
  const int bottomLeft = centerX - 62;
  const int bottomRight = centerX + 62;

  gfx.drawLine(topLeft, topY, topRight, topY, kGlass);
  gfx.drawLine(topLeft, topY, waistLeft, waistY, kGlass);
  gfx.drawLine(topRight, topY, waistRight, waistY, kGlass);
  gfx.drawLine(waistLeft, waistY, bottomLeft, bottomY, kGlass);
  gfx.drawLine(waistRight, waistY, bottomRight, bottomY, kGlass);
  gfx.drawLine(bottomLeft, bottomY, bottomRight, bottomY, kGlass);
}

void drawPomodoroStaticRun() {
  M5.Display.setRotation(0);
  auto &display = M5.Display;
  const int w = display.width();
  const int h = display.height();
  const int centerX = w / 2;

  display.fillScreen(TFT_BLACK);
  if (!hourglassCanvas.width()) {
    hourglassCanvas.setColorDepth(16);
    hourglassCanvas.createSprite(kHourglassSpriteW, kHourglassSpriteH);
  }
  hourglassCanvas.fillScreen(TFT_BLACK);
  drawHourglassFrame(hourglassCanvas, centerX - kHourglassSpriteX,
                     kHourglassTopY - kHourglassSpriteY,
                     kHourglassBottomY - kHourglassSpriteY);
  hourglassCanvas.pushSprite(kHourglassSpriteX, kHourglassSpriteY);
  display.setTextDatum(bottom_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString("Flip 180 for options", centerX, h - 4);
}

void calibratePomodoroGravity() {
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  float sumAx = 0.0f;
  float sumAy = 0.0f;
  float sumAz = 0.0f;
  int samples = 0;
  pomodoroGravityReady = false;
  delay(120);
  for (int i = 0; i < 24; ++i) {
    if (readAccel(ax, ay, az)) {
      sumAx += ax;
      sumAy += ay;
      sumAz += az;
      ++samples;
    }
    delay(12);
  }
  if (samples > 0) {
    pomodoroBaseAx = sumAx / samples;
    pomodoroBaseAy = sumAy / samples;
    pomodoroBaseAz = sumAz / samples;
    pomodoroGravityReady = true;
  }
  LOGI("pomodoro", "gravity base ready=%d ax=%.2f ay=%.2f az=%.2f",
       pomodoroGravityReady ? 1 : 0, pomodoroBaseAx, pomodoroBaseAy, pomodoroBaseAz);
}

void drawPomodoroRun(bool force = false) {
  const uint32_t now = millis();
  if (!force && now - lastPomodoroDrawMs < kPomodoroFrameMs) return;
  lastPomodoroDrawMs = now;

  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  if (readAccel(ax, ay, az)) {
    const float rawLeanX = applyLeanDeadzone(constrain((pomodoroBaseAx - ax) * 1.8f,
                                                       -1.0f, 1.0f));
    const float rawLeanY = applyLeanDeadzone(constrain((az - pomodoroBaseAz) * 1.4f,
                                                       -1.0f, 1.0f));
    liquidLeanX = liquidLeanX * 0.58f + rawLeanX * 0.42f;
    liquidLeanY = liquidLeanY * 0.66f + rawLeanY * 0.34f;
    bool inverted = false;
    if (pomodoroGravityReady) {
      const float baseMag = sqrtf(pomodoroBaseAx * pomodoroBaseAx +
                                  pomodoroBaseAy * pomodoroBaseAy +
                                  pomodoroBaseAz * pomodoroBaseAz);
      const float mag = sqrtf(ax * ax + ay * ay + az * az);
      if (baseMag > 0.01f && mag > 0.01f) {
        const float dot = (ax * pomodoroBaseAx + ay * pomodoroBaseAy + az * pomodoroBaseAz) /
                          (baseMag * mag);
        inverted = dot < kInvertThreshold;
      }
    }
    if (inverted && !pomodoroInverted) {
      pomodoroInverted = true;
      screen = Screen::PomodoroPrompt;
      promptIndex = 0;
      drawPomodoroPrompt();
      return;
    }
    if (!inverted) pomodoroInverted = false;
  }

  const uint32_t elapsed = now - pomodoroStartMs;
  const float progress = constrain(static_cast<float>(elapsed) / pomodoroDurationMs, 0.0f, 1.0f);
  if (!pomodoroFinishedRecorded && elapsed >= pomodoroDurationMs) {
    pomodoroFinishedRecorded = true;
    pomodoroActive = false;
    PomodoroRecord record;
    record.startedAt = pomodoroStartMs / 1000;
    time_t epoch = time(nullptr);
    if (epoch > 100000) record.startedAt = static_cast<uint32_t>(epoch - (pomodoroDurationMs / 1000));
    record.plannedMinutes = kPomodoroMinutes[pomodoroIndex];
    record.actualSeconds = pomodoroDurationMs / 1000;
    record.completed = true;
    pomodoroHistory.add(record);
    LOGI("pomodoro", "completed %u minutes", record.plannedMinutes);
    M5.Speaker.setVolume(kPomodoroDoneVolume);
    M5.Speaker.tone(880, 90);
    delay(140);
    M5.Speaker.tone(880, 90);
  }
  const uint32_t remainingSec = (pomodoroDurationMs > elapsed) ? (pomodoroDurationMs - elapsed) / 1000 : 0;
  const uint32_t mm = remainingSec / 60;
  const uint32_t ss = remainingSec % 60;
  char remainText[12];
  snprintf(remainText, sizeof(remainText), "%02lu:%02lu",
           static_cast<unsigned long>(mm), static_cast<unsigned long>(ss));

  M5.Display.setRotation(0);
  auto &display = M5.Display;
  const int w = display.width();
  const int h = display.height();
  const int centerX = w / 2;

  display.fillRect(0, 0, w, 18, TFT_BLACK);
  display.setTextDatum(top_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString(remainText, centerX, 6);

  updateFallingGrains(liquidLeanX);
  simulateGrains(liquidLeanX);
  int targetTransferred = static_cast<int>(grainCapacity * progress);
  if (elapsed > 0 && progress < 1.0f) {
    targetTransferred = max(1, targetTransferred);
  }
  int fallingCount = activeFallingGrainCount();
  int grainsToRelease = min(2, max(0, targetTransferred - grainCount - fallingCount));
  while (grainsToRelease-- > 0) {
    spawnFallingGrain(liquidLeanX);
  }
  fallingCount = activeFallingGrainCount();
  const int topGrains = max(0, grainCapacity - grainCount - fallingCount);

  if (!hourglassCanvas.width()) {
    hourglassCanvas.setColorDepth(16);
    hourglassCanvas.createSprite(kHourglassSpriteW, kHourglassSpriteH);
  }
  hourglassCanvas.fillScreen(TFT_BLACK);
  drawTopGrains(hourglassCanvas, topGrains, liquidLeanX);
  drawFallingStream(hourglassCanvas, centerX - kHourglassSpriteX, progress, liquidLeanX);
  drawBottomGrains(hourglassCanvas);
  drawHourglassFrame(hourglassCanvas, centerX - kHourglassSpriteX,
                     kHourglassTopY - kHourglassSpriteY,
                     kHourglassBottomY - kHourglassSpriteY);
  hourglassCanvas.pushSprite(kHourglassSpriteX, kHourglassSpriteY);
}

void drawPomodoroPrompt() {
  M5.Display.setRotation(0);
  auto &display = M5.Display;
  const int w = display.width();
  const int h = display.height();

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString("Pomodoro", w / 2, 24);
  display.setTextSize(1);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.drawString("Reset or exit?", w / 2, 52);

  for (int i = 0; i < 2; ++i) {
    const int y = 92 + i * 48;
    const bool selected = i == promptIndex;
    const uint16_t bg = selected ? TFT_CYAN : TFT_DARKGREY;
    const uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;
    display.fillRoundRect(16, y, w - 32, 34, 6, bg);
    display.setTextDatum(middle_center);
    display.setTextColor(fg, bg);
    display.setTextSize(2);
    display.drawString(kPromptItems[i], w / 2, y + 17);
  }

  display.setTextDatum(bottom_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString("B: Next  A: OK", w / 2, h - 8);
}

void savePomodoroStopped() {
  if (!pomodoroActive || pomodoroFinishedRecorded) return;

  const uint32_t elapsed = millis() - pomodoroStartMs;
  PomodoroRecord record;
  record.startedAt = pomodoroStartMs / 1000;
  time_t epoch = time(nullptr);
  if (epoch > 100000) record.startedAt = static_cast<uint32_t>(epoch - (elapsed / 1000));
  record.plannedMinutes = kPomodoroMinutes[pomodoroIndex];
  record.actualSeconds = elapsed / 1000;
  record.completed = false;
  pomodoroHistory.add(record);
  pomodoroActive = false;
  LOGI("pomodoro", "stopped planned=%u actual=%lu seconds",
       record.plannedMinutes, static_cast<unsigned long>(record.actualSeconds));
}

void drawClock(bool force = false) {
  const uint32_t now = millis();
  if (!force && now - lastClockDrawMs < kClockRefreshMs) return;
  lastClockDrawMs = now;

  char hhText[3];
  char mmText[3];
  char ssText[3];
  String subtitle;
  if (timeSync.ready()) {
    const String ntpTime = timeSync.timeText();
    strlcpy(hhText, ntpTime.substring(0, 2).c_str(), sizeof(hhText));
    strlcpy(mmText, ntpTime.substring(3, 5).c_str(), sizeof(mmText));
    strlcpy(ssText, ntpTime.substring(6, 8).c_str(), sizeof(ssText));
    subtitle = timeSync.dateText();
  } else {
    const uint32_t seconds = (now - bootMs) / 1000;
    const uint32_t hh = seconds / 3600;
    const uint32_t mm = (seconds / 60) % 60;
    const uint32_t ss = seconds % 60;
    snprintf(hhText, sizeof(hhText), "%02lu", static_cast<unsigned long>(hh));
    snprintf(mmText, sizeof(mmText), "%02lu", static_cast<unsigned long>(mm));
    snprintf(ssText, sizeof(ssText), "%02lu", static_cast<unsigned long>(ss));
    subtitle = wifiPortal.connected() ? "NTP waiting" : "Offline uptime";
  }

  const String clockText = String(hhText) + ":" + mmText + ":" + ssText;
  if (!force && clockText == lastClockText) return;

  const String oldClockText = lastClockText;
  const bool canAnimate = !force && oldClockText.length() == 8;
  lastClockText = clockText;

  drawClockChrome(subtitle);

  if (!canAnimate) {
    drawFlipCard(4, 43, 74, 72, hhText);
    drawFlipCard(83, 43, 74, 72, mmText);
    drawFlipCard(162, 43, 74, 72, ssText);
    return;
  }

  const String oldHh = oldClockText.substring(0, 2);
  const String oldMm = oldClockText.substring(3, 5);
  const String oldSs = oldClockText.substring(6, 8);

  if (oldHh != hhText) {
    animateFlipCard(4, 43, 74, 72, oldHh, hhText);
  } else {
    drawFlipCard(4, 43, 74, 72, hhText);
  }

  if (oldMm != mmText) {
    animateFlipCard(83, 43, 74, 72, oldMm, mmText);
  } else {
    drawFlipCard(83, 43, 74, 72, mmText);
  }

  if (oldSs != ssText) {
    animateFlipCard(162, 43, 74, 72, oldSs, ssText);
  } else {
    drawFlipCard(162, 43, 74, 72, ssText);
  }
}

void enterSelectedApp() {
  if (menuIndex == 0) {
    screen = Screen::Clock;
    lastClockText = "";
    drawClock(true);
    return;
  }

  if (menuIndex == 1) {
    screen = Screen::PomodoroMenu;
    pomodoroMenuIndex = 0;
    drawPomodoroMenu();
    return;
  }

  screen = Screen::Dice;
  drawDieFace(dieValue);
}

void returnToMenu() {
  if (screen == Screen::PomodoroRun || screen == Screen::PomodoroPrompt) {
    savePomodoroStopped();
  }
  M5.Display.setRotation(0);
  screen = Screen::Menu;
  drawMenu();
}

void startPomodoro() {
  pomodoroDurationMs = static_cast<uint32_t>(kPomodoroMinutes[pomodoroIndex]) * 60UL * 1000UL;
  pomodoroStartMs = millis();
  lastPomodoroDrawMs = 0;
  liquidLeanX = 0.0f;
  liquidLeanY = 0.0f;
  clearGrains();
  pomodoroInverted = false;
  pomodoroActive = true;
  pomodoroFinishedRecorded = false;
  screen = Screen::PomodoroRun;
  drawPomodoroStaticRun();
  calibratePomodoroGravity();
  LOGI("pomodoro", "start %u minutes", kPomodoroMinutes[pomodoroIndex]);
  drawPomodoroRun(true);
}
}  // namespace

void setup() {
  AppLog::begin(115200);
  LOGI("boot", "starting");

  auto cfg = M5.config();
  M5.begin(cfg);

  randomSeed(esp_random());
  bootMs = millis();
  M5.Display.setRotation(1);
  M5.Display.setBrightness(kDisplayBrightness);
  LOGI("display", "brightness=%u ui_loop=%luHz clock_redraw=1Hz",
       M5.Display.getBrightness(),
       static_cast<unsigned long>(1000 / kUiFrameDelayMs));
  imuReady = M5.Imu.getType() != m5::imu_none;
  battery.begin();

  if (!appConfig.begin()) {
    LOGE("config", "preferences init failed");
  }
  if (!pomodoroHistory.begin()) {
    LOGE("pomodoro", "history init failed");
  }
  const AppSettings &settings = appConfig.settings();
  httpClient.setBaseUrl(settings.httpBaseUrl);
  wsClient.onText([](const String &text) {
    LOGI("ws", "text=%s", text.c_str());
  });

  drawMenu();
  wifiPortal.begin(settings.deviceName);
  timeSync.begin(settings);
  wsClient.begin(settings.wsHost, settings.wsPort, settings.wsPath);

  drawMenu();
}

void loop() {
  M5.update();
  wifiPortal.loop();
  timeSync.loop(wifiPortal.connected());
  battery.loop();
  wsClient.loop();

  if (screen == Screen::Menu) {
    if (didShake()) {
      menuIndex = 0;
      drawMenu();
    }
    if (M5.BtnB.wasPressed()) {
      menuIndex = (menuIndex + 1) % kMenuCount;
      drawMenu();
    }
    if (M5.BtnA.wasPressed()) {
      enterSelectedApp();
    }
  } else {
    if (screen == Screen::PomodoroMenu) {
      if (M5.BtnB.wasPressed()) {
        pomodoroMenuIndex = (pomodoroMenuIndex + 1) % kPomodoroMenuCount;
        drawPomodoroMenu();
      }
      if (M5.BtnA.wasPressed()) {
        if (pomodoroMenuIndex == 0) {
          screen = Screen::PomodoroSelect;
          drawPomodoroSelect();
        } else if (pomodoroMenuIndex == 1) {
          recordPage = 0;
          screen = Screen::PomodoroRecords;
          drawPomodoroRecords();
        } else {
          returnToMenu();
        }
      }
    } else if (screen == Screen::PomodoroRecords) {
      if (M5.BtnB.wasPressed()) {
        const uint8_t count = pomodoroHistory.count();
        if (count > 0) recordPage = (recordPage + 1) % ((count - 1) / 4 + 1);
        drawPomodoroRecords();
      }
      if (M5.BtnA.wasPressed()) {
        screen = Screen::PomodoroMenu;
        drawPomodoroMenu();
      }
    } else if (screen == Screen::PomodoroSelect) {
      if (M5.BtnB.wasPressed()) {
        pomodoroIndex = (pomodoroIndex + 1) % kPomodoroCount;
        drawPomodoroSelect();
      }
      if (M5.BtnA.wasPressed()) {
        startPomodoro();
      }
    } else if (screen == Screen::PomodoroPrompt) {
      if (M5.BtnB.wasPressed()) {
        promptIndex = (promptIndex + 1) % 2;
        drawPomodoroPrompt();
      }
      if (M5.BtnA.wasPressed()) {
        if (promptIndex == 0) {
          savePomodoroStopped();
          startPomodoro();
        } else {
          returnToMenu();
        }
      }
    } else if (M5.BtnB.wasPressed()) {
      returnToMenu();
    } else if (screen == Screen::Dice) {
      if (M5.BtnA.wasPressed() || didShake()) {
        rollDie();
      }
    } else if (screen == Screen::Clock) {
      drawClock();
    } else if (screen == Screen::PomodoroRun) {
      drawPomodoroRun();
    }
  }

  delay(kUiFrameDelayMs);
}
