#pragma once
#include "esp_partition.h"
namespace logflash {
constexpr const char *Label = "flight_ext";
extern bool connected;
inline bool begin(){return connected;}
inline const esp_partition_t *partition(){return esp_partition_find_first(1,0x82,Label);}
}
