#pragma once
#include "flight_record.h"
#include "calendar.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace flightlog {
inline bool verify(char *line) {
  char *mark = strrchr(line, '*');
  if (!mark || strlen(mark + 1) != 8) return false;
  char *end;
  const uint32_t expected = strtoul(mark + 1, &end, 16);
  if (*end || crc32(line, mark - line) != expected) return false;
  *mark = 0;
  return true;
}
struct Recovery {
  Record record;
  bool header = false, finalized = false;
  bool consume(char *line) {
    if (finalized || !verify(line)) return false;
    int end = 0;
    if (!header) {
      unsigned schema, id, trusted;
      long long utc;
      if (sscanf(line,"H,%u,%u,%lld,%d,%u,%f,%f%n",&schema,&id,&utc,&record.zoneMinutes,&trusted,
                 &record.qnh,&record.startAltitude,&end) != 7 || line[end] || schema != 1 || !id || trusted > 1 ||
          !calendar::validZone(record.zoneMinutes) || !std::isfinite(record.qnh) || record.qnh < 800 || record.qnh > 1100 ||
          !std::isfinite(record.startAltitude) || std::fabs(record.startAltitude)>20000 || (trusted && (utc<=0 || utc>7258118399LL))) return false;
      record.id=id;record.startUtc=utc;record.clockTrusted=trusted;record.maxAltitude=record.startAltitude;
      record.end=End::PowerLoss;header=true;return true;
    }
    unsigned elapsed,samples,gaps,reason;
    float altitude,gain,climb,sink,pressure,voltage;
    if(sscanf(line,"S,%u,%f,%f,%f,%f%n",&elapsed,&altitude,&climb,&pressure,&voltage,&end)==5&&!line[end]) {
      if(elapsed<record.durationMs||!validSample(altitude,climb,pressure))return false;
      record.durationMs=elapsed;record.maxAltitude=std::max(record.maxAltitude,altitude);
      record.maxGain=std::max(record.maxGain,altitude-record.startAltitude);
      record.maxClimb=std::max(record.maxClimb,climb);record.minClimb=std::min(record.minClimb,climb);return true;
    }
    end=0;
    if(sscanf(line,"G,%u%n",&elapsed,&end)==1&&!line[end]) {
      if(elapsed<record.durationMs)return false;
      record.durationMs=elapsed;return true;
    }
    end=0;
    if((line[0]!='C'&&line[0]!='E')||sscanf(line+1,",%u,%f,%f,%f,%f,%u,%u,%u%n",&elapsed,&altitude,&gain,&climb,&sink,
        &samples,&gaps,&reason,&end)!=8||line[1+end]||elapsed<record.durationMs||!std::isfinite(altitude)||
        !std::isfinite(gain)||!std::isfinite(climb)||!std::isfinite(sink)||std::fabs(altitude)>20000||gain<0||
        climb<0||climb>200||sink>0||sink< -200||reason>4||(line[0]=='E'&&reason==0))return false;
    record.durationMs=elapsed;record.maxAltitude=std::max(record.maxAltitude,altitude);record.maxGain=std::max(record.maxGain,gain);
    record.maxClimb=std::max(record.maxClimb,climb);record.minClimb=std::min(record.minClimb,sink);record.samples=samples;record.gaps=gaps;
    if(line[0]=='E'){record.end=End(reason);finalized=true;}return true;
  }
};
}
