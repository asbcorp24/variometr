#pragma once
#include <cstdint>
#include <cstddef>
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_ERR_NOT_FOUND=1,ESP_ERR_NOT_SUPPORTED=2,ESP_ERR_INVALID_SIZE=3;
constexpr int ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_SUBTYPE_DATA_SPIFFS=0x82;
struct spi_flash_chip_t;
struct esp_flash_t{const spi_flash_chip_t *chip_drv=nullptr;};
struct esp_partition_t{size_t size=32*1024*1024;};
const char *esp_err_to_name(esp_err_t);
esp_err_t esp_partition_register_external(esp_flash_t*,size_t,size_t,const char*,int,int,const esp_partition_t**);
