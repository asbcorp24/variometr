#include "diagnostic_display.h"
#include "russian_gfx.h"
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_ILI9341.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace screen {
namespace {
constexpr uint8_t CS = 27, DC = 26, RST = 25;
constexpr uint8_t DEFAULT_ROTATION = 1; // Portrait on this native-320x240 module.
// This module's observed native geometry is 320x240, unlike the stock driver's
// 240x320 assumption. Preserve MADCTL orientation; correct the drawing bounds.
// This is a panel profile, not an identification of the controller model.
class ModuleDisplay : public Adafruit_ILI9341 {
 public:
  using Adafruit_ILI9341::Adafruit_ILI9341;
  void setRotation(uint8_t r) override {
    Adafruit_ILI9341::setRotation(r);
    _width = (r & 1) ? 240 : 320;
    _height = (r & 1) ? 320 : 240;
  }
};
ru::Display<ModuleDisplay> tft(&SPI, DC, CS, RST);
const char *labels[RowCount] = {"ДАВЛЕНИЕ / ТЕМПЕРАТУРА", "BMI160 УСКОРЕНИЕ XYZ",
  "BMI160 ВРАЩЕНИЕ XYZ", "МАГНИТНОЕ ПОЛЕ XYZ", "ЧАСЫ (! = ВРЕМЯ НЕ ЗАДАНО)", "ДИАГНОСТИКА"};
char values[RowCount][80] = {"ОЖИДАНИЕ", "ОЖИДАНИЕ", "ОЖИДАНИЕ", "ОЖИДАНИЕ", "ОЖИДАНИЕ", "ЗАПУСК"};
char previous[RowCount][80] = {};
bool started = false;
char batteryPrevious[80] = {};
int page = -1; // 0 diagnostics, 1 variometer, 2 menu.
}

void set(Row row, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vsnprintf(values[row], sizeof(values[row]), format, args);
  va_end(args);
}

void drawBattery(const battery::State &state) {
  if (!started) return;
  const bool error = state.mode == battery::Mode::Error;
  const bool demo = state.mode == battery::Mode::Demo;
  char percent[12], detail[32], signature[80];
  if (error) strcpy(percent, "--%");
  else snprintf(percent, sizeof(percent), "%d%%", int(state.percent + 0.5f));
  if (error) strcpy(detail, "ОШИБКА");
  else if (demo) strcpy(detail, "ДЕМО");
  else snprintf(detail, sizeof(detail), "%.2f В", state.voltage);
  snprintf(signature, sizeof(signature), "%s %s", percent, detail);
  if (strcmp(signature, batteryPrevious) == 0) return;
  strcpy(batteryPrevious, signature);
  const uint16_t color = error ? ILI9341_RED : demo ? ILI9341_YELLOW :
                         state.percent <= 20 ? ILI9341_ORANGE : ILI9341_GREEN;
  tft.fillRect(154, 2, 84, 21, ILI9341_BLACK);
  tft.drawRect(156, 4, 19, 10, color);
  tft.fillRect(175, 7, 2, 4, color);
  if (!error) {
    const int fill = int(state.percent * 15 / 100 + 0.5f);
    if (fill > 0) tft.fillRect(158, 6, fill, 6, color);
  }
  tft.setTextSize(1); tft.setTextColor(color);
  tft.setCursor(181, 4); tft.print(percent);
  tft.setCursor(181, 15); tft.print(detail);
}

void draw(bool force) {
  if (!started) return;
  if (page != 0) force = true;
  page = 0;
  if (force) {
    tft.fillScreen(ILI9341_BLACK); batteryPrevious[0] = 0;
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(5, 5);
    tft.print("ДИАГНОСТИКА");
    tft.setTextSize(1);
    for (int i = 0; i < RowCount; ++i) {
      tft.setCursor(5, 35 + i * 40);
      tft.setTextColor(ILI9341_CYAN);
      tft.print(labels[i]);
    }
    tft.drawFastHLine(0, 284, 240, ILI9341_DARKGREY);
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(5, 300);
    tft.print("НАЗАД: ЭКРАН / ЗВУК   ОК: МЕНЮ");
  }
  for (int i = 0; i < RowCount; ++i) {
    if (!force && strcmp(values[i], previous[i]) == 0) continue;
    tft.fillRect(5, 49 + i * 40, 230, 16, ILI9341_BLACK);
    tft.setCursor(5, 49 + i * 40);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.print(values[i]);
    strcpy(previous[i], values[i]);
  }
}

void drawVario(const vario::Filter &f, uint32_t now, bool sound, bool force) {
  if (!started) return;
  if (page != 1) force = true;
  page = 1;
  static char old[7][96] = {};
  char text[7][96] = {};
  const bool fresh = f.fresh(now), ready = f.ready(now);
  if (ready) {
    snprintf(text[0], 9, "%+.1f", f.velocity);
    snprintf(text[1], 9, "%+.1f", f.average);
  } else {
    strcpy(text[0], fresh ? "ЖДИТЕ" : "---");
    strcpy(text[1], "---");
  }
  if (fresh) {
    snprintf(text[2], 9, "%.0f", f.height);
    snprintf(text[3], 9, "%+.0f", f.relative());
  } else { strcpy(text[2], "---"); strcpy(text[3], "---"); }
  snprintf(text[4], sizeof(text[4]), "QNH %.2f  СРЕД. %.0fс", f.qnh, f.averageSeconds);
  snprintf(text[5], sizeof(text[5]), "%s", values[Rtc]);
  snprintf(text[6], sizeof(text[6]), "%s | %s", ready ? "ГОТОВ" : fresh ? "ПРОГРЕВ" : "ОШИБКА БАРО",
           sound ? "ЗВУК ВКЛ" : "БЕЗ ЗВУКА");
  if (force) {
    tft.fillScreen(ILI9341_BLACK); batteryPrevious[0] = 0;
    tft.setTextSize(2); tft.setTextColor(ILI9341_WHITE); tft.setCursor(5, 5);
    tft.print("ВАРИОМЕТР");
    tft.setTextSize(1); tft.setTextColor(ILI9341_CYAN);
    tft.setCursor(5, 45); tft.print("ВЕРТ. СКОРОСТЬ м/с");
    tft.setCursor(5, 110); tft.print("СРЕДНЯЯ м/с");
    tft.drawFastHLine(5, 143, 230, ILI9341_DARKGREY);
    tft.setCursor(5, 155); tft.print("ВЫСОТА QNH / м");
    tft.setCursor(125, 155); tft.print("ОТНОСИТ. / м");
    tft.drawFastHLine(5, 199, 230, ILI9341_DARKGREY);
    tft.setCursor(5, 235); tft.print("ЧАСЫ (! = ЗАДАЙТЕ ВРЕМЯ)");
    tft.drawFastHLine(5, 284, 230, ILI9341_DARKGREY);
    tft.setTextColor(ILI9341_YELLOW); tft.setCursor(5, 300);
    tft.print("НАЗАД: ЭКРАН / ЗВУК   ОК: МЕНЮ");
  }
  const int x[7] = {5, 125, 5, 125, 5, 5, 5};
  const int y[7] = {62, 108, 173, 173, 213, 253, 28};
  const int w[7] = {230, 110, 110, 110, 230, 230, 230};
  const int size[7] = {4, 2, 2, 2, 1, 1, 1};
  for (int i = 0; i < 7; ++i) {
    if (!force && strcmp(old[i], text[i]) == 0) continue;
    tft.fillRect(x[i], y[i], w[i], size[i] * 8, ILI9341_BLACK);
    tft.setTextColor(i == 0 && ready ? (f.velocity >= 0 ? ILI9341_GREEN : ILI9341_ORANGE) : ILI9341_WHITE);
    // Fit unusually long readings inside their own field.
    const int fontSize = ru::length(text[i]) * 6 * size[i] <= unsigned(w[i]) ? size[i] : 1;
    tft.setTextSize(fontSize); tft.setCursor(x[i], y[i]); tft.print(text[i]);
    strcpy(old[i], text[i]);
  }
}

void drawMenu(const buttons::Menu &menu, double qnh, double average, bool sound,
              const char *notice, bool testRunning, bool force) {
  if (!started) return;
  if (page != 2) force = true;
  page = 2;
  static char previousMenu[256] = {};
  char signature[256];
  snprintf(signature, sizeof(signature), "%d %d %.2f %d %.2f %.2f %d %d %s",
           int(menu.mode), menu.selected, menu.draft, menu.confirm, qnh, average, sound, testRunning, notice);
  if (!force && strcmp(signature, previousMenu) == 0) return;
  strcpy(previousMenu, signature);
  tft.fillScreen(ILI9341_BLACK); batteryPrevious[0] = 0;
  tft.setTextColor(ILI9341_WHITE); tft.setTextSize(2); tft.setCursor(8, 8);
  tft.print("НАСТРОЙКИ");
  if (menu.mode == buttons::Mode::List) {
    char rows[6][80];
    snprintf(rows[0], sizeof(rows[0]), "QNH       %.2f", qnh);
    snprintf(rows[1], sizeof(rows[1]), "СРЕДНЕЕ   %.1fс", average);
    snprintf(rows[2], sizeof(rows[2]), "ЗВУК      %s", sound ? "ВКЛ" : "ВЫКЛ");
    strcpy(rows[3], "ОБНУЛИТЬ ВЫСОТУ");
    strcpy(rows[4], "ТЕСТ ПИЩАЛКИ"); strcpy(rows[5], "НАЗАД");
    for (int i = 0; i < 6; ++i) {
      const bool selected = menu.selected == i;
      if (selected) tft.fillRect(4, 38 + i * 32, 232, 28, ILI9341_DARKCYAN);
      tft.setTextColor(ILI9341_WHITE); tft.setCursor(10, 44 + i * 32);
      tft.print(rows[i]);
    }
  } else if (menu.mode == buttons::Mode::BuzzerTest) {
    tft.setCursor(8, 55); tft.print("ТЕСТ ПИЩАЛКИ");
    tft.setCursor(8, 90); tft.setTextColor(ILI9341_YELLOW);
    tft.print(testRunning ? "ЗВУЧИТ..." : "ТЕСТ ЗАВЕРШЁН");
    tft.setTextSize(1); tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(8, 125); tft.print("ИМПУЛЬС, ЗАТЕМ 700 / 1400 / 2100 Гц");
    tft.setCursor(8, 145); tft.print("ПОСЛЕ ТЕСТА ЗВУК ВАРИО ВЫКЛЮЧЕН.");
    tft.setCursor(8, 300); tft.print("ОК / НАЗАД: СТОП И ВОЗВРАТ");
    return;
  } else if (menu.mode == buttons::Mode::ConfirmZero) {
    tft.setCursor(8, 55); tft.print("ОБНУЛИТЬ ВЫСОТУ?");
    tft.setCursor(8, 90); tft.setTextColor(ILI9341_YELLOW); tft.print("ПО УМОЛЧАНИЮ: НЕТ");
    tft.fillRect(menu.confirm ? 125 : 5, 128, 110, 36, ILI9341_DARKCYAN);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(30, 139); tft.print("НЕТ");
    tft.setCursor(150, 139); tft.print("ДА");
  } else {
    const bool isQnh = menu.mode == buttons::Mode::Qnh;
    tft.setCursor(8, 55); tft.print(isQnh ? "QNH / гПа" : "УСРЕДНЕНИЕ / с");
    char value[20]; snprintf(value, sizeof(value), isQnh ? "%.2f" : "%.1f", menu.draft);
    tft.setTextSize(4); tft.setCursor(8, 90); tft.setTextColor(ILI9341_GREEN); tft.print(value);
    tft.setTextSize(1); tft.setTextColor(ILI9341_WHITE); tft.setCursor(8, 145);
    tft.print(isQnh ? "ШАГ 0.1 гПа   ДИАПАЗОН 800..1100" : "ШАГ 1 с   ДИАПАЗОН 1..30");
    tft.setCursor(8, 165); tft.print("УДЕРЖАНИЕ +/-: ПОВТОР. ОК: СОХРАНИТЬ");
  }
  tft.setTextSize(1); tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor(8, 260); tft.print(notice);
  tft.setCursor(8, 291); tft.print("ВВЕРХ/ВНИЗ: ВЫБОР  ОК: ПРИНЯТЬ");
  tft.setCursor(8, 303); tft.print("НАЗАД: ОТМЕНА");
}

void drawCompass(const compass::Heading &heading, uint32_t now, bool force) {
  if (!started) return;
  if (page != 3) force = true;
  page = 3;
  static ru::Display<GFXcanvas1> dial(176, 176);
  static int previousDegrees = -2;
  const bool fresh = heading.fresh(now);
  const int degrees = fresh ? int(heading.degrees + 0.5f) % 360 : -1;
  if (force) {
    tft.fillScreen(ILI9341_BLACK); batteryPrevious[0] = 0;
    tft.setTextSize(2); tft.setTextColor(ILI9341_WHITE); tft.setCursor(8, 5);
    tft.print("КОМПАС");
    tft.setTextSize(1); tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(8, 28); tft.print("БЕЗ КАЛИБРОВКИ. ДЕРЖИТЕ РОВНО.");
    tft.setCursor(8, 302); tft.print("НАЗАД: ЭКРАН   ОК: МЕНЮ");
  }
  if (!force && degrees == previousDegrees) return;
  previousDegrees = degrees;
  if (!dial.getBuffer()) {
    tft.setTextSize(1); tft.setCursor(8, 50); tft.print("НЕТ ПАМЯТИ ДЛЯ КОМПАСА");
    return;
  }
  dial.fillScreen(0);
  dial.drawCircle(88, 88, 80, 1);
  // Fixed bow marker; the compass card rotates beneath it.
  dial.fillTriangle(82, 1, 94, 1, 88, 10, 1);
  if (fresh) {
    for (int bearing = 0; bearing < 360; bearing += 15) {
      const float a = (bearing - degrees) * 3.141592654f / 180;
      const int inner = bearing % 90 == 0 ? 68 : 74;
      dial.drawLine(88 + lroundf(sinf(a) * inner), 88 - lroundf(cosf(a) * inner),
                    88 + lroundf(sinf(a) * 79), 88 - lroundf(cosf(a) * 79), 1);
    }
    const char *cardinals[4] = {"С", "В", "Ю", "З"};
    dial.setTextSize(2); dial.setTextColor(1);
    for (int i = 0; i < 4; ++i) {
      const float a = (i * 90 - degrees) * 3.141592654f / 180;
      dial.setCursor(82 + lroundf(sinf(a) * 55), 80 - lroundf(cosf(a) * 55));
      dial.print(cardinals[i]);
    }
    dial.drawLine(88, 102, 88, 73, 1);
    dial.fillTriangle(88, 66, 82, 77, 94, 77, 1);
  } else {
    dial.setTextSize(1); dial.setTextColor(1);
    dial.setCursor(49, 84); dial.print("НЕТ ДАННЫХ");
  }
  // Send whole scanlines rather than starting an SPI address window for every pixel.
  uint16_t scanline[176];
  const uint8_t *bitmap = dial.getBuffer();
  for (int y = 0; y < 176; ++y) {
    for (int x = 0; x < 176; ++x)
      scanline[x] = bitmap[y * 22 + x / 8] & (0x80 >> (x % 8)) ? ILI9341_WHITE : ILI9341_BLACK;
    tft.drawRGBBitmap(32, 58 + y, scanline, 176, 1);
  }
  tft.fillRect(8, 255, 224, 32, ILI9341_BLACK);
  tft.setTextSize(2); tft.setTextColor(ILI9341_CYAN); tft.setCursor(30, 263);
  if (fresh) {
    const char *names[8] = {"С", "СВ", "В", "ЮВ", "Ю", "ЮЗ", "З", "СЗ"};
    char value[20]; snprintf(value, sizeof(value), "%03d гр.", degrees);
    tft.print(value);
    tft.setCursor(150, 263); tft.print(names[((degrees + 22) / 45) % 8]);
  } else tft.print("--- гр.");
}

void geometryTest(uint8_t rotation) {
  if (!started) return;
  tft.setRotation(rotation);
  const int w = tft.width(), h = tft.height();
  tft.fillScreen(ILI9341_DARKGREY);
  for (int y = 20; y < h - 1; y += 20)
    tft.drawFastHLine(1, y, w - 2, ILI9341_BLUE);
  tft.drawRect(0, 0, w, h, ILI9341_WHITE);
  tft.fillRect(2, 2, 12, 12, ILI9341_RED);
  tft.fillRect(w - 14, 2, 12, 12, ILI9341_GREEN);
  tft.fillRect(2, h - 14, 12, 12, ILI9341_BLUE);
  tft.fillRect(w - 14, h - 14, 12, 12, ILI9341_YELLOW);
  tft.setTextSize(2); tft.setTextColor(ILI9341_WHITE, ILI9341_DARKGREY);
  tft.setCursor(20, 30); tft.print("ТЕСТ ЭКРАНА");
  char label[32];
  snprintf(label, sizeof(label), "R%u: %d x %d", rotation % 4, w, h);
  tft.setCursor(20, 60); tft.print(label);
  tft.setTextSize(1);
  tft.setCursor(20, h - 30); tft.print("g: ПОВОРОТ   t: ВОЗВРАТ");
  Serial.printf("Display geometry R%u: width=%d height=%d; all four corners must be visible.\n",
                rotation % 4, w, h);
  page = -1;
}

void endGeometryTest() {
  tft.setRotation(DEFAULT_ROTATION);
  page = -1;
}

void begin() {
  SPI.begin(18, 19, 23, CS);
  tft.begin(10000000); // Lower SPI clock while checking artifacts on jumper wiring.
  tft.setRotation(DEFAULT_ROTATION); // Portrait with corrected bounds: width 240, height 320.
  Serial.printf("Display profile: native 320x240, rotation=%u, width=%d height=%d\n",
                DEFAULT_ROTATION, tft.width(), tft.height());
  tft.setTextWrap(false);
  started = true;
  draw(true);
}
}
