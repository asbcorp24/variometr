#include "external_log_flash.h"
#include "hardware_config.h"
#include "Arduino.h"
#include "driver/spi_common.h"
#include "esp_flash_spi_init.h"
extern "C" {
#include "spi_flash_chip_winbond.h"
}
#include <cassert>
#include <cstdio>
#include <cstring>
FlashSerial Serial;
static esp_flash_t device;
static esp_partition_t part;
static int stage=0,busFrees=0,removals=0,registrations=0,inits=0;
static uint32_t id=0xc22019,size=LOG_FLASH_BYTES;
static void read4(){}static void write4(){}static void program4(){}static void sector4(){}static void block4(){}
const spi_flash_chip_t esp_flash_chip_winbond={nullptr,nullptr,nullptr,read4,write4,program4,sector4,block4};
void pinMode(int pin,int mode){assert(pin==LOG_FLASH_CS&&mode==OUTPUT);}
void digitalWrite(int pin,int value){assert(pin==LOG_FLASH_CS&&value==HIGH);}
const char *esp_err_to_name(esp_err_t){return "simulated";}
esp_err_t spi_bus_initialize(int host,const spi_bus_config_t *bus,int dma){
  assert(host==SPI2_HOST&&dma==SPI_DMA_DISABLED);
  assert(bus->sclk_io_num==4&&bus->mosi_io_num==13&&bus->miso_io_num==36);
  assert(bus->quadwp_io_num==-1&&bus->quadhd_io_num==-1&&bus->data7_io_num==-1);
  return stage==1?ESP_ERR_NOT_FOUND:ESP_OK;
}
esp_err_t spi_bus_free(int host){assert(host==SPI2_HOST);++busFrees;return ESP_OK;}
esp_err_t spi_bus_add_flash_device(esp_flash_t **out,const esp_flash_spi_device_config_t *cfg){
  assert(cfg->host_id==SPI2_HOST&&cfg->cs_io_num==5&&cfg->io_mode==SPI_FLASH_SLOWRD&&cfg->speed==ESP_FLASH_10MHZ);
  if(stage==2)return ESP_ERR_NOT_FOUND;
  *out=&device;return ESP_OK;
}
esp_err_t spi_bus_remove_flash_device(esp_flash_t *chip){assert(chip==&device);++removals;return ESP_OK;}
esp_err_t esp_flash_init(esp_flash_t *chip){
  ++inits;const auto &d=*chip->chip_drv;
  assert(d.probe(chip,0xc22019)==ESP_OK&&d.probe(chip,0xef4019)==ESP_OK&&d.probe(chip,0xc22018)!=ESP_OK);
  assert(d.read==read4&&d.write==write4&&d.program_page==program4&&d.erase_sector==sector4&&d.erase_block==block4);
  esp_flash_io_mode_t io;assert(d.set_io_mode(chip)==ESP_OK&&d.get_io_mode(chip,&io)==ESP_OK&&io==SPI_FLASH_SLOWRD);
  return stage==3?ESP_ERR_NOT_FOUND:ESP_OK;
}
esp_err_t esp_flash_read_id(esp_flash_t*,uint32_t *out){*out=id;return stage==4?ESP_ERR_NOT_FOUND:ESP_OK;}
esp_err_t esp_flash_get_size(esp_flash_t*,uint32_t *out){*out=size;return ESP_OK;}
esp_err_t esp_partition_register_external(esp_flash_t *chip,size_t offset,size_t bytes,const char *label,int type,int subtype,const esp_partition_t **out){
  assert(chip==&device&&offset==0&&bytes==LOG_FLASH_BYTES&&strcmp(label,"flight_ext")==0&&type==1&&subtype==0x82);
  ++registrations;if(stage==5)return ESP_ERR_NOT_FOUND;*out=&part;return ESP_OK;
}
int main(int argc,char **argv){
  assert(argc==2);const bool winbond=strcmp(argv[1],"winbond")==0;
  stage=1;assert(!logflash::begin()&&busFrees==0);
  stage=2;assert(!logflash::begin()&&busFrees==1);
  stage=3;assert(!logflash::begin()&&removals==1&&busFrees==2);
  stage=4;assert(!logflash::begin()&&removals==2);
  stage=0;id=0xffffff;int before=inits;assert(!logflash::begin()&&inits==before);
  id=winbond?0xef4019:0xc22019;size=16*1024*1024;assert(!logflash::begin());
  size=LOG_FLASH_BYTES;stage=5;assert(!logflash::begin()&&logflash::partition()==nullptr);
  stage=0;assert(logflash::begin()&&logflash::partition()==&part);
  before=registrations;assert(logflash::begin()&&registrations==before);
  std::puts("PASS: external flash pins, JEDEC whitelist, 32 MiB required, IDF 4-byte callbacks, failure cleanup/retry, idempotent partition registration");
}
