#pragma once
// INA228 is on the existing primary I2C bus. Change only after verifying wiring.
constexpr unsigned char INA228_ADDRESS = 0x40;
// R015 (15 milliohms), as marked on the module image supplied by the user.
// Set this to zero to disable current readings if a different shunt is fitted.
constexpr float INA228_SHUNT_OHMS = 0.015f;
// User-specified nominal capacity; voltage-based SOC does not integrate mAh yet.
constexpr unsigned BATTERY_CAPACITY_MAH = 10000;
// User confirmed 3.7 V nominal: one Li-ion cell in series (4.2 V fully charged).
constexpr int BATTERY_DEFAULT_CELLS = 1;

// External 3.3 V / 256 Mbit SPI NOR, dedicated HSPI (SPI2); TFT uses VSPI.
constexpr int LOG_FLASH_SCK = 4;
constexpr int LOG_FLASH_MOSI = 13;
constexpr int LOG_FLASH_MISO = 36; // VP; input-only is suitable for MISO.
constexpr int LOG_FLASH_CS = 5; // Pull up with 10k to 3V3, including at boot.
constexpr unsigned LOG_FLASH_BYTES = 32u * 1024u * 1024u;
