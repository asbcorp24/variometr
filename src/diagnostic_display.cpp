#include "diagnostic_display.h"
#include "russian_gfx.h"
#include "display_panel.h"
#include "hardware_config.h"
#include <Arduino.h>
#include <Adafruit_ILI9341.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace screen {
namespace {
ru::Display<DisplayPanel> tft;
bool started = false;
int page = -1;
char values[RowCount][96] = {"ОЖИДАНИЕ","ОЖИДАНИЕ","ОЖИДАНИЕ","ОЖИДАНИЕ","ОЖИДАНИЕ","ЗАПУСК"};
char cache[24][192] = {};
char batteryPrevious[80] = {};
char menuPrevious[384] = {};
const char *labels[RowCount] = {"ДАВЛЕНИЕ / ТЕМПЕРАТУРА", "BMI160 УСКОРЕНИЕ XYZ", "BMI160 ВРАЩЕНИЕ XYZ",
  "МАГНИТНОЕ ПОЛЕ XYZ", "ЧАСЫ (! = НУЖНА НАСТРОЙКА)", "ДИАГНОСТИКА"};
bool large() { return tft.width() >= 320; }
void text(int x, int y, int size, uint16_t color, const char *value) {
  tft.setTextSize(size); tft.setTextColor(color); tft.setCursor(x,y); tft.print(value);
}
void clear() {
  tft.fillScreen(ILI9341_BLACK);
  memset(cache,0,sizeof(cache)); batteryPrevious[0] = menuPrevious[0] = 0;
}
bool header(int target, const char *title, bool force) {
  if (page != target) force = true;
  page = target;
  if (force) {
    clear(); text(8,8,2,ILI9341_WHITE,title);
    tft.drawFastHLine(8,large()?37:25,tft.width()-16,ILI9341_DARKGREY);
  }
  return force;
}
void footer(const char *value) { text(8,tft.height()-15,1,ILI9341_YELLOW,value); }
void field(int key, int x, int y, int w, int h, int size, uint16_t color, const char *value, bool force) {
  if (!force && strcmp(cache[key],value)==0) return;
  snprintf(cache[key],sizeof(cache[key]),"%s",value);
  tft.fillRect(x,y,w,h,ILI9341_BLACK);
  while (size > 1 && ru::length(value)*6*size > unsigned(w)) --size;
  text(x,y,size,color,value);
}
void duration(char *out, size_t size, uint64_t ms) {
  const unsigned long seconds=ms/1000;
  snprintf(out,size,"%02lu:%02lu:%02lu",seconds/3600,(seconds/60)%60,seconds%60);
}
const char *ending(flightlog::End end) {
  switch(end) {
    case flightlog::End::Active: return "ИДЁТ ЗАПИСЬ";
    case flightlog::End::Manual: return "ЗАВЕРШЁН";
    case flightlog::End::PowerLoss: return "ОБРЫВ ПИТАНИЯ / НЕ ЗАВЕРШЁН";
    case flightlog::End::StorageFull: return "ПАМЯТЬ ЗАПОЛНЕНА";
    default: return "ОШИБКА ЗАПИСИ";
  }
}
void zoneText(char *out,size_t size,int minutes) {
  const int a=minutes<0?-minutes:minutes;
  snprintf(out,size,"UTC%c%02d:%02d",minutes<0?'-':'+',a/60,a%60);
}
}
void set(Row row,const char *format,...) {
  va_list args; va_start(args,format); vsnprintf(values[row],sizeof(values[row]),format,args); va_end(args);
}
void drawBattery(const battery::State &state) {
  if(!started) return;
  const bool bad=state.mode==battery::Mode::Error;
  const bool demo=state.mode==battery::Mode::Demo;
  const bool known=state.mode==battery::Mode::Measured || demo;
  char percent[20],detail[32],signature[80];
  if(known) snprintf(percent,sizeof(percent),"%s%d%%",state.estimated?"~":"",int(state.percent+0.5f));
  else strcpy(percent,"--%");
  if(bad) strcpy(detail,"ОШИБКА");
  else if(demo) strcpy(detail,"ДЕМО");
  else snprintf(detail,sizeof(detail),"%.2f В",state.voltage);
  snprintf(signature,sizeof(signature),"%s %s",percent,detail);
  if(strcmp(signature,batteryPrevious)==0) return;
  strcpy(batteryPrevious,signature);
  const int x=tft.width()-86;
  const uint16_t color=bad?ILI9341_RED:demo?ILI9341_YELLOW:known&&state.percent<=20?ILI9341_ORANGE:ILI9341_GREEN;
  tft.fillRect(x,2,84,22,ILI9341_BLACK);
  tft.drawRect(x+2,4,19,10,color); tft.fillRect(x+21,7,2,4,color);
  if(known) { const int w=int(state.percent*15/100+0.5f); if(w>0)tft.fillRect(x+4,6,w,6,color); }
  text(x+27,4,1,color,percent); text(x+27,15,1,color,detail);
}
void draw(bool force) {
  if(!started)return;
  force=header(0,"ДИАГНОСТИКА",force);
  const int top=large()?56:35,step=large()?60:40;
  if(force) {
    for(int i=0;i<RowCount;++i) text(8,top+i*step,1,ILI9341_CYAN,labels[i]);
    footer("НАЗАД: ЭКРАН  ОК: НАСТРОЙКИ");
  }
  for(int i=0;i<RowCount;++i)
    field(i,8,top+14+i*step,tft.width()-16,large()?24:16,large()?2:1,ILI9341_WHITE,values[i],force);
}
void drawVario(const vario::Filter &f,uint32_t now,bool sound,const FlightJournal &journal,bool force) {
  if(!started)return;
  force=header(1,"ВАРИОМЕТР",force);
  const bool big=large(),fresh=f.fresh(now),ready=f.ready(now);
  char s[9][128] = {};
  if(ready) { snprintf(s[0],sizeof(s[0]),"%+.1f",f.velocity); snprintf(s[1],sizeof(s[1]),"%+.1f",f.average); }
  else { strcpy(s[0],fresh?"ЖДИТЕ":"---"); strcpy(s[1],"---"); }
  if(fresh) { snprintf(s[2],sizeof(s[2]),"%.0f",f.height); snprintf(s[3],sizeof(s[3]),"%+.0f",f.relative()); }
  else { strcpy(s[2],"---");strcpy(s[3],"---"); }
  snprintf(s[4],sizeof(s[4]),"QNH %.2f  СРЕД. %.0fс",f.qnh,f.averageSeconds);
  snprintf(s[5],sizeof(s[5]),"%s",values[Rtc]);
  snprintf(s[6],sizeof(s[6]),"%s | %s",ready?"ГОТОВ":fresh?"ПРОГРЕВ":"ОШИБКА БАРО",sound?"ЗВУК ВКЛ":"БЕЗ ЗВУКА");
  char time[24]; duration(time,sizeof(time),journal.current().durationMs);
  snprintf(s[7],sizeof(s[7]),journal.active()?"ЗАПИСЬ #%lu  %s":"ПОЛЁТОВ: %lu",static_cast<unsigned long>(journal.active()?journal.current().id:journal.count()),time);
  if(!journal.ready() || journal.error()[0]) snprintf(s[8],sizeof(s[8]),"%s",journal.error());
  if(force) {
    text(8,big?71:45,big?2:1,ILI9341_CYAN,"ВЕРТ. СКОРОСТЬ м/с");
    text(8,big?160:110,big?2:1,ILI9341_CYAN,"СРЕДНЯЯ м/с");
    text(8,big?211:155,1,ILI9341_CYAN,"ВЫСОТА QNH / м");
    text(big?168:125,big?211:155,1,ILI9341_CYAN,"ОТНОСИТ. / м");
    text(8,big?313:235,1,ILI9341_CYAN,"ЧАСЫ (! = НУЖНА НАСТРОЙКА)");
    footer("НАЗАД: ЭКРАН  ОК: НАСТРОЙКИ");
  }
  const int x[9]={8,big?192:125,8,big?168:125,8,8,8,8,8};
  const int y[9]={big?99:62,big?157:108,big?231:173,big?231:173,big?278:213,big?335:253,big?48:28,big?389:274,big?421:287};
  const int w[9]={tft.width()-16,big?120:110,big?144:110,big?144:110,tft.width()-16,tft.width()-16,tft.width()-16,tft.width()-16,tft.width()-16};
  const int size[9]={big?5:4,big?3:2,big?3:2,big?3:2,big?2:1,big?2:1,1,1,1};
  for(int i=0;i<9;++i) field(i,x[i],y[i],w[i],size[i]*8,size[i],i==0&&ready?(f.velocity>=0?ILI9341_GREEN:ILI9341_ORANGE):i==7&&journal.active()?ILI9341_RED:ILI9341_WHITE,s[i],force);
}
void drawJournal(const FlightJournal &journal,size_t index,const DeviceClock &clock,const char *notice,bool force) {
  if(!started)return;
  force=header(4,"ЖУРНАЛ",force);
  const bool big=large();
  const int top=big?56:37,dy=big?31:22,w=tft.width()-16;
  char rows[13][160]={},time[32];
  if(journal.active()) {
    duration(time,sizeof(time),journal.current().durationMs);
    snprintf(rows[0],sizeof(rows[0]),"ЗАПИСЬ #%lu  %s",static_cast<unsigned long>(journal.current().id),time);
  } else strcpy(rows[0],"ЗАПИСЬ ОСТАНОВЛЕНА");
  uint64_t total=0;unsigned complete=0;
  for(size_t i=0;i<journal.count();++i){total+=journal.record(i).durationMs;if(journal.record(i).end==flightlog::End::Manual)++complete;}
  snprintf(rows[1],sizeof(rows[1]),"ВСЕГО: %u  ЗАВЕРШЕНО: %u",unsigned(journal.count()),complete);
  duration(time,sizeof(time),total);
  snprintf(rows[2],sizeof(rows[2]),"ОБЩЕЕ ВРЕМЯ: %s",time);
  if(!journal.ready())strcpy(rows[3],"ПАМЯТЬ НЕДОСТУПНА");
  else if(!journal.count())strcpy(rows[3],"ПОЛЁТОВ ПОКА НЕТ");
  else {
    if(index>=journal.count())index=journal.count()-1;
    const auto &r=journal.record(index);
    snprintf(rows[3],sizeof(rows[3]),"ПОЛЁТ #%lu  %u/%u",static_cast<unsigned long>(r.id),unsigned(index+1),unsigned(journal.count()));
    if(r.clockTrusted) {
      const auto d=calendar::civil(r.startUtc+int64_t(clock.zoneMinutes)*60);
      char zone[24];zoneText(zone,sizeof(zone),clock.zoneMinutes);
      snprintf(rows[4],sizeof(rows[4]),"%04d-%02d-%02d %02d:%02d:%02d %s",d.year,d.month,d.day,d.hour,d.minute,d.second,zone);
    } else strcpy(rows[4],"ДАТА НЕ ЗАДАНА");
    duration(time,sizeof(time),r.durationMs);snprintf(rows[5],sizeof(rows[5]),"ДЛИТЕЛЬНОСТЬ %s",time);
    snprintf(rows[6],sizeof(rows[6]),"МАКС. ВЫСОТА %.1f м",r.maxAltitude);
    snprintf(rows[7],sizeof(rows[7]),"МАКС. НАБОР %.1f м",r.maxGain);
    snprintf(rows[8],sizeof(rows[8]),"НАБОР %+.1f / СПУСК %+.1f м/с",r.maxClimb,r.minClimb);
    snprintf(rows[9],sizeof(rows[9]),"%s",ending(r.end));
  }
  strcpy(rows[10],"ВВЕРХ/ВНИЗ: ДРУГОЙ ПОЛЁТ");
  snprintf(rows[11],sizeof(rows[11]),"%s",journal.error()[0]?journal.error():notice?notice:"");
  strcpy(rows[12],journal.active()?"ОК: ЗАВЕРШИТЬ ЗАПИСЬ":"ОК: НАЧАТЬ ЗАПИСЬ");
  for(int i=0;i<13;++i) {
    const int y=i<3?top+i*17:i<10?top+60+(i-3)*dy:i==10?tft.height()-81:i==11?tft.height()-65:tft.height()-49;
    const int size=big&&((i>=3&&i<=9)||i==12)?2:1;
    const uint16_t color=i==0?(journal.active()?ILI9341_RED:ILI9341_CYAN):i==9||i==11?ILI9341_YELLOW:ILI9341_WHITE;
    field(i,8,y,w,size*8,size,color,rows[i],force);
  }
  if(force){text(8,tft.height()-33,1,ILI9341_CYAN,"ДОЛГОЕ ОК: НАСТРОЙКИ");footer("НАЗАД: СЛЕДУЮЩИЙ ЭКРАН");}
}
void drawMenu(const buttons::Menu &menu,double qnh,double average,bool sound,const char *notice,bool testRunning,
              const FlightJournal &journal,size_t journalIndex,const DeviceClock &clock,const battery::State &battery,bool force) {
  if(!started)return;
  const bool big=large();
  char signature[384];
  snprintf(signature,sizeof(signature),"%d/%d/%.2f/%d/%d/%d/%d/%d/%d/%d/%d/%u/%u/%u/%d/%d/%.2f/%s",int(menu.mode),menu.selected,menu.draft,menu.confirm,
      menu.date.year,menu.date.month,menu.date.day,menu.date.hour,menu.date.minute,menu.date.second,menu.dateField,
      unsigned(journal.revision()),unsigned(journalIndex),unsigned(journal.current().durationMs/1000),sound,testRunning,battery.voltage,notice);
  if(page!=2)force=true;
  if(!force&&strcmp(signature,menuPrevious)==0)return;
  const char *title=menu.mode==buttons::Mode::Clock?"ДАТА И ВРЕМЯ":"НАСТРОЙКИ";
  header(2,title,true); strcpy(menuPrevious,signature);
  const int top=big?60:42,step=big?38:30;
  if(menu.mode==buttons::Mode::List) {
    char rows[buttons::MenuCount][80];
    snprintf(rows[0],sizeof(rows[0]),"QNH %.2f гПа",qnh);
    snprintf(rows[1],sizeof(rows[1]),"СРЕДНЕЕ %.0f с",average);
    snprintf(rows[2],sizeof(rows[2]),"ЗВУК %s",sound?"ВКЛ":"ВЫКЛ");
    strcpy(rows[3],"ОБНУЛИТЬ ВЫСОТУ"); strcpy(rows[4],"ТЕСТ ПИЩАЛКИ");
    strcpy(rows[5],"ДАТА И ВРЕМЯ");strcpy(rows[6],"ЧАСОВОЙ ПОЯС");
    strcpy(rows[7],journal.active()?"ЗАВЕРШИТЬ ПОЛЁТ":"НАЧАТЬ ПОЛЁТ");strcpy(rows[8],"ЖУРНАЛ ПОЛЁТОВ");
    strcpy(rows[9],"АККУМУЛЯТОР");strcpy(rows[10],"НАЗАД");
    const int visible=big?9:6, first=(menu.selected/visible)*visible;
    for(int i=first;i<buttons::MenuCount&&i<first+visible;++i) {
      const int y=top+(i-first)*step;
      if(i==menu.selected)tft.fillRect(4,y-4,tft.width()-8,28,ILI9341_DARKCYAN);
      text(8,y,2,ILI9341_WHITE,rows[i]);
    }
    char position[40];snprintf(position,sizeof(position),"%d/%d  ВВЕРХ/ВНИЗ: ВЫБОР",menu.selected+1,buttons::MenuCount);
    text(8,tft.height()-49,1,ILI9341_CYAN,position);
  } else if(menu.mode==buttons::Mode::Clock) {
    const char *names[]={"ГОД","МЕСЯЦ","ДЕНЬ","ЧАСЫ","МИНУТЫ","СЕКУНДЫ"};
    const int v[]={menu.date.year,menu.date.month,menu.date.day,menu.date.hour,menu.date.minute,menu.date.second};
    for(int i=0;i<6;++i) {
      if(i==menu.dateField)tft.fillRect(4,top+i*step-4,tft.width()-8,28,ILI9341_DARKCYAN);
      char row[64];snprintf(row,sizeof(row),"%s: %02d",names[i],v[i]);text(8,top+i*step,2,ILI9341_WHITE,row);
    }
    text(8,tft.height()-49,1,ILI9341_YELLOW,"ОК: СЛЕДУЮЩЕЕ / СОХРАНИТЬ");
  } else if(menu.mode==buttons::Mode::Timezone) {
    text(8,top,2,ILI9341_WHITE,"ЧАСОВОЙ ПОЯС");
    char zone[32];zoneText(zone,sizeof(zone),int(menu.draft));text(8,top+48,big?3:2,ILI9341_GREEN,zone);
    text(8,top+100,1,ILI9341_WHITE,"ВВЕРХ/ВНИЗ: ШАГ 15 МИНУТ");
    text(8,top+122,1,ILI9341_WHITE,"UTC -12:00 ... +14:00");
    text(8,top+144,1,ILI9341_WHITE,"ВРЕМЯ В ЖУРНАЛЕ ХРАНИТСЯ В UTC");
  } else if(menu.mode==buttons::Mode::ConfirmZero||menu.mode==buttons::Mode::ConfirmFlight) {
    const char *prompt=menu.mode==buttons::Mode::ConfirmZero?"ОБНУЛИТЬ ВЫСОТУ?":journal.active()?"ЗАВЕРШИТЬ ПОЛЁТ?":"НАЧАТЬ ПОЛЁТ?";
    text(8,top,2,ILI9341_WHITE,prompt);
    if(menu.mode==buttons::Mode::ConfirmFlight&&!clock.trusted(millis()))text(8,top+40,1,ILI9341_YELLOW,"ДАТА НЕ ЗАДАНА: ЖУРНАЛ БЕЗ ДАТЫ");
    tft.fillRect(menu.confirm?tft.width()/2+4:4,top+88,tft.width()/2-8,32,ILI9341_DARKCYAN);
    text(20,top+96,2,ILI9341_WHITE,"НЕТ");text(tft.width()/2+20,top+96,2,ILI9341_WHITE,"ДА");
  } else if(menu.mode==buttons::Mode::BuzzerTest) {
    text(8,top,2,ILI9341_WHITE,"ТЕСТ ПИЩАЛКИ");text(8,top+42,2,ILI9341_YELLOW,testRunning?"ЗВУЧИТ...":"ТЕСТ ЗАВЕРШЁН");
    text(8,top+90,1,ILI9341_WHITE,"ИМПУЛЬС, ЗАТЕМ 700/1400/2100 Гц");
    text(8,top+112,1,ILI9341_WHITE,"ПОСЛЕ ТЕСТА ЗВУК ВАРИО ВЫКЛЮЧЕН");
  } else {
    const bool cells=menu.mode==buttons::Mode::BatteryCells,q=menu.mode==buttons::Mode::Qnh;
    text(8,top,2,ILI9341_WHITE,cells?"АККУМУЛЯТОР / S":q?"QNH / гПа":"УСРЕДНЕНИЕ / с");
    char value[40];snprintf(value,sizeof(value),cells?"%.0fS":q?"%.2f":"%.0f",menu.draft);
    text(8,top+40,4,ILI9341_GREEN,value);
    text(8,top+92,1,ILI9341_WHITE,cells?"ПОСЛЕДОВАТЕЛЬНЫХ ЯЧЕЕК: 1..6":q?"ШАГ 0.1 гПа, 800..1100":"ШАГ 1 с, 1..30");
    if(cells){
      text(8,top+114,1,ILI9341_YELLOW,"ТОЛЬКО Li-ion 4.2 В НА ЯЧЕЙКУ");
      text(8,top+136,1,ILI9341_YELLOW,"ПРОЦЕНТ ~: ОЦЕНКА ПО НАПРЯЖЕНИЮ");
      if(!std::isfinite(battery.voltage))strcpy(value,"НЕТ ДАННЫХ ДАТЧИКА");
      else if(std::isfinite(battery.currentAmps))snprintf(value,sizeof(value),"%.2f В  %.3f А",battery.voltage,battery.currentAmps);
      else snprintf(value,sizeof(value),"%.2f В / ТОК НЕ ЗАДАН",battery.voltage);
      text(8,top+158,1,ILI9341_WHITE,value);
      snprintf(value,sizeof(value),"ЁМКОСТЬ: %u мАч",BATTERY_CAPACITY_MAH);
      text(8,top+180,1,ILI9341_WHITE,value);
    }
  }
  if(notice&&notice[0])text(8,tft.height()-33,1,ILI9341_YELLOW,notice);
  footer(menu.mode==buttons::Mode::BuzzerTest?"ОК/НАЗАД: СТОП И ВОЗВРАТ":"ОК: ПРИНЯТЬ  НАЗАД: ОТМЕНА");
}
void drawCompass(const compass::Heading &heading,uint32_t now,bool force) {
  if(!started)return;
  force=header(3,"КОМПАС",force);
  const bool big=large(),fresh=heading.fresh(now);
  const int degrees=fresh?int(heading.degrees+0.5f)%360:-1;
  static int oldDegrees=-2;
  if(force){text(8,big?48:30,1,ILI9341_YELLOW,"БЕЗ КАЛИБРОВКИ. ДЕРЖИТЕ РОВНО.");footer("НАЗАД: ЭКРАН  ОК: НАСТРОЙКИ");}
  if(!force&&degrees==oldDegrees)return;
  oldDegrees=degrees;
  static ru::Display<GFXcanvas1> dial(big?256:176,big?256:176);
  if(!dial.getBuffer()){text(8,70,1,ILI9341_RED,"НЕТ ПАМЯТИ ДЛЯ КОМПАСА");return;}
  const int D=dial.width(),c=D/2,R=c-9;
  dial.fillScreen(0);dial.drawCircle(c,c,R,1);dial.fillTriangle(c-6,1,c+6,1,c,10,1);
  if(fresh){
    for(int bearing=0;bearing<360;bearing+=15){const float a=(bearing-degrees)*PI/180;
      const int inner=R-(bearing%90==0?12:6);
      dial.drawLine(c+lroundf(sinf(a)*inner),c-lroundf(cosf(a)*inner),c+lroundf(sinf(a)*(R-1)),c-lroundf(cosf(a)*(R-1)),1);}
    const char *names[]={"С","В","Ю","З"};dial.setTextSize(big?3:2);dial.setTextColor(1);
    for(int i=0;i<4;++i){const float a=(i*90-degrees)*PI/180;dial.setCursor(c-(big?9:6)+lroundf(sinf(a)*(R-25)),c-(big?12:8)-lroundf(cosf(a)*(R-25)));dial.print(names[i]);}
    dial.drawLine(c,c+14,c,c-15,1);dial.fillTriangle(c,c-22,c-6,c-11,c+6,c-11,1);
  }else{dial.setTextSize(1);dial.setTextColor(1);dial.setCursor(c-30,c-4);dial.print("НЕТ ДАННЫХ");}
  uint16_t line[256];const auto *bits=dial.getBuffer();
  for(int y=0;y<D;++y){for(int x=0;x<D;++x)line[x]=(bits[y*(D/8)+x/8]&(0x80>>(x%8)))?ILI9341_WHITE:ILI9341_BLACK;
    tft.drawRGBBitmap((tft.width()-D)/2,(big?74:58)+y,line,D,1);}
  char value[48];const char *names[]={"С","СВ","В","ЮВ","Ю","ЮЗ","З","СЗ"};
  if(fresh)snprintf(value,sizeof(value),"%03d гр.  %s",degrees,names[((degrees+22)/45)%8]);else strcpy(value,"--- гр.");
  field(20,16,big?365:263,tft.width()-32,big?32:24,big?3:2,ILI9341_CYAN,value,true);
}
void geometryTest(uint8_t rotation){
  if(!started)return;
  tft.setRotation(rotation);clear();page=-1;
  const int w=tft.width(),h=tft.height();
  tft.drawRect(0,0,w,h,ILI9341_WHITE);
  for(int y=20;y<h-1;y+=20)tft.drawFastHLine(1,y,w-2,ILI9341_BLUE);
  tft.fillRect(2,2,12,12,ILI9341_RED);tft.fillRect(w-14,2,12,12,ILI9341_GREEN);
  tft.fillRect(2,h-14,12,12,ILI9341_BLUE);tft.fillRect(w-14,h-14,12,12,ILI9341_YELLOW);
  char value[40];snprintf(value,sizeof(value),"R%u: %d x %d",rotation%4,w,h);
  text(20,36,2,ILI9341_WHITE,"ТЕСТ ЭКРАНА");text(20,66,2,ILI9341_WHITE,value);footer("g: ПОВОРОТ  t: ВОЗВРАТ");
  Serial.printf("Display R%u: width=%d height=%d\n",rotation%4,w,h);
}
void endGeometryTest(){tft.setRotation(DisplayPanel::DefaultRotation);page=-1;}
void begin(){
  SPI.begin(18,19,23,27);tft.begin(DisplayPanel::SpiFrequency);tft.setRotation(DisplayPanel::DefaultRotation);tft.setTextWrap(false);
  Serial.printf("Display: width=%d height=%d rotation=%u\n",tft.width(),tft.height(),DisplayPanel::DefaultRotation);
  started=true;draw(true);
}
}
