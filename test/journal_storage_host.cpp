#include "flight_journal.h"
#include "LittleFS.h"
#include "Arduino.h"
#include <cassert>
#include <cstdio>
MemoryStore memoryStore;
LittleFSClass LittleFS;
JournalSerial Serial;
bool partitionErased=false;
int main(){
  FlightJournal j;assert(j.begin());
  assert(j.start(0,1700000000,true,180,1013.25,100));
  for(uint32_t t=100;t<=6300;t+=100)j.sample(t,100+t*0.001,1,100000,4);
  assert(j.active()&&j.count()==1&&j.current().maxAltitude>106);
  memoryStore.crash(); // Drop unflushed tail, just like sudden loss of power.
  FlightJournal recovered;assert(recovered.begin());assert(recovered.count()==1);
  assert(recovered.record(0).end==flightlog::End::PowerLoss&&recovered.record(0).durationMs==5000);
  assert(!recovered.active());
  assert(recovered.start(10000,1700000010,true,180,1013.25,200));
  recovered.sample(11000,210,2,99000,3.9);assert(recovered.stop(12500));
  FlightJournal reopened;assert(reopened.begin());assert(reopened.count()==2);
  assert(reopened.record(0).id==2&&reopened.record(0).durationMs==2500&&reopened.record(0).maxAltitude==210);
  assert(reopened.record(0).end==flightlog::End::Manual&&reopened.record(1).end==flightlog::End::PowerLoss);
  assert(reopened.start(20000,0,false,180,1013,300));memoryStore.failWrites=true;
  reopened.sample(21000,310,1,99000,4);assert(!reopened.active()&&reopened.current().end==flightlog::End::IoError);
  memoryStore.failWrites=false;
  memoryStore=MemoryStore{};
  FlightJournal many;assert(many.begin());
  for(unsigned i=0;i<64;++i){assert(many.start(i*1000,0,false,0,1013,0));assert(many.stop(i*1000+500));}
  assert(many.count()==64&&!many.start(100000,0,false,0,1013,0));
  memoryStore=MemoryStore{};FlightJournal full;assert(full.begin());assert(full.start(0,0,false,0,1013,0));
  memoryStore.capacity=memoryStore.used()+12000;full.sample(1000,0,0,100000,4);
  assert(!full.active()&&full.current().end==flightlog::End::StorageFull);
  memoryStore=MemoryStore{};memoryStore.mounted=false;
  FlightJournal damaged;assert(!damaged.begin()&&memoryStore.formats==0); // Never erase existing/unrecognized storage.
  partitionErased=true;FlightJournal fresh;assert(fresh.begin()&&memoryStore.formats==1);
  std::puts("PASS: actual journal file I/O, reboot recovery, power loss, numbering, capacity, write failure, full storage, non-destructive mount");
}
