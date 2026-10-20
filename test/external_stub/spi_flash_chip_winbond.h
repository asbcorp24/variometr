#pragma once
#include "esp_flash_spi_init.h"
struct spi_flash_chip_t {
  esp_err_t (*probe)(esp_flash_t*,uint32_t);
  esp_err_t (*set_io_mode)(esp_flash_t*);
  esp_err_t (*get_io_mode)(esp_flash_t*,esp_flash_io_mode_t*);
  // Markers ensure we retain the IDF driver's 4-byte I/O callbacks.
  void (*read)();void (*write)();void (*program_page)();void (*erase_sector)();void (*erase_block)();
};
extern const spi_flash_chip_t esp_flash_chip_winbond;
