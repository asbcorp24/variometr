#pragma once
// INA228 is on the existing primary I2C bus. Change only after verifying wiring.
constexpr unsigned char INA228_ADDRESS = 0x40;
// Set the actual shunt resistance in ohms to enable current measurement.
// Zero intentionally leaves current unknown; it does not prevent voltage reads.
constexpr float INA228_SHUNT_OHMS = 0.0f;
