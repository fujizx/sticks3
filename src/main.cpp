#include <Arduino.h>
#include <M5Unified.h>

namespace {
constexpr uint32_t kRollCooldownMs = 650;
constexpr uint32_t kClockRefreshMs = 250;
constexpr float kShakeThreshold = 1.6f;

enum class Screen {
  Menu,
  Dice,
  Clock,
};

constexpr const char *kMenuItems[] = {
    "Dice",
    "Clock",
};
constexpr int kMenuCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);

Screen screen = Screen::Menu;
int menuIndex = 0;
uint32_t lastRollMs = 0;
uint32_t lastClockDrawMs = 0;
uint32_t bootMs = 0;
int dieValue = 1;
bool imuReady = false;

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

void drawMenu() {
  auto &display = M5.Display;
  const int w = display.width();

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString("STICK S3", w / 2, 12);

  display.setTextSize(1);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.drawString("Menu", w / 2, 38);

  for (int i = 0; i < kMenuCount; ++i) {
    const int y = 64 + i * 38;
    const bool selected = i == menuIndex;
    const uint16_t bg = selected ? TFT_WHITE : TFT_BLACK;
    const uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;

    display.fillRoundRect(18, y, w - 36, 28, 6, bg);
    display.setTextDatum(middle_center);
    display.setTextColor(fg, bg);
    display.setTextSize(2);
    display.drawString(kMenuItems[i], w / 2, y + 14);
  }

  drawFooter("B: Next", "A: OK");
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
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(1);
  display.drawString("Shake or press A", w / 2, 6);

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
  drawDieFace(dieValue);
}

bool didShake() {
  if (!imuReady) return false;
  if (!M5.Imu.update()) return false;

  const auto data = M5.Imu.getImuData();
  const float magnitude = sqrtf(data.accel.x * data.accel.x +
                                data.accel.y * data.accel.y +
                                data.accel.z * data.accel.z);
  return fabsf(magnitude - 1.0f) > kShakeThreshold;
}

void drawClock(bool force = false) {
  const uint32_t now = millis();
  if (!force && now - lastClockDrawMs < kClockRefreshMs) return;
  lastClockDrawMs = now;

  const uint32_t seconds = (now - bootMs) / 1000;
  const uint32_t hh = seconds / 3600;
  const uint32_t mm = (seconds / 60) % 60;
  const uint32_t ss = seconds % 60;

  char timeText[16];
  snprintf(timeText, sizeof(timeText), "%02lu:%02lu:%02lu",
           static_cast<unsigned long>(hh),
           static_cast<unsigned long>(mm),
           static_cast<unsigned long>(ss));

  auto &display = M5.Display;
  const int w = display.width();
  const int h = display.height();

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(top_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString("Clock since boot", w / 2, 12);

  display.setTextDatum(middle_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(3);
  display.drawString(timeText, w / 2, h / 2);

  drawFooter("B: Menu", "");
}

void enterSelectedApp() {
  if (menuIndex == 0) {
    screen = Screen::Dice;
    drawDieFace(dieValue);
    return;
  }

  screen = Screen::Clock;
  drawClock(true);
}

void returnToMenu() {
  screen = Screen::Menu;
  drawMenu();
}
}  // namespace

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  randomSeed(esp_random());
  bootMs = millis();
  M5.Display.setRotation(1);
  M5.Display.setBrightness(160);
  imuReady = M5.Imu.getType() != m5::imu_none;

  drawMenu();
}

void loop() {
  M5.update();

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
    if (M5.BtnB.wasPressed()) {
      returnToMenu();
    } else if (screen == Screen::Dice) {
      if (M5.BtnA.wasPressed() || didShake()) {
        rollDie();
      }
    } else if (screen == Screen::Clock) {
      drawClock();
    }
  }

  delay(16);
}
