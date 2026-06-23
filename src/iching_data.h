#pragma once

#include <stdint.h>

struct IChingHexagram {
  uint8_t id;
  const char *name;
  const char *lines;
  const char *upper;
  const char *lower;
  const char *judgement;
  const char *summary;
  const char *transformedSummary;
  const char *keywords;
};

const IChingHexagram *getHexagramById(int id);
const char *getTrigramNameByBits(uint8_t bits);
const char *getTrigramMnemonicByBits(uint8_t bits);
