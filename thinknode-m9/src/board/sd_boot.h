#pragma once
/* Boot-time bring-up of the microSD on a bus shared with the display and the radio (T-Deck Plus, ThinkNode M9).
 *
 * The Arduino SD driver gives a card one shot: CMD0 three times, then ACMD41 for at most 1 s, always at 400 kHz. A card
 * that is still finishing its internal power-up, or one left in the middle of a transfer by the firmware that ran before
 * the reset (its supply never dropped), fails that shot and the device says "No SD card found". WADAMESH answers with
 * repeated SD.begin calls and longer pauses. Here the card is brought up step by step instead:
 *
 *   1. powerCycle(): the peripheral rail on. With offMs > 0 it first cuts the rail with every bus line LOW; NOT used
 *      at boot: on the T-Deck Plus a 250 ms cut made 1 boot in 3 crash right after the keyboard came up (ipc1 stack
 *      canary, USB resets only; 0 in 12 without the cut, tested 2026-11-09 through the lab);
 *   2. wake(): 80+ clocks with CS high, CMD12 + flush clocks to abort a transfer the card may still be in, CMD0 until
 *      the card answers "idle" (SPI mode). Done before the display speaks: an SPI-mode card ignores traffic while its
 *      CS is high, a card still in SD mode can take display bytes as SD commands;
 *   3. ready(): CMD8 then ACMD41 for up to 3 s (spec: 1 s; slow and large cards take longer), CMD0 again on an error.
 *      A card that has answered once mounts at the first SD.begin.
 *
 * Every step is logged on the serial (BOOT_SD_STEP) and the result says where a failing card stopped. */
#include <Arduino.h>
#include <SPI.h>

namespace sdboot {

enum Stage : uint8_t { STAGE_OK = 0, STAGE_NO_ANSWER, STAGE_BAD_CMD8, STAGE_INIT_TIMEOUT, STAGE_INIT_ERROR, STAGE_SKIPPED };

struct Result {
  Stage stage = STAGE_SKIPPED;
  bool sdhc = false;
  uint16_t cmd0Tries = 0;
  uint32_t initMs = 0;
};

inline const char *stageName(Stage s) {
  switch (s) {
    case STAGE_OK: return "ok";
    case STAGE_NO_ANSWER: return "no_answer";
    case STAGE_BAD_CMD8: return "bad_cmd8";
    case STAGE_INIT_TIMEOUT: return "init_timeout";
    case STAGE_INIT_ERROR: return "init_error";
    default: return "skipped";
  }
}

#ifndef MM_EMULATOR
static constexpr uint32_t kWakeHz = 400000;

inline void flush(SPIClass &spi, int cs, int bytes) {
  digitalWrite(cs, HIGH);
  for (int i = 0; i < bytes; i++) spi.transfer(0xFF);
}

/* One command with CS held low; R1 returned (0xFF: no answer). extra receives the 4 bytes of an R3/R7 answer. */
inline uint8_t command(SPIClass &spi, int cs, uint8_t cmd, uint32_t arg, uint8_t crc, uint32_t *extra = nullptr) {
  digitalWrite(cs, HIGH);
  spi.transfer(0xFF);
  digitalWrite(cs, LOW);
  /* Wait while the card holds MISO low (busy), except for CMD0 and CMD12, which must go out whatever it is doing. */
  if (cmd != 0 && cmd != 12) {
    uint32_t t0 = millis();
    while (spi.transfer(0xFF) != 0xFF && millis() - t0 < 300) {}
  }
  const uint8_t packet[6] = {uint8_t(0x40 | cmd), uint8_t(arg >> 24), uint8_t(arg >> 16), uint8_t(arg >> 8), uint8_t(arg), crc};
  for (uint8_t b : packet) spi.transfer(b);
  if (cmd == 12) spi.transfer(0xFF); /* stuff byte after STOP_TRANSMISSION */
  uint8_t r1 = 0xFF;
  for (int i = 0; i < 16 && (r1 & 0x80); i++) r1 = spi.transfer(0xFF);
  if (extra) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) v = (v << 8) | spi.transfer(0xFF);
    *extra = v;
  }
  digitalWrite(cs, HIGH);
  spi.transfer(0xFF);
  return r1;
}

