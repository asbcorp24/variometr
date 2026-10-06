#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_BME280.h>
#include "diagnostic_display.h"
#include <Preferences.h>
#include <Bounce2.h>
#include <cstdlib>

constexpr uint8_t SDA_PIN = 21, SCL_PIN = 22, BUZZER_PIN = 14;
constexpr bool SOUND_ENABLED_ON_BOOT = true;
constexpr uint8_t HMC_SDA_PIN = 16, HMC_SCL_PIN = 17;
TwoWire magnetWire(1);
bool magnetBusReady = false;
Adafruit_BMP280 bmp;
Adafruit_BME280 bme;
enum class Barometer { None, BMP, BME };
Barometer barometer = Barometer::None;
bool streaming = true, pwmAttached = false;
bool bmiReady = false;
constexpr uint8_t BMI_ADDRESS = 0x69;
bool hmcReady = false;
constexpr uint8_t HMC_ADDRESS = 0x1E;
bool qmcReady = false;
constexpr uint8_t QMC_ADDRESS = 0x0D;
int soundStep = -1;
uint32_t soundTime = 0, sampleTime = 0;
vario::Filter flight;
Preferences settings;
bool settingsReady = false;
bool audioEnabled = SOUND_ENABLED_ON_BOOT;
uint8_t currentPage = 0; // 0 variometer, 1 compass, 2 diagnostics.
compass::Heading magneticHeading;
uint32_t compassTime = 0;
uint8_t baroAddress = 0;
uint32_t baroTime = 0, displayTime = 0, audioPhase = 0;
uint32_t playingFrequency = 0;
vario::Sound audioMode = vario::Sound::Silent;
float currentPressure = NAN, currentTemperature = NAN;
// BACK/UP use internal pullups; DOWN/OK require external 10k pullups to 3V3.
constexpr uint8_t buttonPins[4] = {32, 33, 34, 35};
Bounce2::Button buttonInput[4];
buttons::Gestures buttonState[4];
buttons::Menu menu;
char menuNotice[100] = {};
int geometryRotation = -1;
battery::State batteryState;
uint32_t batteryTime = 0;

bool readBytes(uint8_t address, uint8_t reg, uint8_t *data, size_t count, TwoWire &bus = Wire) {
  bus.beginTransmission(address);
  bus.write(reg);
  if (bus.endTransmission(false) != 0) return false;
  if (bus.requestFrom(address, count) != count) return false;
  for (size_t i = 0; i < count; ++i) data[i] = bus.read();
  return true;
}

void batteryTick(uint32_t now, bool force = false) {
  batteryState.expire(now);
  const uint32_t interval = batteryState.mode == battery::Mode::Demo ? 5000 : 1000;
  if (!force && uint32_t(now - batteryTime) < interval) return;
  batteryTime = now;
  const battery::Mode previous = batteryState.mode;
  Wire.beginTransmission(battery::Address);
  const uint8_t result = Wire.endTransmission();
  if (result == 2) batteryState.missing(); // Address NACK: module absent/unpowered.
  else if (result != 0) batteryState.error();
  else {
    uint8_t version[2], data[4];
    if (!readBytes(battery::Address, 0x08, version, 2) ||
        !readBytes(battery::Address, 0x02, data, 4)) batteryState.error();
    else batteryState.update((uint16_t(version[0]) << 8) | version[1],
                             (uint16_t(data[0]) << 8) | data[1],
                             (uint16_t(data[2]) << 8) | data[3], millis());
  }
  if (force || previous != batteryState.mode) {
    if (batteryState.mode == battery::Mode::Demo) Serial.println("Battery: DEMO 100%, MAX17048 absent at 0x36");
    else if (batteryState.mode == battery::Mode::Measured)
      Serial.printf("Battery: MAX17048-compatible, %.1f%%, %.3f V\n", batteryState.percent, batteryState.voltage);
    else Serial.println("Battery: read/identity error; percentage unknown");
  }
}

bool writeRegister(uint8_t address, uint8_t reg, uint8_t value, TwoWire &bus = Wire) {
  bus.beginTransmission(address);
  bus.write(reg);
  bus.write(value);
  return bus.endTransmission() == 0;
}

