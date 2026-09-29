#include "buttons.h"
#include <Bounce2.h>
#include <cassert>
#include <cstdio>
using namespace buttons;
static uint32_t clockMs;
static bool pinDown;
unsigned long millis() { return clockMs; }
int digitalRead(int) { return pinDown ? LOW : HIGH; }
void pinMode(int, int) {}
// Exercise the real Bounce2 source with simulated GPIO and time.
class TestButton {
 public:
  void begin(bool down, uint32_t now) {
    pinDown = down; clockMs = now;
    input.attach(32, INPUT_PULLUP);
    input.interval(30);
    input.setPressedState(LOW);
    gestures.begin(input.isPressed(), now);
  }
  Event update(bool down, uint32_t now, bool repeat) {
    pinDown = down; clockMs = now;
    input.update();
    return gestures.update(input.isPressed(), now, repeat);
  }
 private:
  Bounce2::Button input;
  Gestures gestures;
};
int main() {
  TestButton b;
  b.begin(false, 0);
  assert(b.update(true, 10, false) == Event::None);
  assert(b.update(false, 15, false) == Event::None);
  assert(b.update(true, 20, false) == Event::None);
  assert(b.update(true, 49, false) == Event::None);
  assert(b.update(true, 50, false) == Event::None);
  assert(b.update(false, 100, false) == Event::None);
  assert(b.update(false, 130, false) == Event::Short);
  assert(b.update(false, 200, false) == Event::None);
  b.begin(false, 0);
  b.update(true, 10, true); b.update(true, 40, true);
  assert(b.update(true, 839, true) == Event::None);
  assert(b.update(true, 840, true) == Event::Long);
  assert(b.update(true, 959, true) == Event::None);
  assert(b.update(true, 960, true) == Event::Repeat);
  b.update(false, 980, true);
  assert(b.update(false, 1010, true) == Event::None); // No short after hold.
  b.begin(true, 0);
  assert(b.update(true, 1000, false) == Event::None); // Held at boot is ignored.
  b.update(false, 1100, false);
  assert(b.update(false, 1130, false) == Event::None);
  b.update(true, 1200, false); b.update(true, 1230, false);
  b.update(false, 1250, false);
  assert(b.update(false, 1280, false) == Event::Short);
  const uint32_t wrap = UINT32_MAX - 10;
  b.begin(false, wrap - 100); b.update(true, wrap, false);
  b.update(true, wrap + 30, false);
  assert(b.update(true, wrap + 830, false) == Event::Long);
  assert(b.update(true, wrap + 1000, false) == Event::None);

  Menu m;
  assert(m.handle(Key::Back, Event::Short, 1013.25, 10) == Action::Page);
  assert(m.handle(Key::Back, Event::Long, 1013.25, 10) == Action::ToggleSound);
  m.handle(Key::Ok, Event::Long, 1013.25, 10);
  assert(m.mode == Mode::List);
  m.handle(Key::Ok, Event::Short, 1013.25, 10);
  assert(m.mode == Mode::Qnh);
  m.handle(Key::Up, Event::Short, 1013.25, 10);
  assert(m.draft > 1013.25);
  assert(m.handle(Key::Back, Event::Short, 1013.25, 10) == Action::None);
  assert(m.mode == Mode::List); // Cancel draft without action.
  m.handle(Key::Ok, Event::Short, 1013.25, 10);
  assert(m.draft == 1013.25);
  for (unsigned i = 0; i < 4000; ++i) m.handle(Key::Up, Event::Repeat, 1013.25, 10);
  assert(m.draft == 1100);
  assert(m.handle(Key::Ok, Event::Short, 1013.25, 10) == Action::SaveQnh);
  m.selected = 1; m.handle(Key::Ok, Event::Short, 1013.25, 10);
  for (unsigned i = 0; i < 50; ++i) m.handle(Key::Down, Event::Repeat, 1013.25, 10);
  assert(m.draft == 1);
  assert(m.handle(Key::Ok, Event::Short, 1013.25, 10) == Action::SaveAverage);
  m.selected = 3; m.handle(Key::Ok, Event::Short, 1013.25, 10);
  assert(m.mode == Mode::ConfirmZero && !m.confirm);
  assert(m.handle(Key::Ok, Event::Short, 1013.25, 10) == Action::None);
  m.handle(Key::Ok, Event::Short, 1013.25, 10);
  m.handle(Key::Up, Event::Long, 1013.25, 10);
  assert(!m.confirm); // Holding does not flip destructive confirmation.
  m.handle(Key::Up, Event::Short, 1013.25, 10);
  assert(m.confirm);
  assert(m.handle(Key::Ok, Event::Short, 1013.25, 10) == Action::Zero);
  m.selected = 4;
  assert(m.handle(Key::Ok, Event::Short, 1013.25, 10) == Action::StartBuzzerTest);
  assert(m.mode == Mode::BuzzerTest);
  assert(m.handle(Key::Down, Event::Repeat, 1013.25, 10) == Action::None);
  assert(m.handle(Key::Back, Event::Long, 1013.25, 10) == Action::StopBuzzerTest);
  assert(m.mode == Mode::List && m.selected == 4);
  assert(m.handle(Key::Ok, Event::Short, 1013.25, 10) == Action::StartBuzzerTest);
  assert(m.handle(Key::Ok, Event::Short, 1013.25, 10) == Action::StopBuzzerTest);
  m.handle(Key::Down, Event::Short, 1013.25, 10);
  assert(m.selected == 5);
  m.handle(Key::Down, Event::Short, 1013.25, 10);
  assert(m.selected == 0);
  m.handle(Key::Up, Event::Short, 1013.25, 10);
  assert(m.selected == 5);
  m.handle(Key::Ok, Event::Short, 1013.25, 10);
  assert(m.mode == Mode::Closed);
  std::puts("PASS: Bounce2, gestures, menu settings, buzzer test start/stop, six-item navigation");
}
