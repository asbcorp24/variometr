#pragma once
// INA228 is on the existing primary I2C bus. Change only after verifying wiring.
constexpr unsigned char INA228_ADDRESS = 0x40;
// R015 (15 milliohms), as marked on the module image supplied by the user.
// Set this to zero to disable current readings if a different shunt is fitted.
constexpr float INA228_SHUNT_OHMS = 0.015f;
