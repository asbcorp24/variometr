#include "flight_journal.h"
#include "LittleFS.h"
#include "Arduino.h"
#include <cassert>
#include <cstdio>
MemoryStore memoryStore;
LittleFSClass LittleFS;
JournalSerial Serial;
bool partitionErased=false;
size_t partitionDirtyOffset=SIZE_MAX,highestPartitionRead=0;
namespace logflash { bool connected = true; }
int main(){
  logflash::connected=false;FlightJournal absent;assert(!absent.begin()&&memoryStore.formats==0);
  logflash::connected=true;
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
  // Never delete while recording, and preserve unrelated files/settings.
  memoryStore=MemoryStore{};FlightJournal clearing;assert(clearing.begin());
  assert(clearing.start(0,0,false,0,1013,0));
  assert(!clearing.clear()&&clearing.active()&&clearing.count()==1&&memoryStore.nodes.size()==1);
  assert(clearing.stop(500));
  assert(clearing.start(1000,0,false,0,1013,0));assert(clearing.stop(1500));
  for(const char *path:{"/settings.json","/f1.csv","/f00000003.csv"}) {auto f=LittleFS.open(path,"w");f.close();}
  memoryStore.failRemovePath="/f00000002.csv";
  assert(!clearing.clear());assert(clearing.count()==1&&clearing.record(0).id==2);
  assert(!LittleFS.exists("/f00000001.csv")&&LittleFS.exists("/f00000002.csv"));
  memoryStore.failRemovePath.clear();
  assert(clearing.clear()&&clearing.count()==0&&clearing.error()[0]==0);
  assert(LittleFS.exists("/settings.json")&&LittleFS.exists("/f1.csv"));
  assert(!LittleFS.exists("/f00000003.csv")&&memoryStore.formats==0);
  FlightJournal afterClear;assert(afterClear.begin()&&afterClear.count()==0);
  assert(afterClear.clear());assert(afterClear.start(2000,0,false,0,1013,0)&&afterClear.current().id==1);
  assert(afterClear.stop(2500));
  memoryStore=MemoryStore{};FlightJournal full;assert(full.begin());assert(full.start(0,0,false,0,1013,0));
  memoryStore.capacity=memoryStore.used()+12000;full.sample(1000,0,0,100000,4);
  assert(!full.active()&&full.current().end==flightlog::End::StorageFull);
  memoryStore=MemoryStore{};memoryStore.mounted=false;
  FlightJournal damaged;assert(!damaged.begin()&&memoryStore.formats==0); // Never erase existing/unrecognized storage.
  partitionErased=true;partitionDirtyOffset=16u*1024u*1024u+123;
  FlightJournal dirtyHigh;assert(!dirtyHigh.begin()&&memoryStore.formats==0);
  assert(highestPartitionRead>16u*1024u*1024u); // Upper half must be checked before auto-format.
  partitionDirtyOffset=SIZE_MAX;FlightJournal fresh;assert(fresh.begin()&&memoryStore.formats==1);
  std::puts("PASS: actual journal file I/O, reboot recovery, power loss, numbering, capacity, write failure, full storage, non-destructive mount, confirmed clear, partial remove failure, unrelated files preserved, nonblank upper 16 MiB protected");
}
