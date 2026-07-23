#pragma once

#include <Arduino.h>

namespace Pet {

void begin();
void draw(bool force = false);
void interact();
void stop();

}  // namespace Pet