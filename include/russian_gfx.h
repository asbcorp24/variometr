#pragma once
#include <cstddef>
#include <cstdint>

namespace ru {
// Original 5x7 uppercase Cyrillic bitmaps, rows from top to bottom.
constexpr uint8_t font[33][7] = {
 {14,17,17,31,17,17,17}, {31,16,16,30,17,17,30},
 {30,17,17,30,17,17,30}, {31,16,16,16,16,16,16},
 {6,10,10,10,10,31,17}, {31,16,16,30,16,16,31},
 {21,21,14,4,14,21,21}, {14,17,1,6,1,17,14},
 {17,17,19,21,25,17,17}, {10,4,17,19,21,25,17},
 {17,18,20,24,20,18,17}, {7,9,9,9,9,9,17},
 {17,27,21,21,17,17,17}, {17,17,17,31,17,17,17},
 {14,17,17,17,17,17,14}, {31,17,17,17,17,17,17},
 {30,17,17,30,16,16,16}, {14,17,16,16,16,17,14},
 {31,4,4,4,4,4,4}, {17,17,17,15,1,17,14},
 {4,14,21,21,21,14,4}, {17,17,10,4,10,17,17},
 {18,18,18,18,18,31,1}, {17,17,17,15,1,1,1},
 {21,21,21,21,21,21,31}, {21,21,21,21,21,31,1},
 {24,8,8,14,9,9,14}, {17,17,17,25,21,21,25},
 {16,16,16,30,17,17,30}, {14,17,1,7,1,17,14},
 {18,21,21,29,21,21,18}, {15,17,17,15,5,9,17},
 {10,0,31,16,30,16,31}
};
inline size_t length(const char *s) {
  size_t n = 0;
  for (; *s; ++s) if ((uint8_t(*s) & 0xc0) != 0x80) ++n;
  return n;
}
template<class Base> class Display : public Base {
 public:
  using Base::Base;
  using Base::write;
  size_t write(uint8_t ch) override {
    if (ch < 0x80) { lead = 0; return Base::write(ch); }
    if (ch == 0xd0 || ch == 0xd1) { lead = ch; return 1; }
    if (!lead || (ch & 0xc0) != 0x80) { lead = 0; Base::write('?'); return 1; }
    uint16_t code = ((lead & 0x1f) << 6) | (ch & 0x3f);
    lead = 0;
    if (code >= 0x430 && code <= 0x44f) code -= 32;
    if (code == 0x451) code = 0x401;
    const int index = code == 0x401 ? 32 : int(code) - 0x410;
    if (index < 0 || index > 32) { Base::write('?'); return 1; }
    if (this->wrap && this->cursor_x + 6 * this->textsize_x > this->_width) {
      this->cursor_x = 0; this->cursor_y += 8 * this->textsize_y;
    }
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 6; ++x) {
      const bool ink = y < 7 && x < 5 && (font[index][y] & (16 >> x));
      if (ink || this->textcolor != this->textbgcolor)
        this->fillRect(this->cursor_x + x * this->textsize_x,
                       this->cursor_y + y * this->textsize_y,
                       this->textsize_x, this->textsize_y,
                       ink ? this->textcolor : this->textbgcolor);
    }
    this->cursor_x += 6 * this->textsize_x;
    return 1;
  }
 private:
  uint8_t lead = 0;
};
}
