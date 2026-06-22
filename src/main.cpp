#include <Arduino.h>
#include <M5Unified.h>

namespace {
constexpr uint32_t kRollCooldownMs = 650;
constexpr float kShakeThreshold = 1.6f;

uint32_t lastRollMs = 0;
int value = 1;
bool imuReady = false;

void drawDieFace(int n) {
  auto &display = M5.Display;
  const int w = display.width();
  const int h = display.height();
  const int size = min(w, h) - 28;
  const int x = (w - size) / 2;
  const int y = (h - size) / 2 + 8;
  const int r = max(5, size / 12);
  const int inset = size / 4;
  const int mid = size / 2;

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(1);
  display.drawString("Shake or press BtnA", w / 2, 6);

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
}

void rollDie() {
  const uint32_t now = millis();
  if (now - lastRollMs < kRollCooldownMs) return;

  lastRollMs = now;
  value = random(1, 7);
  drawDieFace(value);
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
}  // namespace

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  randomSeed(esp_random());
  M5.Display.setRotation(1);
  M5.Display.setBrightness(160);
  imuReady = M5.Imu.getType() != m5::imu_none;

  drawDieFace(value);
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed() || didShake()) {
    rollDie();
  }

  delay(16);
}