/* Cuts the peripheral rail with every bus line driven LOW, so the card cannot stay half-powered through its inputs. */
inline void powerCycle(int rail, int onLevel, std::initializer_list<int> lowPins, uint32_t offMs) {
  pinMode(rail, OUTPUT);
  if (offMs) {
    for (int pin : lowPins) {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
    }
    digitalWrite(rail, onLevel == HIGH ? LOW : HIGH);
    delay(offMs);
  }
  digitalWrite(rail, onLevel);
  delay(20); /* supply ramp + the 1 ms the card needs before its first clock */
}

/* Puts the card in SPI mode (idle). Every other chip select on the bus must be HIGH. */
inline Result wake(SPIClass &spi, int cs, int maxTries = 25) {
  Result r;
  spi.beginTransaction(SPISettings(kWakeHz, MSBFIRST, SPI_MODE0));
  flush(spi, cs, 16);
  uint8_t r1 = 0xFF;
  for (int i = 0; i < maxTries; i++) {
    r.cmd0Tries = uint16_t(i + 1);
    r1 = command(spi, cs, 0, 0, 0x95);
    if (r1 == 0x01) break;
    /* A card left mid-read keeps streaming data: stop it, clock out what is left, try again. */
    command(spi, cs, 12, 0, 0x61);
    flush(spi, cs, 64);
    delay(i < 5 ? 5 : 20);
  }
  spi.endTransaction();
  r.stage = r1 == 0x01 ? STAGE_OK : STAGE_NO_ANSWER;
  Serial.printf("BOOT_SD_STEP wake=%s cmd0_tries=%u r1=0x%02X\n", stageName(r.stage), r.cmd0Tries, r1);
  return r;
}

/* Finishes the card's own initialization (ACMD41) so the next SD.begin finds a card that answers at once. */
inline Result ready(SPIClass &spi, int cs, uint32_t timeoutMs = 3000) {
  Result r = wake(spi, cs, 10);
  if (r.stage != STAGE_OK) return r;
  const uint32_t t0 = millis();
  spi.beginTransaction(SPISettings(kWakeHz, MSBFIRST, SPI_MODE0));
  uint32_t r7 = 0;
  uint8_t r1 = command(spi, cs, 8, 0x1AA, 0x87, &r7);
  uint32_t hcs = 0;
  if (r1 == 0x01) {
    if ((r7 & 0xFFF) != 0x1AA) {
      spi.endTransaction();
      r.stage = STAGE_BAD_CMD8;
      Serial.printf("BOOT_SD_STEP ready=%s r7=0x%08lX\n", stageName(r.stage), (unsigned long)r7);
      return r;
    }
    hcs = 1UL << 30;
  } else if (!(r1 & 0x04)) {
    /* Not "illegal command" (an SD v1 card): the card is confused, start it over once. */
    command(spi, cs, 0, 0, 0x95);
  }
  bool restarted = false;
  for (;;) {
    command(spi, cs, 55, 0, 0x65);
    r1 = command(spi, cs, 41, hcs, hcs ? 0x77 : 0xE5);
    if (r1 == 0x00) break;
    if (millis() - t0 > timeoutMs) break;
    if (r1 != 0x01 && !restarted) { /* an error, not "still busy": one fresh start */
      restarted = true;
      command(spi, cs, 0, 0, 0x95);
      if (hcs) command(spi, cs, 8, 0x1AA, 0x87, &r7);
    }
    delay(10);
  }
  if (r1 == 0x00 && hcs) {
    uint32_t ocr = 0;
    if (command(spi, cs, 58, 0, 0xFD, &ocr) == 0x00) r.sdhc = (ocr & (1UL << 30)) != 0;
  }
  flush(spi, cs, 8);
  spi.endTransaction();
  r.initMs = millis() - t0;
  r.stage = r1 == 0x00 ? STAGE_OK : (r1 == 0x01 ? STAGE_INIT_TIMEOUT : STAGE_INIT_ERROR);
  Serial.printf("BOOT_SD_STEP ready=%s ms=%lu sdhc=%d r1=0x%02X\n", stageName(r.stage), (unsigned long)r.initMs, r.sdhc, r1);
  return r;
}
#else
inline void powerCycle(int, int, std::initializer_list<int>, uint32_t) {}
inline Result wake(SPIClass &, int, int = 25) { return Result(); }
inline Result ready(SPIClass &, int, uint32_t = 3000) { return Result(); }
#endif

}  // namespace sdboot
