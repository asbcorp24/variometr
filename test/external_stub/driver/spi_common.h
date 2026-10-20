#pragma once
#include "esp_partition.h"
constexpr int SPI2_HOST=1,SPI_DMA_DISABLED=0;
struct spi_bus_config_t{int mosi_io_num,miso_io_num,sclk_io_num,quadwp_io_num,quadhd_io_num,data4_io_num,data5_io_num,data6_io_num,data7_io_num;};
esp_err_t spi_bus_initialize(int,const spi_bus_config_t*,int);
esp_err_t spi_bus_free(int);