void scanOneBus(TwoWire &bus, const char *label) {
  Serial.printf("\nI2C scan %s (ACK does not identify a chip):\n", label);
  unsigned found = 0;
  for (uint8_t address = 8; address < 120; ++address) {
    bus.beginTransmission(address);
    if (bus.endTransmission() == 0) {
      Serial.printf("  0x%02X\n", address);
      ++found;
    }
  }
  Serial.printf("Devices: %u\n", found);
}

void scanBus() {
  scanOneBus(Wire, "main SDA=21 SCL=22");
  if (magnetBusReady) scanOneBus(magnetWire, "magnetometer SDA=16 SCL=17");
  else Serial.println("Magnetometer I2C bus unavailable");
}

void detectBarometer() {
  flight.reset();
  screen::set(screen::Baro, "НЕ НАСТРОЕН");
  barometer = Barometer::None;
  for (uint8_t address : {uint8_t(0x76), uint8_t(0x77)}) {
    uint8_t id = 0;
    if (!readBytes(address, 0xD0, &id, 1)) continue;
    Serial.printf("Barometer at 0x%02X: chip ID 0x%02X\n", address, id);
    if (id == 0x60 && bme.begin(address, &Wire)) {
      bme.setSampling(Adafruit_BME280::MODE_NORMAL, Adafruit_BME280::SAMPLING_X2,
                      Adafruit_BME280::SAMPLING_X16, Adafruit_BME280::SAMPLING_X1,
                      Adafruit_BME280::FILTER_OFF, Adafruit_BME280::STANDBY_MS_0_5);
      baroAddress = address;
      barometer = Barometer::BME;
      Serial.println("BME280 initialized");
      return;
    }
    if (id == 0x58 && bmp.begin(address, id)) {
      bmp.setSampling(Adafruit_BMP280::MODE_NORMAL, Adafruit_BMP280::SAMPLING_X2,
                      Adafruit_BMP280::SAMPLING_X16, Adafruit_BMP280::FILTER_OFF,
                      Adafruit_BMP280::STANDBY_MS_1);
      baroAddress = address;
      barometer = Barometer::BMP;
      Serial.println("BMP280 initialized");
      return;
    }
  }
  Serial.println("No supported barometer initialized");
}

void detectMotion() {
  screen::set(screen::Accel, "НЕ НАСТРОЕН");
  screen::set(screen::Gyro, "НЕ НАСТРОЕН");
  // Never configure address 0x68: it is reserved for the connected RTC.
  bmiReady = false;
  Wire.beginTransmission(BMI_ADDRESS);
  if (Wire.endTransmission() != 0) {
    Serial.println("BMI160 absent at 0x69. Connect SDO/SA0 and CS/CSB to 3V3; RTC uses 0x68.");
    return;
  }
  uint8_t id = 0;
  if (!readBytes(BMI_ADDRESS, 0x00, &id, 1)) {
    Serial.println("BMI160 chip ID read failed");
    return;
  }
  Serial.printf("BMI160 CHIP_ID = 0x%02X (expected 0xD1)\n", id);
  if (id != 0xD1) {
    Serial.println("Unknown chip: configuration skipped");
    return;
  }
  // Normal mode: accelerometer needs up to 3.8 ms, gyro up to 80 ms.
  if (!writeRegister(BMI_ADDRESS, 0x7E, 0x11)) {
    Serial.println("BMI160 accelerometer startup failed");
    return;
  }
  delay(10);
  if (!writeRegister(BMI_ADDRESS, 0x7E, 0x15)) {
    Serial.println("BMI160 gyroscope startup failed");
    return;
  }
  delay(100);
  uint8_t pmu = 0, error = 0;
  if (!readBytes(BMI_ADDRESS, 0x03, &pmu, 1) ||
      !readBytes(BMI_ADDRESS, 0x02, &error, 1)) {
    Serial.println("BMI160 status read failed");
    return;
  }
  Serial.printf("BMI160 PMU_STATUS=0x%02X ERR_REG=0x%02X\n", pmu, error);
  bmiReady = (pmu & 0x3C) == 0x14 && error == 0;
  Serial.println(bmiReady ? "BMI160 initialized; raw accel/gyro stream enabled. No built-in compass."
                         : "BMI160 not ready; check power, wiring and status registers");
}

