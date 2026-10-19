#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_DATA_SPIFFS 0x82
#define ESP_OK 0
struct esp_partition_t{size_t size=4096;};
extern bool partitionErased;
inline const esp_partition_t *esp_partition_find_first(int,int,const char *){static esp_partition_t p;return &p;}
inline int esp_partition_read(const esp_partition_t *,size_t,void *buffer,size_t n){memset(buffer,partitionErased?0xff:0,n);return 0;}
