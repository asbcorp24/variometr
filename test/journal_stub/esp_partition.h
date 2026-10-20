#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_DATA_SPIFFS 0x82
#define ESP_OK 0
struct esp_partition_t{size_t size=32u*1024u*1024u;};
extern bool partitionErased;
extern size_t partitionDirtyOffset, highestPartitionRead;
inline const esp_partition_t *esp_partition_find_first(int,int,const char *){static esp_partition_t p;return &p;}
inline int esp_partition_read(const esp_partition_t *,size_t offset,void *buffer,size_t n){
  memset(buffer,partitionErased?0xff:0,n);
  if(partitionDirtyOffset>=offset&&partitionDirtyOffset-offset<n)static_cast<uint8_t*>(buffer)[partitionDirtyOffset-offset]=0;
  if(offset+n>highestPartitionRead)highestPartitionRead=offset+n;
  return 0;
}