void printMotion() {
  if (!bmiReady) return;
  screen::set(screen::Accel, "НЕТ ДАННЫХ");
  screen::set(screen::Gyro, "НЕТ ДАННЫХ");
  uint8_t status = 0, data[12];
  if (!readBytes(BMI_ADDRESS, 0x1B, &status, 1)) {
    Serial.println("BMI160 status read failed");
    return;
  }
  if ((status & 0xC0) != 0xC0) {
    Serial.println("BMI160: waiting for fresh accel/gyro data");
    return;
  }
  // Burst from 0x0C: gyro XYZ followed by accelerometer XYZ, little endian.
  if (!readBytes(BMI_ADDRESS, 0x0C, data, sizeof(data))) {
    Serial.println("BMI160 data read failed");
    return;
  }
  int values[6];
  for (size_t i = 0; i < 6; ++i) {
    const uint16_t raw = uint16_t(data[2 * i]) | (uint16_t(data[2 * i + 1]) << 8);
    values[i] = raw >= 0x8000 ? int(raw) - 65536 : int(raw);
  }
  Serial.printf("BMI160 raw: accel XYZ=[%d, %d, %d]; gyro XYZ=[%d, %d, %d]\n",
                values[3], values[4], values[5], values[0], values[1], values[2]);
  screen::set(screen::Accel, "%d %d %d", values[3], values[4], values[5]);
  screen::set(screen::Gyro, "%d %d %d", values[0], values[1], values[2]);
}

bool detectQmc() {
  magnetWire.beginTransmission(QMC_ADDRESS);
  if (magnetWire.endTransmission() != 0) return false;
  uint8_t id = 0;
  if (!readBytes(QMC_ADDRESS, 0x0D, &id, 1, magnetWire)) {
    Serial.println("QMC5883L ID read failed");
    return false;
  }
  Serial.printf("QMC5883L candidate at 0x0D: CHIP_ID=0x%02X (expected 0xFF)\n", id);
  if (id != 0xFF) {
    Serial.println("Unknown chip at 0x0D; QMC configuration skipped");
    return false;
  }
  if (!writeRegister(QMC_ADDRESS, 0x0A, 0x80, magnetWire)) {
    Serial.println("QMC5883L reset failed");
    return false;
  }
  delay(10);
  // QST: set/reset period 1, interrupt disabled, OSR=512, +/-8 G, 50 Hz, continuous.
  if (!writeRegister(QMC_ADDRESS, 0x0B, 0x01, magnetWire) ||
      !writeRegister(QMC_ADDRESS, 0x0A, 0x01, magnetWire) ||
      !writeRegister(QMC_ADDRESS, 0x09, 0x15, magnetWire)) {
    Serial.println("QMC5883L configuration write failed");
    return false;
  }
  uint8_t config[3];
  if (!readBytes(QMC_ADDRESS, 0x09, config, sizeof(config), magnetWire) ||
      config[0] != 0x15 || config[1] != 0x01 || config[2] != 0x01) {
    Serial.println("QMC5883L configuration verification failed");
    return false;
  }
  qmcReady = true;
  Serial.println("QMC5883L-compatible device initialized; raw magnetic XYZ enabled (uncalibrated)");
  return true;
}

void printQmc() {
  uint8_t status = 0, data[6];
  if (!readBytes(QMC_ADDRESS, 0x06, &status, 1, magnetWire)) {
    Serial.println("QMC5883L status read failed");
    return;
  }
  if (!(status & 0x07)) {
    Serial.println("QMC5883L: waiting for fresh magnetic data");
    return;
  }
  // Read all XYZ bytes to clear DRDY/DOR; overflow clears when the field returns in range.
  if (!readBytes(QMC_ADDRESS, 0x00, data, sizeof(data), magnetWire)) {
    Serial.println("QMC5883L data read failed");
    return;
  }
  if (status & 0x02) {
    Serial.println("QMC5883L overflow: move away from magnets and steel");
    screen::set(screen::Magnetic, "QMC ПЕРЕПОЛНЕНИЕ");
    return;
  }
  int values[3];
  for (size_t i = 0; i < 3; ++i) {
    const uint16_t raw = uint16_t(data[2 * i]) | (uint16_t(data[2 * i + 1]) << 8);
    values[i] = raw >= 0x8000 ? int(raw) - 65536 : int(raw);
  }
  Serial.printf("QMC5883L raw: magnetic XYZ=[%d, %d, %d]%s\n",
                values[0], values[1], values[2],
                (status & 0x04) ? " (older samples skipped by 1 Hz diagnostics)" : "");
  screen::set(screen::Magnetic, "%d %d %d", values[0], values[1], values[2]);
}

