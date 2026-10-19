#pragma once
#include <cstdint>

namespace calendar {
struct DateTime { int year = 2026, month = 1, day = 1, hour = 0, minute = 0, second = 0; };
inline bool leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }
inline int monthDays(int y, int m) {
  static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  return m < 1 || m > 12 ? 0 : days[m - 1] + (m == 2 && leap(y));
}
inline bool valid(const DateTime &d) {
  return d.year >= 1970 && d.year <= 2199 && d.month >= 1 && d.month <= 12 &&
         d.day >= 1 && d.day <= monthDays(d.year, d.month) && d.hour >= 0 &&
         d.hour < 24 && d.minute >= 0 && d.minute < 60 && d.second >= 0 && d.second < 60;
}
inline int64_t epoch(const DateTime &d) {
  if (!valid(d)) return -1;
  int64_t days = 0;
  for (int y = 1970; y < d.year; ++y) days += leap(y) ? 366 : 365;
  for (int m = 1; m < d.month; ++m) days += monthDays(d.year, m);
  return ((days + d.day - 1) * 24 + d.hour) * 3600 + d.minute * 60 + d.second;
}
inline DateTime civil(int64_t seconds) {
  DateTime d;
  if (seconds < 0) return d;
  int64_t days = seconds / 86400;
  d.hour = (seconds / 3600) % 24; d.minute = (seconds / 60) % 60; d.second = seconds % 60;
  d.year = 1970;
  while (days >= (leap(d.year) ? 366 : 365)) days -= leap(d.year++) ? 366 : 365;
  d.month = 1;
  while (days >= monthDays(d.year, d.month)) days -= monthDays(d.year, d.month++);
  d.day = int(days) + 1;
  return d;
}
inline bool validZone(int minutes) { return minutes >= -720 && minutes <= 840 && minutes % 15 == 0; }
inline void adjust(DateTime &d, int field, int delta) {
  switch (field) {
    case 0: d.year += delta; if (d.year < 2000) d.year = 2099; if (d.year > 2099) d.year = 2000; break;
    case 1: d.month = (d.month - 1 + delta + 12) % 12 + 1; break;
    case 2: d.day = (d.day - 1 + delta + monthDays(d.year, d.month)) % monthDays(d.year, d.month) + 1; break;
    case 3: d.hour = (d.hour + delta + 24) % 24; break;
    case 4: d.minute = (d.minute + delta + 60) % 60; break;
    case 5: d.second = (d.second + delta + 60) % 60; break;
  }
  if (d.day > monthDays(d.year, d.month)) d.day = monthDays(d.year, d.month);
}
inline uint8_t bcd(int value) { return uint8_t((value / 10) * 16 + value % 10); }
inline int unbcd(uint8_t b) { return (b & 15) > 9 || (b >> 4) > 9 ? -1 : (b >> 4) * 10 + (b & 15); }
inline bool decodeRtc(const uint8_t *r, DateTime &d) {
  if ((r[0] & 0x80) || (r[1] & 0x80) || (r[2] & 0x80) || r[3] < 1 || r[3] > 7) return false;
  d.second = unbcd(r[0]); d.minute = unbcd(r[1]);
  int h = unbcd(r[2] & ((r[2] & 0x40) ? 0x1f : 0x3f));
  if (r[2] & 0x40) { if (h < 1 || h > 12) return false; h = h % 12 + ((r[2] & 0x20) ? 12 : 0); }
  d.hour = h; d.day = unbcd(r[4]); d.month = unbcd(r[5] & 0x1f);
  const int y = unbcd(r[6]); if (y < 0) return false;
  d.year = 2000 + y + ((r[5] & 0x80) ? 100 : 0);
  return valid(d);
}
inline void encodeRtc(const DateTime &d, uint8_t *r) {
  r[0] = bcd(d.second); r[1] = bcd(d.minute); r[2] = bcd(d.hour);
  r[3] = uint8_t((epoch(d) / 86400 + 4) % 7 + 1); // Sunday=1.
  r[4] = bcd(d.day); r[5] = bcd(d.month) | (d.year >= 2100 ? 0x80 : 0);
  r[6] = bcd(d.year % 100);
}
}
