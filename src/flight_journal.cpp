#include "flight_journal.h"
#include "journal_codec.h"
#include "external_log_flash.h"
#include <LittleFS.h>
#include <esp_partition.h>
#include <Arduino.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace {
bool journalName(const char *path, unsigned &id) {
  const char *name = strrchr(path, '/');
  name = name ? name + 1 : path;
  if (strlen(name) != 13 || name[0] != 'f' || strcmp(name + 9, ".csv") != 0) return false;
  for (int i = 1; i <= 8; ++i) if (name[i] < '0' || name[i] > '9') return false;
  id = strtoul(name + 1, nullptr, 10);
  return true;
}
bool erasedPartition() {
  const auto *p = logflash::partition();
  if (!p) return false;
  uint8_t block[4096];
  for (size_t offset = 0; offset < p->size; offset += sizeof(block)) {
    const size_t size = std::min(sizeof(block), p->size - offset);
    if (esp_partition_read(p, offset, block, size) != ESP_OK) return false;
    for (size_t i = 0; i < size; ++i) if (block[i] != 0xff) return false;
    if (!(offset % 16384)) yield();
  }
  return true;
}
bool readLine(File &file, char *line, size_t capacity) {
  size_t length = 0;
  bool overflow = false;
  while (file.available()) {
    const char ch = file.read();
    if (ch == '\n') {
      line[length] = 0;
      return !overflow;
    }
    if (ch == '\r') continue;
    if (length + 1 < capacity) line[length++] = ch;
    else overflow = true;
  }
  line[0] = 0; // Unterminated power-loss tail is never trusted.
  return false;
}
}
void FlightJournal::fault(const char *message) {
  snprintf(error_, sizeof(error_), "%s", message);
  Serial.printf("Flight journal: %s\n", error_);
  ++revision_;
}
size_t FlightJournal::freeBytes() const {
  return ready_ ? LittleFS.totalBytes() - LittleFS.usedBytes() : 0;
}
bool FlightJournal::begin() {
  if (active()) return false;
  count_ = 0; nextId_ = 1; error_[0] = 0;
  session_ = flightlog::Session{};
  // Never auto-format a partition that contains data.
  if (!logflash::begin()) { ready_ = false; fault("НЕТ ВНЕШНЕЙ FLASH"); return false; }
  ready_ = LittleFS.begin(false, "/flightlogs", 10, logflash::Label);
  if (!ready_) {
    Serial.printf("External log flash: mount failed; checking entire chip for erased bytes before format\n");
    if (erasedPartition()) ready_ = LittleFS.format() && LittleFS.begin(false, "/flightlogs", 10, logflash::Label);
  }
  if (!ready_) { fault("НЕ УДАЛОСЬ ОТКРЫТЬ ПАМЯТЬ"); return false; }
  File root = LittleFS.open("/");
  if (!root) { ready_ = false; fault("ОШИБКА КАТАЛОГА"); return false; }
  while (true) {
    File file = root.openNextFile();
    if (!file) break;
    unsigned id = 0;
    const bool matches = journalName(file.name(), id);
    if (matches) {
      nextId_ = std::max(nextId_, uint32_t(id + 1));
      flightlog::Record record;
      if (count_ < Capacity && load(file, record)) records_[count_++] = record;
      else fault("ЧАСТЬ ЗАПИСЕЙ НЕ ПРОЧИТАНА");
    }
    file.close(); yield();
  }
  std::sort(records_, records_ + count_, [](const flightlog::Record &a, const flightlog::Record &b) { return a.id < b.id; });
  ++revision_;
  return true;
}
bool FlightJournal::clear() {
  if (active()) { fault("СНАЧАЛА ЗАВЕРШИТЕ ПОЛЁТ"); return false; }
  if (!ready_) { fault("ПАМЯТЬ НЕДОСТУПНА"); return false; }
  bool ok = true;
  // Close the directory before removing a file; reopen it after every deletion.
  // This also removes damaged journal files that could not be loaded into records_.
  while (true) {
    File root = LittleFS.open("/");
    if (!root) { ok = false; break; }
    char path[16] = {};
    while (true) {
      File entry = root.openNextFile();
      if (!entry) break;
      unsigned id = 0;
      const bool match = journalName(entry.name(), id);
      if (match) snprintf(path, sizeof(path), "/f%08u.csv", id);
      entry.close();
      if (match) break;
    }
    root.close();
    if (!path[0]) break;
    if (!LittleFS.remove(path)) { ok = false; break; }
    yield();
  }
  // Reload surviving records even after a partial deletion or a remove error.
  const bool loaded = begin();
  if (!ok) fault("НЕ ВСЕ ЛОГИ УДАЛЕНЫ");
  return ok && loaded;
}
bool FlightJournal::load(File &file, flightlog::Record &r) {
  flightlog::Recovery recovery;
  char line[256];
  if (!readLine(file,line,sizeof(line)) || !recovery.consume(line)) return false;
  unsigned count=0;
  while(file.available()) {
    if(readLine(file,line,sizeof(line))) recovery.consume(line);
    if(++count%64==0)yield();
  }
  r=recovery.record;
  return recovery.header;
}
bool FlightJournal::append(const char *format, ...) {
  char line[256];
  va_list args; va_start(args, format);
  const int length = vsnprintf(line, sizeof(line) - 12, format, args);
  va_end(args);
  if (length < 0 || length >= int(sizeof(line) - 12)) return false;
  const uint32_t crc = flightlog::crc32(line, length);
  const int suffix = snprintf(line + length, sizeof(line) - length, "*%08lx\n", static_cast<unsigned long>(crc));
  return file_.write(reinterpret_cast<const uint8_t *>(line), length + suffix) == size_t(length + suffix);
}
bool FlightJournal::checkpoint(char kind, flightlog::End reason) {
  const auto &r = session_.record;
  return append("%c,%lu,%.3f,%.3f,%.3f,%.3f,%lu,%lu,%u", kind,
                static_cast<unsigned long>(r.durationMs), r.maxAltitude, r.maxGain, r.maxClimb, r.minClimb,
                static_cast<unsigned long>(r.samples), static_cast<unsigned long>(r.gaps), unsigned(reason));
}
bool FlightJournal::start(uint32_t now, int64_t utc, bool trusted, int zone, float qnh, float altitude) {
  if (active()) return false;
  if (!ready_) { fault("ПАМЯТЬ НЕДОСТУПНА"); return false; }
  if (count_ >= Capacity || nextId_ > 99999999 || freeBytes() < 65536) { fault("ЖУРНАЛ ЗАПОЛНЕН"); return false; }
  char path[32]; snprintf(path, sizeof(path), "/f%08lu.csv", static_cast<unsigned long>(nextId_));
  if (LittleFS.exists(path)) { fault("ФАЙЛ УЖЕ СУЩЕСТВУЕТ"); return false; }
  file_ = LittleFS.open(path, FILE_WRITE);
  if (!file_) { fault("ОШИБКА СОЗДАНИЯ ФАЙЛА"); return false; }
  flightlog::Record r;
  r.id = nextId_++; r.startUtc = trusted ? utc : 0; r.clockTrusted = trusted; r.zoneMinutes = zone;
  r.qnh = qnh; r.startAltitude = r.maxAltitude = altitude;
  session_.begin(r, now);
  if (!append("H,1,%lu,%lld,%d,%u,%.3f,%.3f", static_cast<unsigned long>(r.id),
              static_cast<long long>(r.startUtc), zone, trusted ? 1 : 0, qnh, altitude)) {
    file_.close(); session_.active = false; fault("ОШИБКА ЗАГОЛОВКА"); return false;
  }
  file_.flush(); records_[count_++] = r;
  sampleAt_ = flushAt_ = now; error_[0] = 0; ++revision_;
  return true;
}
bool FlightJournal::stop(uint32_t now, flightlog::End reason) {
  if (!active()) return false;
  session_.tick(now); session_.record.end = reason;
  const bool ok = checkpoint('E', reason);
  file_.flush(); file_.close(); session_.active = false;
  if (!ok) { session_.record.end = flightlog::End::IoError; fault("ОШИБКА ЗАВЕРШЕНИЯ"); }
  records_[count_ - 1] = session_.record; ++revision_;
  return ok;
}
void FlightJournal::tick(uint32_t now) {
  if (!active()) return;
  session_.tick(now);
  records_[count_ - 1] = session_.record;
}
void FlightJournal::sample(uint32_t now, float altitude, float velocity, float pressure, float voltage) {
  if (!active()) return;
  tick(now); session_.sample(altitude, velocity, pressure);
  records_[count_ - 1] = session_.record;
  if (uint32_t(now - sampleAt_) < 1000) return;
  sampleAt_ = now;
  if (freeBytes() < 16384) { stop(now, flightlog::End::StorageFull); fault("ПАМЯТЬ ЗАПОЛНЕНА"); return; }
  bool ok;
  if (flightlog::validSample(altitude, velocity, pressure))
    ok = append("S,%lu,%.3f,%.3f,%.2f,%.3f", static_cast<unsigned long>(session_.record.durationMs),
                altitude, velocity, pressure, std::isfinite(voltage) ? voltage : -1.0f);
  else ok = append("G,%lu", static_cast<unsigned long>(session_.record.durationMs));
  if (!ok) { stop(now, flightlog::End::IoError); fault("ОШИБКА ЗАПИСИ"); return; }
  if (uint32_t(now - flushAt_) >= 5000) {
    if (!checkpoint('C', flightlog::End::Active)) { stop(now, flightlog::End::IoError); fault("ОШИБКА СВОДКИ"); return; }
    file_.flush(); flushAt_ = now; ++revision_;
  }
}
const flightlog::Record &FlightJournal::record(size_t newestIndex) const {
  static const flightlog::Record empty;
  return newestIndex < count_ ? records_[count_ - 1 - newestIndex] : empty;
}
