#pragma once
#include <esp_partition.h>
namespace logflash {
constexpr const char *Label = "flight_ext";
bool begin();
const esp_partition_t *partition();
}
