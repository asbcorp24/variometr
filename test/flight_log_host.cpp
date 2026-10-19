#include "flight_record.h"
#include "journal_codec.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
bool consume(flightlog::Recovery &r,const char *payload){
  char line[256];snprintf(line,sizeof(line),"%s*%08lx",payload,static_cast<unsigned long>(flightlog::crc32(payload,strlen(payload))));
  return r.consume(line);
}
int main(){
  using namespace flightlog;
  assert(crc32("123456789",9)==0xcbf43926);
  Record r;r.id=1;r.startAltitude=r.maxAltitude=-100;r.startUtc=100;
  Session s;s.begin(r,UINT32_MAX-100);s.tick(100);assert(s.record.durationMs==201);
  s.sample(-50,2,100000);s.sample(-120,-5,100000);s.sample(NAN,0,100000);
  assert(s.record.maxAltitude==-50&&s.record.maxGain==50&&s.record.maxClimb==2&&s.record.minClimb==-5&&s.record.gaps==1);
  s.tick(1100);assert(s.record.durationMs==1201); // Independent of wall clock.
  Recovery powerLoss;
  assert(consume(powerLoss,"H,1,1,1700000000,180,1,1013.25,-100"));
  assert(consume(powerLoss,"S,1000,-90,1,100000,4.0"));
  assert(consume(powerLoss,"C,5000,-80,20,3,-2,50,1,0"));
  assert(consume(powerLoss,"S,6000,-70,2,100000,3.9"));
  assert(powerLoss.record.end==End::PowerLoss&&powerLoss.record.maxAltitude==-70&&powerLoss.record.durationMs==6000);
  char damaged[]="E,10000,100,200,5,-3,100,0,1*00000000";
  assert(!powerLoss.consume(damaged));assert(powerLoss.record.end==End::PowerLoss);
  assert(!consume(powerLoss,"S,2000,100,1,100000,4")); // Older record cannot rewind duration.
  assert(!consume(powerLoss,"S,7000,nan,1,100000,4"));
  assert(consume(powerLoss,"G,8000"));assert(powerLoss.record.durationMs==8000);
  assert(consume(powerLoss,"E,10000,100,200,5,-3,100,1,1"));
  assert(powerLoss.record.end==End::Manual&&powerLoss.record.maxGain==200);
  assert(!consume(powerLoss,"S,20000,500,50,100000,4")); // Closed flight never resurrects.
  Recovery bad;assert(!consume(bad,"H,2,1,1700000000,180,1,1013,0"));
  Recovery noClock;assert(consume(noClock,"H,1,2,0,180,0,1013,0"));assert(!noClock.record.clockTrusted);
  std::puts("PASS: flight extrema, clock-independent duration, timer wrap, checksums, power loss, corrupted tails, finalization");
}
