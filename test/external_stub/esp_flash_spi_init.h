#pragma once
#include "esp_partition.h"
enum esp_flash_io_mode_t{SPI_FLASH_SLOWRD};
constexpr int ESP_FLASH_10MHZ=1;
struct esp_flash_spi_device_config_t{int host_id,cs_io_num;esp_flash_io_mode_t io_mode;int speed;};
esp_err_t spi_bus_add_flash_device(esp_flash_t**,const esp_flash_spi_device_config_t*);
esp_err_t spi_bus_remove_flash_device(esp_flash_t*);
esp_err_t esp_flash_init(esp_flash_t*);
esp_err_t esp_flash_read_id(esp_flash_t*,uint32_t*);
esp_err_t esp_flash_get_size(esp_flash_t*,uint32_t*);