void detectMagnetometer() {
  magneticHeading.invalidate();
  screen::set(screen::Magnetic, "НЕ НАСТРОЕН");
  hmcReady = false;
  qmcReady = false;
  if (!magnetBusReady) {
    Serial.println("Magnetometer skipped: second I2C bus failed to start");
    return;
  }
  if (detectQmc()) return;
  magnetWire.beginTransmission(HMC_ADDRESS);
  if (magnetWire.endTransmission() != 0) {
    Serial.println("No supported magnetometer initialized (QMC=0x0D, HMC=0x1E); SDA=16 SCL=17");
    return;
  }
  uint8_t id[3];
  if (!readBytes(HMC_ADDRESS, 0x0A, id, sizeof(id), magnetWire)) {
    Serial.println("HMC5883L ID read failed");
    return;
  }
  Serial.printf("HMC5883L ID = %02X %02X %02X (expected 48 34 33 / H43)\n",
                id[0], id[1], id[2]);
  if (id[0] != 0x48 || id[1] != 0x34 || id[2] != 0x33) {
    Serial.println("Unknown magnetometer: configuration skipped");
    return;
  }
  // Honeywell: 8-sample average, 15 Hz, normal bias; +/-1.3 gauss; continuous.
  if (!writeRegister(HMC_ADDRESS, 0x00, 0x70, magnetWire) ||
      !writeRegister(HMC_ADDRESS, 0x01, 0x20, magnetWire) ||
      !writeRegister(HMC_ADDRESS, 0x02, 0x00, magnetWire)) {
    Serial.println("HMC5883L configuration write failed");
    return;
  }
  uint8_t config[3];
  if (!readBytes(HMC_ADDRESS, 0x00, config, sizeof(config), magnetWire) ||
      config[0] != 0x70 || config[1] != 0x20 || (config[2] & 0x03) != 0) {
    Serial.println("HMC5883L configuration verification failed");
    return;
  }
  hmcReady = true;
  Serial.println("HMC5883L initialized; raw magnetic XYZ and uncalibrated XY heading enabled");
}

void printMagnetometer() {
  screen::set(screen::Magnetic, (qmcReady || hmcReady) ? "НЕТ ДАННЫХ" : "НЕ НАСТРОЕН");
  if (qmcReady) {
    printQmc();
    return;
  }
  if (!hmcReady) return;
  uint8_t status = 0, data[6];
  if (!readBytes(HMC_ADDRESS, 0x09, &status, 1, magnetWire)) {
    Serial.println("HMC5883L status read failed");
    return;
  }
  // Reading all six bytes also releases LOCK after a previously interrupted read.
  if (status & 0x02) {
    const bool recovered = readBytes(HMC_ADDRESS, 0x03, data, sizeof(data), magnetWire);
    Serial.println(recovered ? "HMC5883L lock cleared; waiting for next sample"
                             : "HMC5883L lock recovery failed");
    return;
  }
  if (!(status & 0x01)) {
    Serial.println("HMC5883L: waiting for fresh magnetic data");
    return;
  }
  if (!readBytes(HMC_ADDRESS, 0x03, data, sizeof(data), magnetWire)) {
    Serial.println("HMC5883L data read failed");
    return;
  }
  // Register order is X, Z, Y, with the most significant byte first.
  int values[3];
  for (size_t i = 0; i < 3; ++i) {
    const uint16_t raw = (uint16_t(data[2 * i]) << 8) | data[2 * i + 1];
    values[i] = raw >= 0x8000 ? int(raw) - 65536 : int(raw);
    if (values[i] == -4096) {
      screen::set(screen::Magnetic, "HMC ПЕРЕПОЛНЕНИЕ");
      Serial.println("HMC5883L overflow: move away from magnets, steel and current-carrying wires");
      return;
    }
  }
  Serial.printf("HMC5883L raw: magnetic XYZ=[%d, %d, %d]\n", values[0], values[2], values[1]);
  screen::set(screen::Magnetic, "%d %d %d", values[0], values[2], values[1]);
}

