#include <algorithm>

#include <Arduino.h>
#include <M5Unified.h>

#include "core/AppConfig.h"
#include "core/AppLog.h"
#include "core/BatteryMeter.h"
#include "core/GuaHistory.h"
#include "core/PomodoroHistory.h"
#include "core/PowerPolicy.h"
#include "core/TimeSync.h"
#include "core/WifiPortal.h"
#include "apps/Pet.h"
#include "apps/SandTimer.h"
#include "iching_data.h"

namespace {
constexpr uint32_t kRollCooldownMs = 650;
constexpr uint32_t kClockRefreshMs = 1000;
constexpr uint32_t kPomodoroFrameMs = 67;
constexpr uint8_t kDisplayBrightness = 50;
constexpr uint8_t kPomodoroDoneVolume = 77;
constexpr float kShakeThreshold = 1.6f;
constexpr float kInvertThreshold = -0.70f;
constexpr float kLeanDeadzone = 0.08f;
constexpr int kHourglassTopY = 24;
constexpr int kHourglassBottomY = 232;
constexpr int kGrainCell = 2;
constexpr int kGrainW = 63;
constexpr int kGrainH = 104;
constexpr int kGrainMid = kGrainH / 2;
constexpr int kGrainX0 = 4;
constexpr int kGrainY0 = 24;
constexpr int kGrainCenter = kGrainW / 2;
constexpr int kHourglassSpriteX = 0;
constexpr int kHourglassSpriteY = 18;
constexpr int kHourglassSpriteW = 135;
constexpr int kHourglassSpriteH = 222;
constexpr int kFallingGrainCount = 24;
constexpr int kMaxBottomGrains = 1700;
constexpr int kSandInset = 1;

enum class Screen {
  Menu,
  Clock,
  PomodoroMenu,
  PomodoroSelect,
  PomodoroRecords,
  PomodoroRun,
  PomodoroPrompt,
  SandTimerSelect,
  SandTimerRun,
  SandTimerPrompt,
  Pet,
  SuanGua,
  DeviceInfo,
  ImuCal,
};

constexpr const char *kMenuItems[] = {
    "Pet",
    "Clock",
    "Pomodoro",
    "SandTimer",
    "SuanGua",
    "Info",
    "IMU Cal",
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
int sandTimerIndex = 2;
int deviceInfoPage = 0;
int promptIndex = 0;
int recordPage = 0;
int imuCalStep = 0;
uint32_t lastRollMs = 0;
uint32_t lastClockDrawMs = 0;
uint32_t lastPomodoroDrawMs = 0;
uint32_t lastDeviceInfoDrawMs = 0;
uint32_t pomodoroStartMs = 0;
uint32_t pomodoroDurationMs = 25UL * 60UL * 1000UL;
uint32_t bootMs = 0;
uint8_t guaLines[6] = {};
int guaLineCount = 0;
int guaPage = 0;
bool imuReady = false;
bool pomodoroInverted = false;
bool pomodoroActive = false;
bool pomodoroFinishedRecorded = false;
bool pomodoroGravityReady = false;
float pomodoroBaseAx = 0.0f;
float pomodoroBaseAy = 1.0f;
float pomodoroBaseAz = 0.0f;
float liquidLeanX = 0.0f;
float liquidLeanY = 1.0f;
bool grainGrid[kGrainH][kGrainW] = {};
int grainCount = 0;
int grainCapacity = 0;
struct FallingGrain {
  float x = 0.0f;
  float y = 0.0f;
  float speed = 0.0f;
  bool active = false;
};
struct BottomGrain {
  float x = 0.0f;
  float y = 0.0f;
  float vx = 0.0f;
  float vy = 0.0f;
  bool active = false;
};
FallingGrain fallingGrains[kFallingGrainCount];
BottomGrain bottomGrains[kMaxBottomGrains];
String lastClockText;
int lastClockRotation = -1;
AppConfig appConfig;
WifiPortal wifiPortal;
TimeSync timeSync;
BatteryMeter battery;
PowerPolicy powerPolicy;
PomodoroHistory pomodoroHistory;
GuaHistory guaHistory;
M5Canvas hourglassCanvas(&M5.Display);

struct ImuSample {
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  bool valid = false;
};
constexpr const char *kImuCalSteps[] = {
    "Vertical",
    "Left flat",
    "Right flat",
    "Face up",
    "Face down",
};
constexpr int kImuCalStepCount = sizeof(kImuCalSteps) / sizeof(kImuCalSteps[0]);
ImuSample imuCalSamples[kImuCalStepCount];

String recordTimeText(uint32_t epoch);

void drawPomodoroPrompt();
void drawClock(bool force);
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

  display.fillScreen(TFT_BLACK);
  drawTopStatus();
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString("STICK S3", w / 2, 22);

  constexpr int visibleCount = 3;
  int firstIndex = menuIndex - visibleCount + 1;
  if (firstIndex < 0) firstIndex = 0;
  if (firstIndex > kMenuCount - visibleCount) firstIndex = max(0, kMenuCount - visibleCount);

  display.setTextDatum(top_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString(String(menuIndex + 1) + "/" + String(kMenuCount), w / 2, 49);

  for (int row = 0; row < visibleCount; ++row) {
    const int i = firstIndex + row;
    if (i >= kMenuCount) break;
    const int y = 68 + row * 46;
    const bool selected = i == menuIndex;
    const uint16_t bg = selected ? TFT_CYAN : TFT_BLACK;
    const uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;

    display.fillRoundRect(16, y, w - 32, 32, 6, bg);
    display.setTextDatum(middle_center);
    display.setTextColor(fg, bg);
    display.setTextSize(2);
    display.drawString(kMenuItems[i], w / 2, y + 16);
  }

  display.setTextDatum(middle_right);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  if (firstIndex > 0) display.drawString("^", w - 5, 68);
  if (firstIndex + visibleCount < kMenuCount) display.drawString("v", w - 5, 68 + (visibleCount - 1) * 46 + 16);
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

bool guaLineIsYang(uint8_t line) {
  return line == 7 || line == 9;
}

bool guaLineIsMoving(uint8_t line) {
  return line == 6 || line == 9;
}

uint8_t changedGuaLine(uint8_t line) {
  if (line == 6) return 7;
  if (line == 9) return 8;
  return line;
}

constexpr uint8_t kKingWenMap[8][8] = {
    {2, 24, 7, 19, 15, 36, 46, 11},
    {16, 51, 40, 54, 62, 55, 32, 34},
    {8, 3, 29, 60, 39, 63, 48, 5},
    {45, 17, 47, 58, 31, 49, 28, 43},
    {23, 27, 4, 41, 52, 22, 18, 26},
    {35, 21, 64, 38, 56, 30, 50, 14},
    {20, 42, 59, 61, 53, 37, 57, 9},
    {12, 25, 6, 10, 33, 13, 44, 1},
};

uint8_t guaTrigram(bool upper, bool changed) {
  uint8_t value = 0;
  const int start = upper ? 3 : 0;
  for (int i = 0; i < 3; ++i) {
    const uint8_t line = changed ? changedGuaLine(guaLines[start + i]) : guaLines[start + i];
    if (guaLineIsYang(line)) value |= 1 << i;
  }
  return value;
}

int guaNumber(bool changed) {
  if (guaLineCount < 6) return 0;
  return kKingWenMap[guaTrigram(true, changed)][guaTrigram(false, changed)];
}

bool guaHasMovingLine() {
  for (int i = 0; i < 6; ++i) {
    if (guaLineIsMoving(guaLines[i])) return true;
  }
  return false;
}

int guaHistoryPageIndex() {
  return guaHasMovingLine() ? 5 : 3;
}

int guaPageCount() {
  return guaHistoryPageIndex() + 1;
}

int drawWrappedText(const String &text, int x, int y, int width, int lineHeight, uint16_t color) {
  auto &display = M5.Display;
  display.setTextDatum(top_left);
  display.setTextColor(color, TFT_BLACK);
  display.setTextSize(1);
  String line;
  int cursorY = y;
  for (size_t i = 0; i < text.length();) {
    const uint8_t c = static_cast<uint8_t>(text[i]);
    const int charLen = c >= 0xE0 ? 3 : (c >= 0xC0 ? 2 : 1);
    const String token = text.substring(i, min(i + charLen, text.length()));
    const String candidate = line + token;
    if (display.textWidth(candidate) > width && line.length() > 0) {
      display.drawString(line, x, cursorY);
      cursorY += lineHeight;
      line = token;
    } else {
      line = candidate;
    }
    i += charLen;
  }
  if (line.length() > 0) {
    display.drawString(line, x, cursorY);
    cursorY += lineHeight;
  }
  return cursorY;
}

void drawYaoLine(int y, uint8_t line, bool filled) {
  auto &display = M5.Display;
  const int centerX = display.width() / 2;
  const int lineW = 80;
  const int gap = 14;
  const uint16_t color = filled ? TFT_CYAN : TFT_DARKGREY;
  const bool yang = filled && guaLineIsYang(line);

  if (!filled) {
    display.drawFastHLine(centerX - lineW / 2, y, lineW, color);
    return;
  }
  if (yang) {
    display.fillRect(centerX - lineW / 2, y - 2, lineW, 4, color);
  } else {
    display.fillRect(centerX - lineW / 2, y - 2, (lineW - gap) / 2, 4, color);
    display.fillRect(centerX + gap / 2, y - 2, (lineW - gap) / 2, 4, color);
  }
  if (guaLineIsMoving(line)) {
    display.fillCircle(centerX + lineW / 2 + 10, y, 3, TFT_ORANGE);
  }
}


void drawGuaSummary(bool transformed = false) {
  auto &display = M5.Display;
  const int ben = guaNumber(false);
  const int zhi = guaNumber(true);
  const IChingHexagram *benHex = getHexagramById(ben);
  const IChingHexagram *zhiHex = getHexagramById(zhi);
  const IChingHexagram *shownHex = transformed && zhiHex ? zhiHex : benHex;
  const int shownId = transformed && zhiHex ? zhi : ben;
  if (!shownHex) return;

  display.setFont(&fonts::efontCN_16_b);
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(1);
  display.drawString(String(shownId) + " " + shownHex->name, display.width() / 2, 48);

  display.setFont(&fonts::efontCN_12);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.drawString(String(shownHex->upper) + "上 / " + shownHex->lower + "下", display.width() / 2, 70);
  if (!transformed && guaHasMovingLine() && zhiHex) {
    display.drawString("之卦 " + String(zhi) + " " + zhiHex->name, display.width() / 2, 87);
  }
}

void drawHexagramExplanation(const IChingHexagram &hex, bool transformed) {
  auto &display = M5.Display;
  const int w = display.width();
  display.setFont(&fonts::efontCN_12);
  display.setTextSize(1);
  display.setTextDatum(top_left);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString(transformed ? "之卦" : "卦辞", 10, 108);

  if (transformed) {
    display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    int y = drawWrappedText(hex.judgement, 10, 126, w - 20, 14, TFT_LIGHTGREY);
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.drawString("趋势", 10, y + 4);
    drawWrappedText(hex.transformedSummary, 10, y + 20, w - 20, 14, TFT_WHITE);
  } else {
    int y = drawWrappedText(hex.judgement, 10, 126, w - 20, 15, TFT_LIGHTGREY);
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.drawString("解读", 10, y + 4);
    drawWrappedText(hex.summary, 10, y + 22, w - 20, 15, TFT_WHITE);
  }
}

void drawHexagramKeywords(const IChingHexagram &hex, bool transformed) {
  auto &display = M5.Display;
  const int w = display.width();
  display.setFont(&fonts::efontCN_12);
  display.setTextSize(1);
  display.setTextDatum(top_left);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString(transformed ? "之卦关键词" : "本卦关键词", 10, 108);
  drawWrappedText(hex.keywords, 10, 130, w - 20, 16, TFT_WHITE);
}

String guaRecordName(uint8_t id) {
  const IChingHexagram *hex = getHexagramById(id);
  if (!hex) return "--";
  return String(id) + " " + hex->name;
}

void drawGuaHistory() {
  auto &display = M5.Display;
  const int w = display.width();

  display.setFont(&fonts::Font0);
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString("History", w / 2, 48);

  const uint8_t count = guaHistory.count();
  display.setTextSize(1);
  if (count == 0) {
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.drawString("No records yet", w / 2, 96);
    return;
  }

  for (uint8_t i = 0; i < count; ++i) {
    GuaRecord record;
    if (!guaHistory.get(i, record)) break;

    const int y = 82 + i * 48;
    display.setTextDatum(top_left);
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.drawString(recordTimeText(record.createdAt), 10, y);

    display.setFont(&fonts::efontCN_12);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawString(guaRecordName(record.hexagramId), 10, y + 15);

    display.setTextColor(TFT_CYAN, TFT_BLACK);
    const String transformed = record.transformedId > 0 ? guaRecordName(record.transformedId) : "--";
    display.drawString(String("-> ") + transformed, 10, y + 30);
    display.setFont(&fonts::Font0);
  }
}

void drawTrigramHint() {
  auto &display = M5.Display;
  if (guaLineCount < 3) return;

  display.setFont(&fonts::efontCN_12);
  display.setTextDatum(top_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  const uint8_t lowerBits = guaTrigram(false, false);
  display.drawString(String("下卦 ") + getTrigramNameByBits(lowerBits) + " " + getTrigramMnemonicByBits(lowerBits), display.width() / 2, 207);
  if (guaLineCount >= 6) {
    const uint8_t upperBits = guaTrigram(true, false);
    display.drawString(String("上卦 ") + getTrigramNameByBits(upperBits) + " " + getTrigramMnemonicByBits(upperBits), display.width() / 2, 222);
  }
}
void drawSuanGua() {
  M5.Display.setRotation(0);
  auto &display = M5.Display;
  const int w = display.width();

  display.fillScreen(TFT_BLACK);
  drawStatusBar();
  display.setFont(&fonts::Font0);
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString("SuanGua", w / 2, 24);

  if (guaLineCount < 6 || guaPage == 0) {
    display.setTextDatum(top_center);
    display.setTextSize(1);
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    if (guaLineCount < 6) {
      display.drawString(String(guaLineCount) + "/6", w / 2, 51);
    } else {
      display.setFont(&fonts::efontCN_12);
      display.drawString("已成卦", w / 2, 51);
      display.setFont(&fonts::Font0);
    }
    for (int i = 5; i >= 0; --i) {
      const int row = 5 - i;
      drawYaoLine(68 + row * 21, i < guaLineCount ? guaLines[i] : 0, i < guaLineCount);
    }
    drawTrigramHint();
    display.setFont(&fonts::Font0);
    return;
  }

  if (guaPage == guaHistoryPageIndex()) {
    drawGuaHistory();
    return;
  }

  const bool hasMoving = guaHasMovingLine();
  const bool transformedPage = hasMoving && guaPage >= 3;
  const bool keywordPage = guaPage == 2 || (hasMoving && guaPage == 4);
  drawGuaSummary(transformedPage);
  const int hexId = transformedPage ? guaNumber(true) : guaNumber(false);
  const IChingHexagram *hex = getHexagramById(hexId);
  if (hex) {
    if (keywordPage) {
      drawHexagramKeywords(*hex, transformedPage);
    } else {
      drawHexagramExplanation(*hex, transformedPage);
    }
  }
  display.setFont(&fonts::Font0);
}

void resetSuanGua() {
  memset(guaLines, 0, sizeof(guaLines));
  guaLineCount = 0;
  guaPage = 0;
  drawSuanGua();
}

void saveCurrentGuaRecord() {
  const int ben = guaNumber(false);
  if (ben <= 0) return;

  GuaRecord record;
  record.createdAt = static_cast<uint32_t>(time(nullptr));
  record.hexagramId = static_cast<uint8_t>(ben);
  record.transformedId = guaHasMovingLine() ? static_cast<uint8_t>(guaNumber(true)) : 0;
  guaHistory.add(record);
  LOGI("suangua", "record hex=%u transformed=%u",
       record.hexagramId, record.transformedId);
}

void castGuaLine() {
  const uint32_t now = millis();
  if (now - lastRollMs < kRollCooldownMs) return;
  lastRollMs = now;

  if (guaLineCount >= 6) {
    guaPage = (guaPage + 1) % guaPageCount();
    drawSuanGua();
    return;
  }

  int coins = 0;
  for (int i = 0; i < 3; ++i) {
    coins += random(0, 2) == 0 ? 2 : 3;
  }
  guaLines[guaLineCount++] = static_cast<uint8_t>(coins);
  guaPage = 0;
  LOGI("suangua", "line=%d value=%d", guaLineCount, coins);
  if (guaLineCount == 6) {
    saveCurrentGuaRecord();
  }
  drawSuanGua();
}

bool didShake() {
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  if (!readAccel(ax, ay, az)) return false;

  const float magnitude = sqrtf(ax * ax + ay * ay + az * az);
  return fabsf(magnitude - 1.0f) > kShakeThreshold;
}

void noteUserActivity() {
  powerPolicy.noteActivity();
  if (screen == Screen::Clock) {
    lastClockText = "";
    lastClockDrawMs = 0;
    drawClock(true);
  }
}

bool isAnimatedScreen() {
  return screen == Screen::PomodoroRun || screen == Screen::SandTimerRun ||
         screen == Screen::Pet;
}

bool screenAllowsShake() {
  return screen == Screen::SuanGua;
}

String bytesText(uint32_t bytes) {
  if (bytes >= 1024UL * 1024UL) return String(bytes / (1024UL * 1024UL)) + " MB";
  return String(bytes / 1024UL) + " KB";
}

String uptimeText() {
  const uint32_t seconds = millis() / 1000;
  const uint32_t hh = seconds / 3600;
  const uint32_t mm = (seconds / 60) % 60;
  const uint32_t ss = seconds % 60;
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu",
           static_cast<unsigned long>(hh),
           static_cast<unsigned long>(mm),
           static_cast<unsigned long>(ss));
  return String(buffer);
}

void drawInfoRow(int y, const String &label, const String &value) {
  auto &display = M5.Display;
  display.setTextSize(1);
  display.setTextDatum(top_left);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.drawString(label, 8, y);
  display.setTextDatum(top_right);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString(value, display.width() - 8, y);
}

void drawDeviceInfo(bool force = false) {
  const uint32_t now = millis();
  if (!force && now - lastDeviceInfoDrawMs < 2000) return;
  lastDeviceInfoDrawMs = now;

  M5.Display.setRotation(0);
  auto &display = M5.Display;
  display.fillScreen(TFT_BLACK);
  drawStatusBar();
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString("Device Info", display.width() / 2, 18);
  display.setTextSize(1);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.drawString(String(deviceInfoPage + 1) + "/2", display.width() / 2, 39);

  battery.loop();
  int y = 58;
  if (deviceInfoPage == 0) {
    drawInfoRow(y, "Battery cap", "250 mAh"); y += 16;
    drawInfoRow(y, "Battery now", battery.level() >= 0 ? String(battery.level()) + "%" : String("--")); y += 16;
    drawInfoRow(y, "Voltage", battery.voltageMv() > 0 ? String(battery.voltageMv()) + " mV" : String("--")); y += 16;
    drawInfoRow(y, "Current", String(battery.currentMa()) + " mA"); y += 16;
    drawInfoRow(y, "Charging", battery.charging() ? "Yes" : "No"); y += 20;
    drawInfoRow(y, "WiFi", wifiPortal.statusText()); y += 16;
    drawInfoRow(y, "SSID", wifiPortal.ssid()); y += 16;
    drawInfoRow(y, "IP", wifiPortal.ip()); y += 16;
    drawInfoRow(y, "RSSI", wifiPortal.everConnected() ? String(wifiPortal.rssi()) + " dBm" : String("--")); y += 16;
    drawInfoRow(y, "NTP", timeSync.ready() ? "Synced" : "Waiting");
  } else {
    drawInfoRow(y, "Heap free", bytesText(ESP.getFreeHeap())); y += 16;
    drawInfoRow(y, "Heap min", bytesText(ESP.getMinFreeHeap())); y += 16;
    drawInfoRow(y, "Heap size", bytesText(ESP.getHeapSize())); y += 16;
    drawInfoRow(y, "PSRAM free", bytesText(ESP.getFreePsram())); y += 16;
    drawInfoRow(y, "PSRAM size", bytesText(ESP.getPsramSize())); y += 20;
    drawInfoRow(y, "Sketch", bytesText(ESP.getSketchSize())); y += 16;
    drawInfoRow(y, "Free sketch", bytesText(ESP.getFreeSketchSpace())); y += 16;
    drawInfoRow(y, "Flash", bytesText(ESP.getFlashChipSize())); y += 16;
    drawInfoRow(y, "CPU", String(ESP.getCpuFreqMHz()) + " MHz"); y += 16;
    drawInfoRow(y, "Uptime", uptimeText());
  }

  display.setTextDatum(bottom_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString("A Next  B Back", display.width() / 2, display.height() - 3);
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

void drawClockChrome(const String &subtitle, int rotation) {
  M5.Display.setRotation(rotation);
  auto &display = M5.Display;
  const int w = display.width();

  display.fillScreen(TFT_BLACK);
  drawStatusBar();
  display.setTextDatum(top_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString(subtitle, w / 2, 18);
}

int clockRotationFromImu() {
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  if (!readAccel(ax, ay, az)) return lastClockRotation >= 0 ? lastClockRotation : 0;

  if (fabsf(ax) >= fabsf(ay)) {
    return ax < 0.0f ? 0 : 2;
  }
  return ay > 0.0f ? 1 : 3;
}

void drawPortraitClock(const String &subtitle, const char *hhText, const char *mmText,
                       const char *ssText, int rotation, const String &oldClockText,
                       bool canAnimate) {
  M5.Display.setRotation(rotation);
  auto &display = M5.Display;
  const int w = display.width();

  display.fillScreen(TFT_BLACK);
  drawStatusBar();
  display.setTextDatum(top_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString(subtitle, w / 2, 17);

  const int cardX = (w - 110) / 2;
  const int cardW = 110;
  const int cardH = 58;
  const int yHh = 34;
  const int yMm = 99;
  const int ySs = 164;

  if (!canAnimate) {
    drawFlipCard(cardX, yHh, cardW, cardH, hhText);
    drawFlipCard(cardX, yMm, cardW, cardH, mmText);
    drawFlipCard(cardX, ySs, cardW, cardH, ssText);
  } else {
    const String oldHh = oldClockText.substring(0, 2);
    const String oldMm = oldClockText.substring(3, 5);
    const String oldSs = oldClockText.substring(6, 8);
    if (oldHh != hhText) {
      animateFlipCard(cardX, yHh, cardW, cardH, oldHh, hhText);
    } else {
      drawFlipCard(cardX, yHh, cardW, cardH, hhText);
    }
    if (oldMm != mmText) {
      animateFlipCard(cardX, yMm, cardW, cardH, oldMm, mmText);
    } else {
      drawFlipCard(cardX, yMm, cardW, cardH, mmText);
    }
    if (oldSs != ssText) {
      animateFlipCard(cardX, ySs, cardW, cardH, oldSs, ssText);
    } else {
      drawFlipCard(cardX, ySs, cardW, cardH, ssText);
    }
  }

  display.setTextDatum(middle_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString(":", w / 2, 96);
  display.drawString(":", w / 2, 161);
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
  for (auto &grain : bottomGrains) {
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

bool bottomCellFree(int x, int y) {
  return y >= kGrainMid && grainInside(x, y) && !grainGrid[y][x];
}

void rebuildBottomGrid() {
  memset(grainGrid, 0, sizeof(grainGrid));
  for (const auto &grain : bottomGrains) {
    if (!grain.active) continue;
    const int x = constrain(static_cast<int>(roundf(grain.x)), 0, kGrainW - 1);
    const int y = constrain(static_cast<int>(roundf(grain.y)), kGrainMid, kGrainH - 1);
    if (grainInside(x, y)) {
      grainGrid[y][x] = true;
    }
  }
}

void spawnFallingGrain(float leanX) {
  const int waistY = kGrainY0 + kGrainMid * kGrainCell - kHourglassSpriteY;
  const int centerX = kHourglassSpriteW / 2;
  for (auto &grain : fallingGrains) {
    if (grain.active) continue;
    const int x = centerX + static_cast<int>(leanX * 7.0f) + random(-2, 3);
    const int y = waistY - random(1, 5);
    if (!hourglassInsideSprite(x, y, kSandInset)) continue;
    grain.x = x;
    grain.y = y;
    grain.speed = random(26, 42) / 10.0f;
    grain.active = true;
    return;
  }
}

bool addBottomGrainAt(float spriteX, float spriteY) {
  if (grainCount >= grainCapacity) return false;
  const int gridX = static_cast<int>((spriteX + kHourglassSpriteX - kGrainX0) / kGrainCell);
  const int gridY = constrain(static_cast<int>((spriteY + kHourglassSpriteY - kGrainY0) /
                                               kGrainCell),
                              kGrainMid, kGrainH - 1);
  for (int dy = 0; dy < kGrainH; ++dy) {
    const int yDown = gridY + dy;
    const int yUp = gridY - dy;
    const int ys[] = {yDown, yUp};
    for (int y : ys) {
      if (y < kGrainMid || y >= kGrainH) continue;
    for (int radius = 0; radius <= 6; ++radius) {
      const int offsets[] = {0, radius, -radius};
      for (int offset : offsets) {
        const int x = gridX + offset;
        if (grainInside(x, y) && !grainGrid[y][x]) {
          grainGrid[y][x] = true;
          ++grainCount;
          return true;
        }
      }
    }
    }
  }
  return false;
}

bool addAnyBottomGrain() {
  if (grainCount >= grainCapacity) return false;
  for (int y = kGrainH - 1; y >= kGrainMid; --y) {
    const int half = grainHalfWidth(y);
    for (int radius = 0; radius <= half; ++radius) {
      const int offsets[] = {0, radius, -radius};
      for (int offset : offsets) {
        if (radius == 0 && offset != 0) continue;
        const int x = kGrainCenter + offset;
        if (bottomCellFree(x, y)) {
          grainGrid[y][x] = true;
          ++grainCount;
          return true;
        }
      }
    }
  }
  return false;
}

void updateFallingGrains(float gravityX) {
  const int bottomY = kHourglassBottomY - kHourglassSpriteY;
  for (auto &grain : fallingGrains) {
    if (!grain.active) continue;
    grain.y += grain.speed;
    grain.x += gravityX * grain.speed * 0.75f;
    if (grain.y >= bottomY - kSandInset - 1 ||
        !hourglassInsideSprite(static_cast<int>(grain.x), static_cast<int>(grain.y), kSandInset)) {
      addBottomGrainAt(grain.x, grain.y);
      grain.active = false;
    }
  }
}

void simulateGrains(float gravityX, float gravityY) {
  if (fabsf(gravityX) < 0.04f && gravityY < 0.04f) {
    gravityX = 0.0f;
    gravityY = 1.0f;
  }
  const int horizontal = gravityX >= 0.0f ? 1 : -1;
  const bool sideDominant = fabsf(gravityX) > gravityY;

  for (int pass = 0; pass < 2; ++pass) {
    const int yStart = sideDominant ? kGrainMid : kGrainH - 2;
    const int yEnd = sideDominant ? kGrainH : kGrainMid - 1;
    const int yStep = sideDominant ? 1 : -1;
    const int xStart = horizontal > 0 ? kGrainW - 1 : 0;
    const int xEnd = horizontal > 0 ? -1 : kGrainW;
    const int xStep = horizontal > 0 ? -1 : 1;

    for (int y = yStart; y != yEnd; y += yStep) {
      for (int x = xStart; x != xEnd; x += xStep) {
        if (!grainGrid[y][x]) continue;
        const int candidates[5][2] = {
            {x + horizontal, y},
            {x + horizontal, y + 1},
            {x, y + 1},
            {x - horizontal, y + 1},
            {x - horizontal, y},
        };
        int best = -1;
        float bestScore = -1000.0f;
        for (int i = 0; i < 5; ++i) {
          const int nx = candidates[i][0];
          const int ny = candidates[i][1];
          if (!bottomCellFree(nx, ny)) continue;
          const float score = (nx - x) * gravityX + (ny - y) * max(gravityY, 0.12f);
          if (score > bestScore) {
            bestScore = score;
            best = i;
          }
        }
        if (best >= 0 && bestScore > 0.02f) {
          grainGrid[y][x] = false;
          grainGrid[candidates[best][1]][candidates[best][0]] = true;
        }
      }
    }
  }
}
struct TopGrainCell {
  int8_t x;
  int8_t y;
  float score;
};

uint16_t sandColorFor(int x, int y, bool surface = false) {
  constexpr uint16_t kSandDark = 0x037B;
  constexpr uint16_t kSandMid = 0x04FF;
  constexpr uint16_t kSandBright = 0x5DFF;
  const uint8_t grain = static_cast<uint8_t>((x * 37 + y * 23 + (x ^ y) * 11) & 0x0F);
  if (surface && grain < 10) return kSandBright;
  if (grain < 3) return kSandBright;
  if (grain < 10) return kSandMid;
  return kSandDark;
}

template <typename Gfx>
void drawTopGrains(Gfx &gfx, int remainingGrains, float gravityX, float gravityY) {
  if (remainingGrains <= 0) return;

  if (fabsf(gravityX) < 0.04f && gravityY < 0.04f) {
    gravityX = 0.0f;
    gravityY = 1.0f;
  }
  static TopGrainCell cells[kMaxBottomGrains];
  int cellCount = 0;
  for (int y = 0; y < kGrainMid; ++y) {
    for (int x = 0; x < kGrainW; ++x) {
      if (!grainInside(x, y)) continue;
      const int px = kGrainX0 + x * kGrainCell - kHourglassSpriteX;
      const int py = kGrainY0 + y * kGrainCell - kHourglassSpriteY;
      if (!hourglassInsideSprite(px + 1, py + 1, kSandInset)) continue;
      const float centerBias = -fabsf(static_cast<float>(x - kGrainCenter)) * 0.015f;
      const float roughness = static_cast<float>((x * 17 + y * 11) % 7) * 0.01f;
      cells[cellCount++] = {static_cast<int8_t>(x), static_cast<int8_t>(y),
                            (x - kGrainCenter) * gravityX +
                                (y - kGrainMid) * max(gravityY, 0.0f) +
                                centerBias + roughness};
    }
  }
  std::sort(cells, cells + cellCount, [](const TopGrainCell &a, const TopGrainCell &b) {
    return a.score > b.score;
  });

  const int drawnCount = min(remainingGrains, cellCount);
  for (int i = 0; i < drawnCount; ++i) {
    const int x = cells[i].x;
    const int y = cells[i].y;
    const int px = kGrainX0 + x * kGrainCell - kHourglassSpriteX;
    const int py = kGrainY0 + y * kGrainCell - kHourglassSpriteY;
    gfx.fillRect(px, py, kGrainCell, kGrainCell, sandColorFor(x, y));
  }
}
template <typename Gfx>
void drawBottomGrains(Gfx &gfx) {
  for (int y = kGrainMid; y < kGrainH; ++y) {
    for (int x = 0; x < kGrainW; ++x) {
      if (!grainGrid[y][x]) continue;
      const int px = kGrainX0 + x * kGrainCell - kHourglassSpriteX;
      const int py = kGrainY0 + y * kGrainCell - kHourglassSpriteY;
      if (!hourglassInsideSprite(px + 1, py + 1, kSandInset)) continue;
      const bool surface = y == kGrainMid || !grainGrid[y - 1][x];
      gfx.fillRect(px, py, kGrainCell, kGrainCell, sandColorFor(x, y, surface));
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
    if (!hourglassInsideSprite(x, y, kSandInset)) continue;
    gfx.drawPixel(x, y, kBlueLight);
    if (((x + y) & 0x03) == 0) gfx.drawPixel(x, y + 1, kBlueLight);
  }
}

template <typename Gfx>
void drawHourglassFrame(Gfx &gfx, int centerX, int topY, int bottomY) {
  constexpr uint16_t kGlass = 0x7BEF;
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
  drawTopGrains(hourglassCanvas, grainCapacity, 0.0f, 1.0f);
  drawHourglassFrame(hourglassCanvas, centerX - kHourglassSpriteX,
                     kHourglassTopY - kHourglassSpriteY,
                     kHourglassBottomY - kHourglassSpriteY);
  hourglassCanvas.pushSprite(kHourglassSpriteX, kHourglassSpriteY);
  display.setTextDatum(bottom_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
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
    const float rawLeanX = applyLeanDeadzone(constrain((pomodoroBaseAy - ay) * 1.8f,
                                                       -1.0f, 1.0f));
    const float rawGravityY = constrain(-ax, 0.0f, 1.0f);
    liquidLeanX = liquidLeanX * 0.58f + rawLeanX * 0.42f;
    liquidLeanY = liquidLeanY * 0.66f + rawGravityY * 0.34f;
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
  simulateGrains(liquidLeanX, liquidLeanY);
  int targetTransferred = progress >= 1.0f
                              ? grainCapacity
                              : static_cast<int>(grainCapacity * progress);
  if (elapsed > 0 && progress < 1.0f) {
    targetTransferred = max(1, targetTransferred);
  }
  int fallingCount = activeFallingGrainCount();
  const int releaseLimit = progress >= 1.0f ? kFallingGrainCount : 2;
  int grainsToRelease = min(releaseLimit,
                            max(0, targetTransferred - grainCount - fallingCount));
  while (grainsToRelease-- > 0) {
    spawnFallingGrain(liquidLeanX);
  }
  if (progress >= 1.0f) {
    for (auto &grain : fallingGrains) {
      if (!grain.active) continue;
      addBottomGrainAt(grain.x, grain.y);
      grain.active = false;
    }
    while (grainCount < grainCapacity && addAnyBottomGrain()) {
    }
  }
  fallingCount = activeFallingGrainCount();
  const int topGrains = progress >= 1.0f ? 0 : max(0, grainCapacity - grainCount - fallingCount);

  if (!hourglassCanvas.width()) {
    hourglassCanvas.setColorDepth(16);
    hourglassCanvas.createSprite(kHourglassSpriteW, kHourglassSpriteH);
  }
  hourglassCanvas.fillScreen(TFT_BLACK);
  drawTopGrains(hourglassCanvas, topGrains, liquidLeanX, liquidLeanY);
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
}

bool readAverageAccel(float &ax, float &ay, float &az, int samples = 24) {
  float sx = 0.0f;
  float sy = 0.0f;
  float sz = 0.0f;
  int count = 0;
  for (int i = 0; i < samples; ++i) {
    float rx = 0.0f;
    float ry = 0.0f;
    float rz = 0.0f;
    if (readAccel(rx, ry, rz)) {
      sx += rx;
      sy += ry;
      sz += rz;
      ++count;
    }
    delay(10);
  }
  if (count == 0) return false;
  ax = sx / count;
  ay = sy / count;
  az = sz / count;
  return true;
}

void drawImuCal() {
  M5.Display.setRotation(0);
  auto &display = M5.Display;
  const int w = display.width();
  const int h = display.height();

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString("IMU Cal", w / 2, 18);

  display.setTextSize(1);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.drawString("Hold pose, press A", w / 2, 46);

  display.setTextDatum(top_left);
  for (int i = 0; i < kImuCalStepCount; ++i) {
    const int y = 66 + i * 30;
    const bool current = i == imuCalStep;
    display.setTextColor(current ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
    display.drawString(String(i + 1) + ". " + kImuCalSteps[i], 8, y);
    display.setTextColor(imuCalSamples[i].valid ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
    if (imuCalSamples[i].valid) {
      char buffer[36];
      snprintf(buffer, sizeof(buffer), "%.2f %.2f %.2f",
               imuCalSamples[i].ax, imuCalSamples[i].ay, imuCalSamples[i].az);
      display.drawString(buffer, 18, y + 12);
    } else {
      display.drawString("not captured", 18, y + 12);
    }
  }

  display.setTextDatum(bottom_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
}

void captureImuCalStep() {
  if (imuCalStep >= kImuCalStepCount) imuCalStep = 0;
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  if (readAverageAccel(ax, ay, az, 28)) {
    imuCalSamples[imuCalStep].ax = ax;
    imuCalSamples[imuCalStep].ay = ay;
    imuCalSamples[imuCalStep].az = az;
    imuCalSamples[imuCalStep].valid = true;
    LOGI("imu-cal", "%s ax=%.3f ay=%.3f az=%.3f",
         kImuCalSteps[imuCalStep], ax, ay, az);
    imuCalStep = (imuCalStep + 1) % kImuCalStepCount;
  } else {
    LOGW("imu-cal", "capture failed");
  }
  drawImuCal();
}

void enterImuCal() {
  screen = Screen::ImuCal;
  imuCalStep = 0;
  drawImuCal();
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
  const uint32_t refreshMs = kClockRefreshMs;
  if (!force && now - lastClockDrawMs < refreshMs) return;
  lastClockDrawMs = now;
  const int rotation = clockRotationFromImu();
  const bool portrait = rotation == 0 || rotation == 2;
  const bool rotationChanged = rotation != lastClockRotation;
  if (rotationChanged) {
    force = true;
    lastClockRotation = rotation;
  }

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
  const bool canAnimate = !powerPolicy.dimmed() && !powerPolicy.screenOff() && !force && oldClockText.length() == 8;
  lastClockText = clockText;

  if (portrait) {
    drawPortraitClock(subtitle, hhText, mmText, ssText, rotation, oldClockText, canAnimate);
    return;
  }

  drawClockChrome(subtitle, rotation);

  if (!canAnimate) {
    drawFlipCard(2, 41, 77, 78, hhText);
    drawFlipCard(81, 41, 77, 78, mmText);
    drawFlipCard(160, 41, 77, 78, ssText);
    return;
  }

  const String oldHh = oldClockText.substring(0, 2);
  const String oldMm = oldClockText.substring(3, 5);
  const String oldSs = oldClockText.substring(6, 8);

  if (oldHh != hhText) {
    animateFlipCard(2, 41, 77, 78, oldHh, hhText);
  } else {
    drawFlipCard(2, 41, 77, 78, hhText);
  }

  if (oldMm != mmText) {
    animateFlipCard(81, 41, 77, 78, oldMm, mmText);
  } else {
    drawFlipCard(81, 41, 77, 78, mmText);
  }

  if (oldSs != ssText) {
    animateFlipCard(160, 41, 77, 78, oldSs, ssText);
  } else {
    drawFlipCard(160, 41, 77, 78, ssText);
  }
}

void enterSelectedApp() {
  if (menuIndex == 0) {
    screen = Screen::Pet;
    Pet::begin();
    return;
  }

  if (menuIndex == 1) {
    screen = Screen::Clock;
    lastClockText = "";
    lastClockRotation = -1;
    drawClock(true);
    return;
  }

  if (menuIndex == 2) {
    screen = Screen::PomodoroMenu;
    pomodoroMenuIndex = 0;
    drawPomodoroMenu();
    return;
  }

  if (menuIndex == 3) {
    screen = Screen::SandTimerSelect;
    SandTimer::drawSelect(sandTimerIndex);
    return;
  }

  if (menuIndex == 4) {
    screen = Screen::SuanGua;
    resetSuanGua();
    return;
  }

  if (menuIndex == 5) {
    screen = Screen::DeviceInfo;
    deviceInfoPage = 0;
    drawDeviceInfo(true);
    return;
  }

  enterImuCal();
}

void returnToMenu() {
  if (screen == Screen::PomodoroRun || screen == Screen::PomodoroPrompt) {
    savePomodoroStopped();
  }
  if (screen == Screen::SandTimerRun || screen == Screen::SandTimerPrompt) {
    SandTimer::stop();
  }
  if (screen == Screen::Pet) {
    Pet::stop();
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
  liquidLeanY = 1.0f;
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
  powerPolicy.begin(kDisplayBrightness);
  LOGI("display", "brightness=%u", M5.Display.getBrightness());
  imuReady = M5.Imu.getType() != m5::imu_none;
  battery.begin();

  if (!appConfig.begin()) {
    LOGE("config", "preferences init failed");
  }
  if (!pomodoroHistory.begin()) {
    LOGE("pomodoro", "history init failed");
  }
  if (!guaHistory.begin()) {
    LOGE("suangua", "history init failed");
  }
  const AppSettings &settings = appConfig.settings();

  screen = Screen::Clock;
  lastClockText = "";
  lastClockRotation = -1;
  drawClock(true);
  wifiPortal.begin(settings.deviceName);
  timeSync.begin(settings);

  lastClockText = "";
  drawClock(true);
}

void loop() {
  M5.update();
  const bool btnA = M5.BtnA.wasPressed();
  const bool btnB = M5.BtnB.wasPressed();
  const bool animatedScreen = isAnimatedScreen();
  const bool shaken = powerPolicy.shouldSampleImu(screenAllowsShake(), animatedScreen) && didShake();
  if (btnA || btnB || shaken) noteUserActivity();
  powerPolicy.update(animatedScreen);

  wifiPortal.loop(!timeSync.ready());
  if (timeSync.loop(wifiPortal.connected())) {
    wifiPortal.shutdown();
  }
  battery.loop();

  if (screen == Screen::Menu) {
    if (shaken) {
      menuIndex = 0;
      drawMenu();
    }
    if (btnB) {
      menuIndex = (menuIndex + 1) % kMenuCount;
      drawMenu();
    }
    if (btnA) {
      enterSelectedApp();
    }
  } else {
    if (screen == Screen::PomodoroMenu) {
      if (btnB) {
        pomodoroMenuIndex = (pomodoroMenuIndex + 1) % kPomodoroMenuCount;
        drawPomodoroMenu();
      }
      if (btnA) {
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
      if (btnB) {
        const uint8_t count = pomodoroHistory.count();
        if (count > 0) recordPage = (recordPage + 1) % ((count - 1) / 4 + 1);
        drawPomodoroRecords();
      }
      if (btnA) {
        screen = Screen::PomodoroMenu;
        drawPomodoroMenu();
      }
    } else if (screen == Screen::PomodoroSelect) {
      if (btnB) {
        pomodoroIndex = (pomodoroIndex + 1) % kPomodoroCount;
        drawPomodoroSelect();
      }
      if (btnA) {
        startPomodoro();
      }
    } else if (screen == Screen::SandTimerSelect) {
      if (btnB) {
        sandTimerIndex = (sandTimerIndex + 1) % SandTimer::durationCount();
        SandTimer::drawSelect(sandTimerIndex);
      }
      if (btnA) {
        SandTimer::start(sandTimerIndex);
        screen = Screen::SandTimerRun;
      }
    } else if (screen == Screen::SandTimerPrompt) {
      if (btnB) {
        promptIndex = (promptIndex + 1) % 2;
        SandTimer::drawPrompt(promptIndex);
      }
      if (btnA) {
        if (promptIndex == 0) {
          SandTimer::stop();
          SandTimer::start(sandTimerIndex);
          screen = Screen::SandTimerRun;
        } else {
          returnToMenu();
        }
      }
    } else if (screen == Screen::PomodoroPrompt) {
      if (btnB) {
        promptIndex = (promptIndex + 1) % 2;
        drawPomodoroPrompt();
      }
      if (btnA) {
        if (promptIndex == 0) {
          savePomodoroStopped();
          startPomodoro();
        } else {
          returnToMenu();
        }
      }
    } else if (btnB) {
      returnToMenu();
    } else if (screen == Screen::SuanGua) {
      if (btnA || (shaken && guaLineCount < 6)) {
        castGuaLine();
      }
    } else if (screen == Screen::Clock) {
      drawClock();
    } else if (screen == Screen::PomodoroRun) {
      drawPomodoroRun();
    } else if (screen == Screen::SandTimerRun) {
      if (SandTimer::drawRun()) {
        screen = Screen::SandTimerPrompt;
        promptIndex = 0;
        SandTimer::drawPrompt(promptIndex);
      }
    } else if (screen == Screen::Pet) {
      if (btnA) {
        Pet::interact();
      } else {
        Pet::draw();
      }
    } else if (screen == Screen::DeviceInfo) {
      if (btnA) {
        deviceInfoPage = (deviceInfoPage + 1) % 2;
        drawDeviceInfo(true);
      } else {
        drawDeviceInfo();
      }
    } else if (screen == Screen::ImuCal) {
      if (btnA) {
        captureImuCalStep();
      }
    }
  }

  delay(powerPolicy.loopDelayMs(isAnimatedScreen()));
}
