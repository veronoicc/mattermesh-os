#pragma once
#include "SPI.h"
#include "ff.h"
static inline uint8_t sdcard_init(uint8_t,SPIClass*,int){return 0;}
static inline uint8_t sdcard_uninit(uint8_t){return 0;}