void compassTick(uint32_t now) {
  if (currentPage != 1 || uint32_t(now - compassTime) < 100) return;
  compassTime = now;
  if (soundStep >= 0 || (!qmcReady && !hmcReady)) { magneticHeading.invalidate(); return; }
  uint8_t status = 0, data[6];
  const uint8_t address = qmcReady ? QMC_ADDRESS : HMC_ADDRESS;
  if (!readBytes(address, qmcReady ? 0x06 : 0x09, &status, 1, magnetWire)) {
    magneticHeading.invalidate(); return;
  }
  if (qmcReady) {
    if (!(status & 0x07)) return; // No new sample: existing heading expires after 500 ms.
  } else if (status & 0x02) {
    readBytes(address, 0x03, data, sizeof(data), magnetWire); // Release HMC LOCK.
    magneticHeading.invalidate(); return;
  } else if (!(status & 0x01)) return;
  if (!readBytes(address, qmcReady ? 0x00 : 0x03, data, sizeof(data), magnetWire) ||
      (qmcReady && (status & 0x02))) {
    magneticHeading.invalidate(); return;
  }
  int values[3];
  for (int i = 0; i < 3; ++i) {
    const uint16_t raw = qmcReady ? uint16_t(data[i * 2]) | (uint16_t(data[i * 2 + 1]) << 8)
                                 : (uint16_t(data[i * 2]) << 8) | data[i * 2 + 1];
    values[i] = raw >= 0x8000 ? int(raw) - 65536 : int(raw);
    if (!qmcReady && values[i] == -4096) { magneticHeading.invalidate(); return; }
  }
  magneticHeading.update(values[0], values[qmcReady ? 1 : 2], millis());
}

int bcd(uint8_t value) {
  if ((value & 15) > 9 || (value >> 4) > 9) return -1;
  return (value >> 4) * 10 + (value & 15);
}

void printRtc(bool log = true) {
  uint8_t r[7], status = 0;
  if (!readBytes(0x68, 0, r, sizeof(r)) || !readBytes(0x68, 0x0F, &status, 1)) {
    screen::set(screen::Rtc, "НЕТ ОТВЕТА");
    if (log) Serial.println("RTC: no response");
    return;
  }
  int second = bcd(r[0]), minute = bcd(r[1]);
  int hour = bcd(r[2] & ((r[2] & 0x40) ? 0x1F : 0x3F));
  bool validHour = (r[2] & 0x40) ? hour >= 1 && hour <= 12 : hour >= 0 && hour <= 23;
  if (r[2] & 0x40) hour = hour % 12 + ((r[2] & 0x20) ? 12 : 0);
  int day = bcd(r[4]), month = bcd(r[5] & 0x1F), year = bcd(r[6]);
  if (second < 0 || second > 59 || minute < 0 || minute > 59 || !validHour ||
      day < 1 || day > 31 || month < 1 || month > 12 || year < 0 || r[3] < 1 || r[3] > 7) {
    screen::set(screen::Rtc, "ОШИБКА ДАННЫХ");
    if (log) Serial.println("RTC: invalid registers; check model, time and address collision");
    return;
  }
  screen::set(screen::Rtc, "%04d-%02d-%02d %02d:%02d:%02d%s",
              2000 + year + ((r[5] & 0x80) ? 100 : 0), month, day, hour, minute, second,
              (status & 0x80) ? " !" : "");
  if (log) Serial.printf("RTC (assuming DS3231): %04d-%02d-%02d %02d:%02d:%02d; OSF=%u%s\n",
                2000 + year + ((r[5] & 0x80) ? 100 : 0), month, day, hour, minute, second,
                (status >> 7) & 1, (status & 0x80) ? " TIME NOT TRUSTED: oscillator stopped" : "");
}

void stopSound() {
  playingFrequency = 0;
  audioMode = vario::Sound::Silent;
  if (pwmAttached) {
    ledcWriteTone(0, 0);
    ledcDetachPin(BUZZER_PIN);
    pwmAttached = false;
  }
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  soundStep = -1;
}

void startSoundTest() {
  audioEnabled = false; stopSound(); flight.reset();
  Serial.println("Sound test: DC HIGH for 400 ms");
  digitalWrite(BUZZER_PIN, HIGH); soundStep = 0; soundTime = millis();
}

