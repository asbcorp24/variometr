#include "russian_gfx.h"
#include <cassert>
#include <cstdio>
#include <vector>
struct Pixel { int x, y, w, h, color; };
struct Surface {
  int cursor_x = 0, cursor_y = 0, _width = 240;
  uint8_t textsize_x = 1, textsize_y = 1;
  uint16_t textcolor = 1, textbgcolor = 0;
  bool wrap = false;
  std::vector<Pixel> pixels;
  virtual size_t write(uint8_t) { cursor_x += 6 * textsize_x; return 1; }
  void fillRect(int x, int y, int w, int h, int color) { pixels.push_back({x,y,w,h,color}); }
};
void print(ru::Display<Surface> &s, const char *text) {
  for (; *text; ++text) s.write(uint8_t(*text));
}
int main() {
  ru::Display<Surface> s;
  s.write(0xd0); assert(s.cursor_x == 0);
  s.write(0x90); assert(s.cursor_x == 6); // Two UTF-8 bytes, one glyph.
  assert(s.pixels.size() == 48);
  assert(s.pixels[0].color == 0 && s.pixels[1].color == 1); // Top of А.
  ru::Display<Surface> lower; print(lower, "а");
  for (size_t i = 0; i < s.pixels.size(); ++i) assert(s.pixels[i].color == lower.pixels[i].color);
  ru::Display<Surface> alphabet;
  print(alphabet, "АБВГДЕЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯЁ");
  assert(alphabet.cursor_x == 33 * 6);
  assert(ru::length("QNH 1013.25 СРЕД. 10с") == 21);
  ru::Display<Surface> scaled; scaled.textsize_x = scaled.textsize_y = 2;
  print(scaled, "ЗВУК"); assert(scaled.cursor_x == 48);
  for (const auto &p : scaled.pixels) assert(p.w == 2 && p.h == 2);
  std::puts("PASS: UTF-8 Cyrillic, full alphabet, lowercase, glyph width, scaling");
}
