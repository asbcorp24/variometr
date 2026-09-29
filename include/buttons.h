#pragma once
#include <cstdint>

namespace buttons {
enum class Event { None, Short, Long, Repeat };
enum class Key { Back, Up, Down, Ok };

// Receives the already debounced state from Bounce2; only classifies gestures.
class Gestures {
 public:
  void begin(bool down, uint32_t now) {
    stable = down; blocked = down; pressed = repeated = now; longSent = false;
  }
  Event update(bool down, uint32_t now, bool repeat) {
    if (down != stable) {
      stable = down;
      if (blocked) { if (!stable) blocked = false; return Event::None; }
      if (stable) { pressed = repeated = now; longSent = false; }
      else return longSent ? Event::None : Event::Short;
    }
    if (!stable || blocked) return Event::None;
    if (!longSent && uint32_t(now - pressed) >= 800) {
      longSent = true; repeated = now; return Event::Long;
    }
    if (longSent && repeat && uint32_t(now - repeated) >= 120) {
      repeated = now; return Event::Repeat;
    }
    return Event::None;
  }
 private:
  bool stable = false, blocked = false, longSent = false;
  uint32_t pressed = 0, repeated = 0;
};

enum class Mode { Closed, List, Qnh, Average, ConfirmZero, BuzzerTest };
enum class Action { None, Page, ToggleSound, SaveQnh, SaveAverage, Zero, StartBuzzerTest, StopBuzzerTest };
struct Menu {
  Mode mode = Mode::Closed;
  int selected = 0;
  double draft = 0;
  bool confirm = false;
  void close() { mode = Mode::Closed; }
  Action handle(Key key, Event event, double qnh, double average) {
    if (event == Event::None) return Action::None;
    if (mode == Mode::BuzzerTest) {
      if ((key == Key::Back || key == Key::Ok) &&
          (event == Event::Short || event == Event::Long)) {
        mode = Mode::List;
        return Action::StopBuzzerTest;
      }
      return Action::None;
    }
    if (key == Key::Back && event == Event::Long) return Action::ToggleSound;
    if (mode == Mode::Closed) {
      if (key == Key::Back && event == Event::Short) return Action::Page;
      if (key == Key::Ok && (event == Event::Short || event == Event::Long)) {
        mode = Mode::List; selected = 0;
      }
      return Action::None;
    }
    if (key == Key::Back && event == Event::Short) {
      mode = mode == Mode::List ? Mode::Closed : Mode::List;
      return Action::None;
    }
    if (key == Key::Up || key == Key::Down) {
      const int direction = key == Key::Up ? 1 : -1;
      if (mode == Mode::List) selected = (selected - direction + 6) % 6;
      if (mode == Mode::Qnh) {
        draft += direction * 0.1;
        if (draft < 800) draft = 800;
        if (draft > 1100) draft = 1100;
      }
      if (mode == Mode::Average) {
        draft += direction;
        if (draft < 1) draft = 1;
        if (draft > 30) draft = 30;
      }
      if (mode == Mode::ConfirmZero && event == Event::Short) confirm = !confirm;
      return Action::None;
    }
    if (key != Key::Ok || event != Event::Short) return Action::None;
    if (mode == Mode::Qnh) { mode = Mode::List; return Action::SaveQnh; }
    if (mode == Mode::Average) { mode = Mode::List; return Action::SaveAverage; }
    if (mode == Mode::ConfirmZero) {
      mode = Mode::List;
      return confirm ? Action::Zero : Action::None;
    }
    switch (selected) {
      case 0: draft = qnh; mode = Mode::Qnh; break;
      case 1: draft = average; mode = Mode::Average; break;
      case 2: return Action::ToggleSound;
      case 3: confirm = false; mode = Mode::ConfirmZero; break;
      case 4: mode = Mode::BuzzerTest; return Action::StartBuzzerTest;
      case 5: close(); break;
    }
    return Action::None;
  }
};
}
