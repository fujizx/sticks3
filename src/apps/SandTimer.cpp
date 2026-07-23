#include "SandTimer.h"

#include <algorithm>

#include <M5Unified.h>

namespace SandTimer {
namespace {
constexpr uint32_t kFrameMs = 45;
constexpr uint8_t kDoneVolume = 77;
constexpr float kInvertThreshold = -0.70f;
constexpr float kLeanDeadzone = 0.08f;
constexpr int kSpriteX = 9;
constexpr int kSpriteY = 18;
constexpr int kSpriteW = 118;
constexpr int kSpriteH = 220;
constexpr int kPanelX = 22;
constexpr int kPanelY = 29;
constexpr int kPanelW = 74;
constexpr int kPanelH = 154;
constexpr int kTopY = 38;
constexpr int kWaistY = 106;
constexpr int kBottomY = 174;
constexpr int kCenterX = 59;
constexpr int kHalf = 31;
constexpr int kButtonX = 36;
constexpr int kButtonY = 195;
constexpr int kButtonW = 46;
constexpr int kButtonH = 11;
constexpr int kMaxParticles = 280;
constexpr int kScale = 256;
constexpr uint16_t kMinutes[] = {1, 3, 5, 10, 15};
constexpr const char *kPromptItems[] = {"Reset", "Exit"};

struct Particle {
  int32_t x = 0;
  int32_t y = 0;
  int32_t vx = 0;
  int32_t vy = 0;
  bool active = false;
};

M5Canvas canvas(&M5.Display);
Particle particles[kMaxParticles];
bool occupied[kSpriteH][kSpriteW] = {};
uint32_t startMs = 0;
uint32_t durationMs = 5UL * 60UL * 1000UL;
uint32_t lastDrawMs = 0;
bool invertedLatch = false;
bool active = false;
bool finished = false;
bool gravityReady = false;
float baseAx = 0.0f;
float baseAy = 1.0f;
float baseAz = 0.0f;
float gravityX = 0.0f;
float gravityY = 1.0f;

bool readAccel(float &ax, float &ay, float &az) {
  if (M5.Imu.getType() == m5::imu_none) return false;
  if (!M5.Imu.update()) return false;
  const auto data = M5.Imu.getImuData();
  ax = data.accel.x;
  ay = data.accel.y;
  az = data.accel.z;
  return true;
}

float applyLeanDeadzone(float value) {
  if (fabsf(value) < kLeanDeadzone) return 0.0f;
  const float sign = value > 0.0f ? 1.0f : -1.0f;
  return sign * ((fabsf(value) - kLeanDeadzone) / (1.0f - kLeanDeadzone));
}

uint16_t colorFor(int x, int y, bool highlight = false) {
  constexpr uint16_t kGoldDark = 0x9340;
  constexpr uint16_t kGoldMid = 0xD5A0;
  constexpr uint16_t kGoldBright = 0xFFE0;
  const uint8_t grain = static_cast<uint8_t>((x * 31 + y * 17 + (x ^ y) * 9) & 0x0F);
  if (highlight || grain < 3) return kGoldBright;
  if (grain < 11) return kGoldMid;
  return kGoldDark;
}

int halfWidthAt(int y) {
  if (y < kTopY || y > kBottomY) return -1;
  if (y <= kWaistY) return map(y, kTopY, kWaistY, kHalf, 2);
  return map(y, kWaistY, kBottomY, 2, kHalf);
}

bool inside(int x, int y, int inset = 0) {
  const int half = halfWidthAt(y);
  if (half < 0) return false;
  const int edge = max(0, half - inset);
  return x >= kCenterX - edge && x <= kCenterX + edge;
}

void rebuildOccupancy() {
  memset(occupied, 0, sizeof(occupied));
  for (const auto &particle : particles) {
    if (!particle.active) continue;
    const int x = constrain(static_cast<int>(particle.x / kScale), 0, kSpriteW - 1);
    const int y = constrain(static_cast<int>(particle.y / kScale), 0, kSpriteH - 1);
    if (inside(x, y, 1)) occupied[y][x] = true;
  }
}

bool canMoveTo(int32_t nx, int32_t ny, int ignoreX, int ignoreY) {
  const int x = constrain(static_cast<int>(nx / kScale), 0, kSpriteW - 1);
  const int y = constrain(static_cast<int>(ny / kScale), 0, kSpriteH - 1);
  if (!inside(x, y, 1)) return false;
  if (x == ignoreX && y == ignoreY) return true;
  return !occupied[y][x];
}

void resetParticles() {
  int index = 0;
  memset(occupied, 0, sizeof(occupied));
  for (auto &particle : particles) particle.active = false;

  for (int y = kTopY + 4; y < kWaistY - 3 && index < kMaxParticles; ++y) {
    const int half = max(0, halfWidthAt(y) - 4);
    for (int x = kCenterX - half; x <= kCenterX + half && index < kMaxParticles; x += 2) {
      if (((x + y) & 1) != 0 || !inside(x, y, 3)) continue;
      particles[index].x = (x * kScale) + random(0, kScale);
      particles[index].y = (y * kScale) + random(0, kScale);
      particles[index].vx = 0;
      particles[index].vy = 0;
      particles[index].active = true;
      ++index;
    }
  }
  rebuildOccupancy();
}

void simulate(float gxNorm, float gyNorm) {
  rebuildOccupancy();
  const int32_t gx = static_cast<int32_t>(gxNorm * 44.0f);
  const int32_t gy = static_cast<int32_t>(max(gyNorm, 0.18f) * 50.0f);

  for (auto &particle : particles) {
    if (!particle.active) continue;
    const int oldX = constrain(static_cast<int>(particle.x / kScale), 0, kSpriteW - 1);
    const int oldY = constrain(static_cast<int>(particle.y / kScale), 0, kSpriteH - 1);
    occupied[oldY][oldX] = false;

    particle.vx = constrain(particle.vx + gx, -210, 210);
    particle.vy = constrain(particle.vy + gy, -60, 250);
    const int32_t nextX = particle.x + particle.vx;
    const int32_t nextY = particle.y + particle.vy;

    if (canMoveTo(nextX, nextY, oldX, oldY)) {
      particle.x = nextX;
      particle.y = nextY;
    } else {
      const int32_t slide = gxNorm >= 0.0f ? kScale : -kScale;
      bool moved = false;
      const int32_t tries[4][2] = {
          {particle.x + slide, particle.y + abs(particle.vy)},
          {particle.x - slide, particle.y + abs(particle.vy)},
          {particle.x + slide, particle.y},
          {particle.x, particle.y + max<int32_t>(kScale / 2, abs(particle.vy) / 2)},
      };
      for (const auto &target : tries) {
        if (!canMoveTo(target[0], target[1], oldX, oldY)) continue;
        particle.x = target[0];
        particle.y = target[1];
        particle.vx /= 2;
        particle.vy = max<int32_t>(20, particle.vy / 2);
        moved = true;
        break;
      }
      if (!moved) {
        particle.vx = 0;
        particle.vy = 0;
      }
    }

    const int newX = constrain(static_cast<int>(particle.x / kScale), 0, kSpriteW - 1);
    const int newY = constrain(static_cast<int>(particle.y / kScale), 0, kSpriteH - 1);
    if (inside(newX, newY, 1)) occupied[newY][newX] = true;
  }
}

template <typename Gfx>
void drawDevice(Gfx &gfx) {
  constexpr uint16_t kBody = 0x18C3;
  constexpr uint16_t kBodyEdge = 0x39E7;
  constexpr uint16_t kScreen = 0x010B;
  constexpr uint16_t kScreenEdge = 0x3D7F;
  constexpr uint16_t kScreenInner = 0x00A8;
  constexpr uint16_t kButton = 0x04FF;
  constexpr uint16_t kButtonEdge = 0x0278;

  gfx.fillScreen(TFT_BLACK);
  gfx.fillRoundRect(3, 2, kSpriteW - 6, kSpriteH - 4, 8, kBody);
  gfx.drawRoundRect(3, 2, kSpriteW - 6, kSpriteH - 4, 8, kBodyEdge);
  gfx.fillRect(kPanelX, kPanelY, kPanelW, kPanelH, kScreen);
  gfx.drawRect(kPanelX, kPanelY, kPanelW, kPanelH, kScreenEdge);
  gfx.drawRect(kPanelX + 2, kPanelY + 2, kPanelW - 4, kPanelH - 4, kScreenInner);
  gfx.fillRoundRect(kButtonX, kButtonY, kButtonW, kButtonH, 4, kButton);
  gfx.drawRoundRect(kButtonX, kButtonY, kButtonW, kButtonH, 4, kButtonEdge);
}

template <typename Gfx>
void drawDiamonds(Gfx &gfx) {
  constexpr uint16_t kGlass = 0xFFE0;
  gfx.drawLine(kCenterX, kTopY, kCenterX - kHalf, kWaistY, kGlass);
  gfx.drawLine(kCenterX - kHalf, kWaistY, kCenterX, kBottomY, kGlass);
  gfx.drawLine(kCenterX, kBottomY, kCenterX + kHalf, kWaistY, kGlass);
  gfx.drawLine(kCenterX + kHalf, kWaistY, kCenterX, kTopY, kGlass);
  gfx.drawLine(kCenterX - 2, kWaistY, kCenterX + 2, kWaistY, kGlass);
}

template <typename Gfx>
void drawParticles(Gfx &gfx) {
  for (const auto &particle : particles) {
    if (!particle.active) continue;
    const int x = static_cast<int>(particle.x / kScale);
    const int y = static_cast<int>(particle.y / kScale);
    if (!inside(x, y, 1)) continue;
    const bool bright = ((x * 7 + y * 5) & 0x0F) < 4;
    gfx.drawPixel(x, y, colorFor(x, y, bright));
    if (((x + y) & 0x07) == 0 && inside(x + 1, y, 1)) {
      gfx.drawPixel(x + 1, y, colorFor(x + 1, y));
    }
  }
}

void calibrateGravity() {
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  float sumAx = 0.0f;
  float sumAy = 0.0f;
  float sumAz = 0.0f;
  int samples = 0;
  gravityReady = false;
  delay(80);
  for (int i = 0; i < 18; ++i) {
    if (readAccel(ax, ay, az)) {
      sumAx += ax;
      sumAy += ay;
      sumAz += az;
      ++samples;
    }
    delay(10);
  }
  if (samples > 0) {
    baseAx = sumAx / samples;
    baseAy = sumAy / samples;
    baseAz = sumAz / samples;
    gravityReady = true;
  }
}

}  // namespace

int durationCount() {
  return sizeof(kMinutes) / sizeof(kMinutes[0]);
}

void drawSelect(int selectedIndex) {
  M5.Display.setRotation(0);
  auto &display = M5.Display;
  const int w = display.width();

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString("SandTimer", w / 2, 18);

  for (int i = 0; i < durationCount(); ++i) {
    const int y = 54 + i * 34;
    const bool selected = i == selectedIndex;
    const uint16_t bg = selected ? 0xFDA0 : TFT_DARKGREY;
    const uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;
    display.fillRoundRect(18, y, w - 36, 27, 6, bg);
    display.setTextDatum(middle_center);
    display.setTextColor(fg, bg);
    display.setTextSize(2);
    display.drawString(String(kMinutes[i]) + " min", w / 2, y + 14);
  }

  display.setTextDatum(bottom_center);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
}

void start(int selectedIndex) {
  selectedIndex = constrain(selectedIndex, 0, durationCount() - 1);
  durationMs = static_cast<uint32_t>(kMinutes[selectedIndex]) * 60UL * 1000UL;
  startMs = millis();
  lastDrawMs = 0;
  gravityX = 0.0f;
  gravityY = 1.0f;
  invertedLatch = false;
  active = true;
  finished = false;
  resetParticles();
  M5.Display.setRotation(0);
  M5.Display.fillScreen(TFT_BLACK);
  calibrateGravity();
}

bool drawRun(bool force) {
  const uint32_t now = millis();
  if (!force && now - lastDrawMs < kFrameMs) return false;
  lastDrawMs = now;

  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  if (readAccel(ax, ay, az)) {
    const float rawGravityX = applyLeanDeadzone(constrain((baseAy - ay) * 1.55f, -1.0f, 1.0f));
    const float rawGravityY = constrain(-ax, 0.0f, 1.0f);
    gravityX = gravityX * 0.62f + rawGravityX * 0.38f;
    gravityY = gravityY * 0.70f + rawGravityY * 0.30f;
    bool isInverted = false;
    if (gravityReady) {
      const float baseMag = sqrtf(baseAx * baseAx + baseAy * baseAy + baseAz * baseAz);
      const float mag = sqrtf(ax * ax + ay * ay + az * az);
      if (baseMag > 0.01f && mag > 0.01f) {
        const float dot = (ax * baseAx + ay * baseAy + az * baseAz) / (baseMag * mag);
        isInverted = dot < kInvertThreshold;
      }
    }
    if (isInverted && !invertedLatch) {
      invertedLatch = true;
      return true;
    }
    if (!isInverted) invertedLatch = false;
  }

  const uint32_t elapsed = now - startMs;
  const uint32_t remainingSec = (durationMs > elapsed) ? (durationMs - elapsed) / 1000 : 0;
  if (!finished && elapsed >= durationMs) {
    finished = true;
    active = false;
    M5.Speaker.setVolume(kDoneVolume);
    M5.Speaker.tone(1175, 90);
    delay(120);
    M5.Speaker.tone(1568, 100);
  }

  simulate(gravityX, gravityY);

  if (!canvas.width()) {
    canvas.setColorDepth(16);
    canvas.createSprite(kSpriteW, kSpriteH);
  }
  canvas.fillScreen(TFT_BLACK);
  drawDevice(canvas);
  drawParticles(canvas);
  drawDiamonds(canvas);
  canvas.pushSprite(kSpriteX, kSpriteY);

  auto &display = M5.Display;
  display.setRotation(0);
  display.fillRect(0, 0, display.width(), 18, TFT_BLACK);
  display.setTextDatum(top_center);
  display.setTextColor(finished ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
  display.setTextSize(1);
  char remainText[12];
  snprintf(remainText, sizeof(remainText), "%02lu:%02lu",
           static_cast<unsigned long>(remainingSec / 60),
           static_cast<unsigned long>(remainingSec % 60));
  display.drawString(remainText, display.width() / 2, 6);
  return false;
}

void drawPrompt(int promptIndex) {
  M5.Display.setRotation(0);
  auto &display = M5.Display;
  const int w = display.width();

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(top_center);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString("SandTimer", w / 2, 24);
  display.setTextSize(1);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.drawString("Reset or exit?", w / 2, 52);

  for (int i = 0; i < 2; ++i) {
    const int y = 92 + i * 48;
    const bool selected = i == promptIndex;
    const uint16_t bg = selected ? 0xFDA0 : TFT_DARKGREY;
    const uint16_t fg = selected ? TFT_BLACK : TFT_WHITE;
    display.fillRoundRect(16, y, w - 32, 34, 6, bg);
    display.setTextDatum(middle_center);
    display.setTextColor(fg, bg);
    display.setTextSize(2);
    display.drawString(kPromptItems[i], w / 2, y + 17);
  }
}

void stop() {
  active = false;
}

}  // namespace SandTimer