void soundTick() {
  if (soundStep < 0 || millis() - soundTime < 400) return;
  soundTime = millis();
  ++soundStep;
  if (soundStep >= 7) {
    stopSound();
    Serial.println("Sound test finished");
  } else if (soundStep % 2 == 1) {
    if (pwmAttached) ledcWriteTone(0, 0);
    else digitalWrite(BUZZER_PIN, LOW);
  } else {
    if (!pwmAttached) {
      pwmAttached = ledcSetup(0, 700, 8) > 0;
      if (pwmAttached) ledcAttachPin(BUZZER_PIN, 0);
    }
    if (!pwmAttached) {
      Serial.println("LEDC attach failed");
      stopSound();
      return;
    }
    const uint32_t frequency = soundStep == 2 ? 700 : soundStep == 4 ? 1400 : 2100;
    ledcWriteTone(0, frequency);
    Serial.printf("Tone: %lu Hz\n", static_cast<unsigned long>(frequency));
  }
}

void help() {
  Serial.println("Buttons: BACK=32 UP=33 DOWN=34 OK=35, active LOW; 34/35 need 10k to 3V3.");
  Serial.println("BACK: page/cancel, hold BACK: sound; OK: select/settings, hold OK: settings.");
  Serial.println("UP/DOWN: select/change, hold repeats. Menu zero requires YES + OK.");
  Serial.printf("\nVARIOMETER: barometer 10 Hz, display 5 Hz, sound %s.\n",
                audioEnabled ? "ON" : "MUTED");
  Serial.println("m=toggle vario sound, x=mute, z=zero relative altitude, v=vario/compass/diagnostics");
  Serial.println("q1013.25 + Enter = QNH hPa (800..1100); a10 + Enter = averaging seconds (1..30)");
  Serial.println("s=scan, d=detect sensors, r=RTC, p=toggle Serial telemetry, t=redraw, h=help");
  Serial.println("g=display geometry/next rotation; t=return. Menu: Buzzer test, OK/BACK stops.");
  Serial.println("b=short buzzer test (mutes vario); scan/detect/test reset the filter.");
  Serial.println("Climb sound: >=+0.2m/s (off <=+0.1); sink <=-2; alarm <=-10.");
  Serial.println("Compass: uncalibrated XY heading, keep sensor level; no tilt compensation.");
  Serial.println("Altitude depends on QNH. No flight detection/log yet. RTC is not overwritten.");
}

void drawCurrent(bool force = false) {
  if (geometryRotation >= 0) return;
  if (menu.mode != buttons::Mode::Closed)
    screen::drawMenu(menu, flight.qnh, flight.averageSeconds, audioEnabled, menuNotice, soundStep >= 0, force);
  else if (currentPage == 1) screen::drawCompass(magneticHeading, millis(), force);
  else if (currentPage == 2) screen::draw(force);
  else screen::drawVario(flight, millis(), audioEnabled, force);
  screen::drawBattery(batteryState);
}

void updateBarometer(uint32_t now) {
  if (uint32_t(now - baroTime) < 100) return;
  baroTime = now;
  currentPressure = currentTemperature = NAN;
  bool responds = false;
  if (barometer != Barometer::None) {
    Wire.beginTransmission(baroAddress);
    responds = Wire.endTransmission() == 0;
  }
  if (responds) {
    currentPressure = barometer == Barometer::BMP ? bmp.readPressure() : bme.readPressure();
    currentTemperature = barometer == Barometer::BMP ? bmp.readTemperature() : bme.readTemperature();
  }
  if (!isfinite(currentTemperature)) currentPressure = NAN;
  if (!flight.update(currentPressure, millis())) screen::set(screen::Baro, "НЕТ ДАННЫХ");
  else screen::set(screen::Baro, "%.2fгПа %.1fC", currentPressure / 100, currentTemperature);
}

void varioSoundTick(uint32_t now) {
  if (soundStep >= 0) return;
  vario::Sound next = audioEnabled && flight.ready(now)
      ? vario::soundMode(flight.velocity, audioMode) : vario::Sound::Silent;
  if (next != audioMode) { audioMode = next; audioPhase = now; }
  const uint32_t frequency = vario::tone(flight.velocity, audioMode, now - audioPhase);
  if (frequency == playingFrequency) return;
  if (!pwmAttached && frequency) {
    pwmAttached = ledcSetup(0, frequency, 8) > 0;
    if (pwmAttached) ledcAttachPin(BUZZER_PIN, 0);
    else { audioEnabled = false; Serial.println("Audio initialization failed"); return; }
  }
  if (pwmAttached) ledcWriteTone(0, frequency);
  playingFrequency = frequency;
}

