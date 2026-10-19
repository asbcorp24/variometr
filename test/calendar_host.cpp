#include "calendar.h"
#include <cassert>
#include <cstdio>
int main(){
  using namespace calendar;
  assert(leap(2000)&&leap(2024)&&!leap(2100));
  DateTime d;d.year=2024;d.month=2;d.day=29;d.hour=23;d.minute=59;d.second=58;
  assert(valid(d));assert(civil(epoch(d)).day==29);
  for(int y=2000;y<=2100;++y)for(int m=1;m<=12;++m){
    d.year=y;d.month=m;d.day=monthDays(y,m);
    const auto c=civil(epoch(d));assert(c.year==y&&c.month==m&&c.day==d.day&&c.second==58);
  }
  d.year=2026;d.month=1;d.day=1;d.hour=0;d.minute=15;d.second=0;
  const auto utc=civil(epoch(d)-330*60);assert(utc.year==2025&&utc.month==12&&utc.day==31&&utc.hour==18&&utc.minute==45);
  assert(civil(epoch(utc)+330*60).day==1);
  d.year=2024;d.month=1;d.day=31;adjust(d,1,1);assert(d.month==2&&d.day==29);
  adjust(d,0,1);assert(d.year==2025&&d.day==28);
  uint8_t raw[7];encodeRtc(d,raw);DateTime decoded;assert(decodeRtc(raw,decoded));assert(epoch(decoded)==epoch(d));
  raw[1]=0x6a;assert(!decodeRtc(raw,decoded));
  encodeRtc(d,raw);raw[2]=0x52;assert(decodeRtc(raw,decoded)&&decoded.hour==0); // 12 AM
  raw[2]=0x72;assert(decodeRtc(raw,decoded)&&decoded.hour==12); // 12 PM
  raw[2]=0x70;assert(decodeRtc(raw,decoded)&&decoded.hour==22); // 10 PM
  assert(validZone(-720)&&validZone(840)&&validZone(345)&&!validZone(841)&&!validZone(1));
  std::puts("PASS: calendar, leap days, RTC BCD/12h, timezone midnight/year boundaries, editing");
}
