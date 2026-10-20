#pragma once
#include "vario.h"
#include "buttons.h"
#include "compass.h"
#include "battery.h"
#include "flight_journal.h"
#include "rtc_clock.h"

namespace screen {
enum Row { Baro, Accel, Gyro, Magnetic, Rtc, State, RowCount };
void begin();
void geometryTest(uint8_t rotation);
void endGeometryTest();
void set(Row row, const char *format, ...);
void draw(bool force = false);
void drawBattery(const battery::State &state);
void drawCompass(const compass::Heading &heading, uint32_t now, bool force = false);
void drawJournal(const FlightJournal &journal, size_t journalIndex, const DeviceClock &clock,
                 const char *notice, bool force = false);
void drawVario(const vario::Filter &filter, uint32_t now, bool sound, const FlightJournal &journal, bool force = false);
void drawMenu(const buttons::Menu &menu, double qnh, double average, bool sound,
              const char *notice, bool testRunning, const FlightJournal &journal,
              size_t journalIndex, const DeviceClock &clock, const battery::State &battery, bool force = false);
}