void settingCommand(char *command) {
  char *end = nullptr;
  const double value = strtod(command + 1, &end);
  while (*end == ' ' || *end == '\t') ++end;
  if (end == command + 1 || *end || !isfinite(value)) {
    Serial.println("Invalid number; examples: q1013.25 or a10 followed by Enter"); return;
  }
  if (command[0] == 'q') {
    if (!flight.setQnh(value)) { Serial.println("QNH must be 800..1100 hPa"); return; }
    menu.close();
    stopSound();
    const bool saved = settingsReady && settings.putFloat("qnh", value) == sizeof(float);
    Serial.printf("QNH %.2f hPa; filter warming up; %s\n", value, saved ? "saved" : "RAM only");
  } else {
    if (!flight.setAverage(value)) { Serial.println("Averaging must be 1..30 seconds"); return; }
    menu.close();
    const bool saved = settingsReady && settings.putFloat("avg", value) == sizeof(float);
    Serial.printf("Averaging %.1f seconds; %s\n", value, saved ? "saved" : "RAM only");
  }
}

void serialTick() {
  static char command[32] = {};
  static size_t length = 0;
  static bool discard = false;
  while (Serial.available()) {
    const char key = Serial.read();
    if (key == '\r' || key == '\n') {
      if (length && !discard) { command[length] = 0; settingCommand(command); }
      length = 0; discard = false;
      continue;
    }
    if (discard) continue;
    if (length || key == 'q' || key == 'a') {
      if (length >= sizeof(command) - 1) {
        Serial.println("Command too long; discarded until Enter"); discard = true;
      } else command[length++] = key;
      continue;
    }
    switch (key) {
      case 's': stopSound(); flight.reset(); scanBus(); break;
      case 'd': stopSound(); detectBarometer(); detectMotion(); detectMagnetometer(); break;
      case 'r': printRtc(); break;
      case 'p': streaming = !streaming; Serial.println(streaming ? "Serial ON" : "Serial OFF"); break;
      case 'm': audioEnabled = !audioEnabled; stopSound(); Serial.println(audioEnabled ? "Sound ON" : "Muted"); break;
      case 'z': Serial.println(flight.zero(millis()) ? "Relative altitude zeroed" : "Wait for valid pressure"); break;
      case 'v': menu.close(); currentPage = (currentPage + 1) % 3; drawCurrent(true); break;
      case 'b': startSoundTest(); break;
      case 'x': audioEnabled = false; stopSound(); Serial.println("Muted"); break;
      case 'h': help(); break;
      case 'g':
        geometryRotation = geometryRotation < 0 ? 1 : (geometryRotation + 1) % 4;
        screen::geometryTest(geometryRotation); break;
      case 't':
        if (geometryRotation >= 0) { geometryRotation = -1; screen::endGeometryTest(); }
        drawCurrent(true); break;
    }
  }
}

void buttonsBegin() {
  for (unsigned i = 0; i < 4; ++i) {
    buttonInput[i].attach(buttonPins[i], i < 2 ? INPUT_PULLUP : INPUT);
    buttonInput[i].interval(30);
    buttonInput[i].setPressedState(LOW);
    buttonState[i].begin(buttonInput[i].isPressed(), millis());
  }
}

