#pragma once
// Only what the companion factory reset uses: erase the whole NVS (the emulator keeps it in DATA/nvs.json).
#include "esp_system.h"
int emuNvsErase();
inline esp_err_t nvs_flash_init() { return ESP_OK; }
inline esp_err_t nvs_flash_deinit() { return ESP_OK; }
inline esp_err_t nvs_flash_erase() { return emuNvsErase() ? ESP_FAIL : ESP_OK; }
