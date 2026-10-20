#include "external_log_flash.h"
#include "hardware_config.h"
#include <Arduino.h>
#include <driver/spi_common.h>
#include <esp_flash_spi_init.h>
extern "C" {
#include <spi_flash_chip_winbond.h>
}

namespace logflash {
namespace {
esp_flash_t *chip = nullptr;
const esp_partition_t *storage = nullptr;
bool busOwned = false;
spi_flash_chip_t driver;
// Both confirmed 3 V families implement the dedicated 4-byte commands used by
// IDF's Winbond driver (read 13h, program 12h, erase 21h/DCh). The stock MXIC
// driver in IDF 4.4 only uses 24-bit addressing. Never change global address mode.
esp_err_t IRAM_ATTR probe(esp_flash_t *, uint32_t id) {
  return id == 0xc22019 || id == 0xef4019 ? ESP_OK : ESP_ERR_NOT_FOUND;
}
esp_err_t IRAM_ATTR singleIo(esp_flash_t *) { return ESP_OK; }
esp_err_t IRAM_ATTR getIo(esp_flash_t *, esp_flash_io_mode_t *mode) {
  *mode = SPI_FLASH_SLOWRD; return ESP_OK;
}
bool fail(const char *step, esp_err_t err) {
  Serial.printf("External log flash: %s failed: %s\n", step, esp_err_to_name(err));
  if (chip) { spi_bus_remove_flash_device(chip); chip = nullptr; }
  if (busOwned) { spi_bus_free(SPI2_HOST); busOwned = false; }
  return false;
}
}
bool begin() {
  if (storage) return true;
  pinMode(LOG_FLASH_CS, OUTPUT); digitalWrite(LOG_FLASH_CS, HIGH);
  spi_bus_config_t bus = {};
  bus.mosi_io_num = LOG_FLASH_MOSI; bus.miso_io_num = LOG_FLASH_MISO;
  bus.sclk_io_num = LOG_FLASH_SCK; bus.quadwp_io_num = bus.quadhd_io_num = -1;
  bus.data4_io_num = bus.data5_io_num = bus.data6_io_num = bus.data7_io_num = -1;
  esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_DISABLED);
  if (err != ESP_OK) return fail("SPI2 init", err);
  busOwned = true;
  esp_flash_spi_device_config_t config = {};
  config.host_id = SPI2_HOST; config.cs_io_num = LOG_FLASH_CS;
  config.io_mode = SPI_FLASH_SLOWRD; config.speed = ESP_FLASH_10MHZ;
  err = spi_bus_add_flash_device(&chip, &config);
  if (err != ESP_OK) return fail("add chip", err);
  driver = esp_flash_chip_winbond;
  driver.probe = probe; driver.set_io_mode = singleIo; driver.get_io_mode = getIo;
  chip->chip_drv = &driver;
  uint32_t id = 0, size = 0;
  err = esp_flash_read_id(chip, &id);
  if (err != ESP_OK || probe(chip, id) != ESP_OK) return fail("JEDEC ID", err != ESP_OK ? err : ESP_ERR_NOT_SUPPORTED);
  err = esp_flash_init(chip);
  if (err != ESP_OK) return fail("identify/init", err);
  err = esp_flash_get_size(chip, &size);
  if (err != ESP_OK || size != LOG_FLASH_BYTES) return fail("expected 32 MiB", err != ESP_OK ? err : ESP_ERR_INVALID_SIZE);
  err = esp_partition_register_external(chip, 0, size, Label, ESP_PARTITION_TYPE_DATA,
                                        ESP_PARTITION_SUBTYPE_DATA_SPIFFS, &storage);
  if (err != ESP_OK) { storage = nullptr; return fail("register partition", err); }
  Serial.printf("External log flash: JEDEC=%06lx, %lu bytes, SPI2 SCK=%d MOSI=%d MISO=%d CS=%d\n",
                static_cast<unsigned long>(id), static_cast<unsigned long>(size), LOG_FLASH_SCK, LOG_FLASH_MOSI, LOG_FLASH_MISO, LOG_FLASH_CS);
  return true;
}
const esp_partition_t *partition() { return storage; }
}