void buttonsTick() {
  for (unsigned i = 0; i < 4; ++i) {
    buttonInput[i].update(); // Exactly once per loop, before reading the stable state.
    const auto event = buttonState[i].update(buttonInput[i].isPressed(),
                                            millis(), i == 1 || i == 2);
    if (event == buttons::Event::None) continue;
    if (geometryRotation >= 0) {
      geometryRotation = -1; screen::endGeometryTest(); drawCurrent(true);
      continue;
    }
    menuNotice[0] = 0;
    const auto action = menu.handle(static_cast<buttons::Key>(i), event,
                                    flight.qnh, flight.averageSeconds);
    switch (action) {
      case buttons::Action::StartBuzzerTest: startSoundTest(); break;
      case buttons::Action::StopBuzzerTest: stopSound(); break;
      case buttons::Action::Page: currentPage = (currentPage + 1) % 3; break;
      case buttons::Action::ToggleSound:
        audioEnabled = !audioEnabled; stopSound();
        Serial.println(audioEnabled ? "Button: sound ON" : "Button: muted");
        break;
      case buttons::Action::SaveQnh: {
        if (!flight.setQnh(menu.draft)) { strcpy(menuNotice, "ОШИБКА QNH"); break; }
        stopSound();
        const bool saved = settingsReady && settings.putFloat("qnh", flight.qnh) == sizeof(float);
        strcpy(menuNotice, saved ? "QNH СОХРАНЁН. ПРОГРЕВ ФИЛЬТРА." : "QNH ПРИМЕНЁН, НЕ СОХРАНЁН.");
        Serial.println(menuNotice);
        break;
      }
      case buttons::Action::SaveAverage: {
        if (!flight.setAverage(menu.draft)) { strcpy(menuNotice, "ОШИБКА ПЕРИОДА УСРЕДНЕНИЯ"); break; }
        const bool saved = settingsReady && settings.putFloat("avg", flight.averageSeconds) == sizeof(float);
        strcpy(menuNotice, saved ? "УСРЕДНЕНИЕ СОХРАНЕНО." : "УСРЕДНЕНИЕ НЕ СОХРАНЕНО.");
        Serial.println(menuNotice);
        break;
      }
      case buttons::Action::Zero:
        strcpy(menuNotice, flight.zero(millis()) ? "ОТНОСИТ. ВЫСОТА ОБНУЛЕНА." : "НЕТ ДАВЛЕНИЯ. НОЛЬ НЕ ИЗМЕНЁН.");
        Serial.println(menuNotice);
        break;
      default: break;
    }
  }
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.begin(115200);
  delay(1200);
  screen::begin();
  settingsReady = settings.begin("variometer", false);
  if (settingsReady) {
    flight.setQnh(settings.getFloat("qnh", 1013.25f));
    flight.setAverage(settings.getFloat("avg", 10.0f));
  }
  Serial.println("\nVARIOMETER hardware diagnostics / ESP32");
  Serial.println("Firmware build: " __DATE__ " " __TIME__);
  Serial.printf("Boot sound setting: %s\n", SOUND_ENABLED_ON_BOOT ? "ON" : "MUTED");
  Serial.println("Required: BMI160 SDO/SA0=3V3, CS/CSB=3V3 (0x69), RTC=0x68, SDA=21 SCL=22.");
  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  Wire.setTimeOut(50);
  magnetBusReady = magnetWire.begin(HMC_SDA_PIN, HMC_SCL_PIN, 100000);
  if (magnetBusReady) magnetWire.setTimeOut(50);
  Serial.println("Magnetometer QMC/HMC uses separate I2C: SDA=16 SCL=17; pins reserved for the sensor.");
  scanBus();
  batteryTick(millis(), true);
  detectBarometer();
  detectMotion();
  detectMagnetometer();
  printRtc();
  help();
  buttonsBegin();
  drawCurrent(true);
}

void loop() {
  buttonsTick();
  serialTick();
  updateBarometer(millis());
  batteryTick(millis());
  compassTick(millis());
  soundTick();
  varioSoundTick(millis());
  if (soundStep < 0 && millis() - sampleTime >= 1000) {
    sampleTime = millis();
    if (currentPage == 2 && streaming) { printMotion(); printMagnetometer(); }
    printRtc(false);
    if (streaming) {
      if (flight.ready(millis())) Serial.printf("VARIO alt=%.1fm rel=%.1fm v=%+.2fm/s avg=%+.2fm/s p=%.2fhPa\n",
          flight.height, flight.relative(), flight.velocity, flight.average, currentPressure / 100);
      else Serial.println("VARIO: warming up or missing pressure; sound inhibited");
    }
  }
  screen::set(screen::State, "%s / %s", streaming ? "ПОРТ ВКЛ" : "ПОРТ ВЫКЛ",
              soundStep >= 0 ? "ТЕСТ ЗВУКА" : (qmcReady ? "QMC5883L" : hmcReady ? "HMC5883L" : "НЕТ МАГНИТОМЕТРА"));
  if (millis() - displayTime >= 200) { displayTime = millis(); drawCurrent(); }
  delay(2);
}
