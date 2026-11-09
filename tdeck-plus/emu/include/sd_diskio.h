#pragma once
// Emulator stand-in for the Arduino SD driver's sd_diskio.h: the early card wake-up in setup() (SD before the LCD)
// finds no real card here (ff_sd_initialize fails), and the emulated SD keeps working through the SD / FS layer; the
// mount diagnosis (sdDiagnose) reads the card through diskio.h (emu/stubs).
#include <stdint.h>
class SPIClass;
inline uint8_t sdcard_init(uint8_t,SPIClass*,int){return 0;}
inline uint8_t sdcard_uninit(uint8_t){return 0;}
inline unsigned char ff_sd_initialize(uint8_t){return 1;}
