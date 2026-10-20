#include "diagnostic_display.h"
#include "screen_host_panel.h"
#include <cassert>
#include <cstdio>
SerialStub Serial;
SPIStub SPI;
DisplayPanel *DisplayPanel::instance=nullptr;
bool FlightJournal::begin(){ready_=true;return true;}
bool FlightJournal::start(uint32_t now,int64_t utc,bool trusted,int zone,float qnh,float height){
  flightlog::Record r;r.id=1;r.startUtc=utc;r.clockTrusted=trusted;r.zoneMinutes=zone;r.qnh=qnh;r.startAltitude=r.maxAltitude=height;
  r.maxAltitude=1320;r.maxGain=340;r.maxClimb=5.7;r.minClimb=-3.1;r.durationMs=136000;
  session_.begin(r,now);records_[0]=r;count_=1;return true;
}
const flightlog::Record &FlightJournal::record(size_t)const{return records_[0];}
int main(){
  screen::begin();
  FlightJournal journal;journal.begin();journal.start(0,1792411200,true,180,1013.25,980);
  DeviceClock clock;battery::State battery;battery.updateIna(3.9,0.35,1,10000);
  vario::Filter f;for(uint32_t t=0;t<=10000;t+=100)f.update(100000-t*0.01,t);
  screen::set(screen::Rtc,"2026-10-19 15:30:45");
  screen::drawVario(f,10000,true,journal,true);screen::drawBattery(battery);DisplayPanel::instance->save(".pio/ui/vario.bmp");
  compass::Heading heading;heading.update(0,100,10000);screen::drawCompass(heading,10000,true);screen::drawBattery(battery);DisplayPanel::instance->save(".pio/ui/compass.bmp");
  buttons::Menu menu;menu.mode=buttons::Mode::List;
  auto draw=[&](const char *path){screen::drawMenu(menu,1013.25,10,true,"",false,journal,0,clock,battery,true);screen::drawBattery(battery);DisplayPanel::instance->save(path);};
  draw(".pio/ui/menu.bmp");menu.selected=11;draw(".pio/ui/menu_last.bmp");
  menu.mode=buttons::Mode::Clock;menu.date.year=2026;menu.date.month=10;menu.date.day=19;menu.dateField=4;draw(".pio/ui/clock.bmp");
  screen::drawJournal(journal,0,clock,"",true);screen::drawBattery(battery);DisplayPanel::instance->save(".pio/ui/journal.bmp");
  FlightJournal empty;empty.begin();screen::drawJournal(empty,0,clock,"",true);screen::drawBattery(battery);DisplayPanel::instance->save(".pio/ui/journal_empty.bmp");
  screen::drawJournal(journal,0,clock,"ЗАПИСЬ ПОЛЁТА НАЧАТА",false);screen::drawBattery(battery);DisplayPanel::instance->save(".pio/ui/journal_updated.bmp");
  menu.mode=buttons::Mode::Timezone;menu.draft=330;draw(".pio/ui/timezone.bmp");
  menu.mode=buttons::Mode::BatteryCells;menu.draft=1;draw(".pio/ui/battery.bmp");
  menu.mode=buttons::Mode::ConfirmFlight;draw(".pio/ui/flight_confirm.bmp");
  menu.mode=buttons::Mode::ConfirmClearLogs;draw(".pio/ui/clear_logs.bmp");
  assert(DisplayPanel::instance->clipped==0);
  std::puts("PASS: actual screen renderer, 320x480 pages, no clipped pixels; BMP previews in .pio/ui");
}
