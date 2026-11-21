#include "SchedulingModule.h"
#include <Wire.h>
#include <stddef.h>
#include <string.h>

namespace {
constexpr uint8_t kStatusActive = 1;
constexpr uint8_t kStatusExpired = 2;

uint8_t fromBcd(uint8_t value) {
  return (value >> 4) * 10 + (value & 0x0F);
}

bool leapYear(uint16_t year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

uint8_t daysInMonth(uint16_t year, uint8_t month) {
  static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && leapYear(year)) return 29;
  return days[month - 1];
}

uint32_t toEpoch(uint16_t year, uint8_t month, uint8_t day,
                 uint8_t hour, uint8_t minute, uint8_t second) {
  uint32_t days = 0;
  for (uint16_t y = 2000; y < year; ++y) days += leapYear(y) ? 366UL : 365UL;
  for (uint8_t m = 1; m < month; ++m) days += daysInMonth(year, m);
  days += day - 1;
  return days * 86400UL + hour * 3600UL + minute * 60UL + second;
}

void fromEpoch(uint32_t epoch, uint16_t &year, uint8_t &month, uint8_t &day,
               uint8_t &hour, uint8_t &minute, uint8_t &second) {
  uint32_t days = epoch / 86400UL;
  const uint32_t daySeconds = epoch % 86400UL;
  year = 2000;
  while (days >= (leapYear(year) ? 366UL : 365UL)) {
    days -= leapYear(year) ? 366UL : 365UL;
    ++year;
  }
  month = 1;
  while (days >= daysInMonth(year, month)) {
    days -= daysInMonth(year, month);
    ++month;
  }
  day = days + 1;
  hour = daySeconds / 3600UL;
  minute = (daySeconds % 3600UL) / 60UL;
  second = daySeconds % 60UL;
}

void printDateTime(uint32_t epoch) {
  uint16_t year;
  uint8_t month, day, hour, minute, second;
  fromEpoch(epoch, year, month, day, hour, minute, second);
  Serial.print(year); Serial.print('-');
  if (month < 10) Serial.print('0'); Serial.print(month); Serial.print('-');
  if (day < 10) Serial.print('0'); Serial.print(day); Serial.print(' ');
  if (hour < 10) Serial.print('0'); Serial.print(hour); Serial.print(':');
  if (minute < 10) Serial.print('0'); Serial.print(minute); Serial.print(':');
  if (second < 10) Serial.print('0'); Serial.print(second);
}
}

const char *Schedule::scheduleName(TimeSchedule interval) {
  switch (interval) {
    case TimeSchedule::EveryFourHr: return "Every 4 Hours";
    case TimeSchedule::EverySixHr: return "Every 6 Hours";
    case TimeSchedule::EveryEightHr: return "Every 8 Hours";
    case TimeSchedule::EveryTwelveHr: return "Every 12 Hours";
  }
  return "Invalid interval";
}

const char *Schedule::durationName(DurationType type) {
  switch (type) {
    case DurationType::Days: return "Days";
    case DurationType::Weeks: return "Weeks";
    case DurationType::Months: return "Months";
  }
  return "Unknown";
}

uint32_t Schedule::intervalSeconds(TimeSchedule interval) const {
  const uint8_t hours = (uint8_t)interval;
  if (database_.mode) {
    // Preserve the old 4h=5min and 8h=8min mappings; new choices
    // use one test minute per selected production hour.
    switch (interval) {
      case TimeSchedule::EveryFourHr: return 300UL;
      case TimeSchedule::EverySixHr: return 360UL;
      case TimeSchedule::EveryEightHr: return 480UL;
      case TimeSchedule::EveryTwelveHr: return 720UL;
    }
  } else {
    return (uint32_t)hours * 3600UL;
  }
  return 0;
}

uint16_t Schedule::checksum(const Database &db) const {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&db);
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < offsetof(Database, checksum); ++i) {
    crc ^= (uint16_t)bytes[i] << 8;
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
  }
  return crc;
}

void Schedule::saveDatabase() {
  database_.magic = kDatabaseMagic;
  database_.version = kDatabaseVersion;
  database_.checksum = checksum(database_);
  EEPROM.put(kEepromAddress, database_);
}

bool Schedule::validInterval(TimeSchedule interval) const {
  return interval == TimeSchedule::EveryFourHr ||
         interval == TimeSchedule::EverySixHr ||
         interval == TimeSchedule::EveryEightHr ||
         interval == TimeSchedule::EveryTwelveHr;
}

bool Schedule::validateDuration(DurationType type, uint8_t value) const {
  switch (type) {
    case DurationType::Days: return value == 1 || value == 3 || value == 5;
    case DurationType::Weeks: return value == 1 || value == 2 || value == 3;
    case DurationType::Months: return value == 1 || value == 6 || value == 12;
  }
  return false;
}

void Schedule::loadDatabase() {
  EEPROM.get(kEepromAddress, database_);
  bool valid = database_.magic == kDatabaseMagic &&
               database_.version == kDatabaseVersion &&
               database_.mode <= 1 && database_.count <= kMaxSchedules &&
               database_.nextId != 0 && database_.checksum == checksum(database_);
  if (valid) {
    for (uint8_t i = 0; i < database_.count; ++i) {
      const ScheduleRecord &record = database_.records[i];
      if (record.id == 0 || !validInterval((TimeSchedule)record.intervalHours) ||
          !validateDuration((DurationType)record.durationType, record.durationValue) ||
          record.status != kStatusActive || record.startEpoch >= record.expirationEpoch ||
          record.nextReminderEpoch < record.startEpoch) {
        valid = false;
        break;
      }
    }
  }

  if (!valid) {
    memset(&database_, 0, sizeof(database_));
    database_.mode = TEST_MODE ? 1 : 0;
    database_.nextId = 1;
    saveDatabase();
    Serial.println(F("EEPROM schedule data invalid or uninitialized; defaults loaded."));
  }
}

bool Schedule::readRtc(uint32_t &epoch) const {
  Wire.beginTransmission(0x68);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission(false) != 0 ||
      Wire.requestFrom((uint8_t)0x68, (uint8_t)7) != 7) return false;

  const uint8_t rawSecond = Wire.read();
  const uint8_t rawMinute = Wire.read();
  const uint8_t rawHour = Wire.read();
  Wire.read();
  const uint8_t rawDay = Wire.read();
  const uint8_t rawMonth = Wire.read();
  const uint8_t rawYear = Wire.read();

  Wire.beginTransmission(0x68);
  Wire.write((uint8_t)0x0F);
  if (Wire.endTransmission(false) != 0 ||
      Wire.requestFrom((uint8_t)0x68, (uint8_t)1) != 1) return false;
  if (Wire.read() & 0x80) return false; // DS3231 oscillator-stop flag

  const uint8_t second = fromBcd(rawSecond & 0x7F);
  const uint8_t minute = fromBcd(rawMinute & 0x7F);
  uint8_t hour;
  if (rawHour & 0x40) {
    hour = fromBcd(rawHour & 0x1F) % 12;
    if (rawHour & 0x20) hour += 12;
  } else {
    hour = fromBcd(rawHour & 0x3F);
  }
  const uint8_t day = fromBcd(rawDay & 0x3F);
  const uint8_t month = fromBcd(rawMonth & 0x1F);
  const uint16_t year = 2000 + fromBcd(rawYear);
  if (second > 59 || minute > 59 || hour > 23 || month < 1 || month > 12 ||
      day < 1 || day > daysInMonth(year, month)) return false;
  epoch = toEpoch(year, month, day, hour, minute, second);
  return true;
}

uint32_t Schedule::expirationFrom(uint32_t startEpoch, DurationType type,
                                  uint8_t value) const {
  if (type == DurationType::Days) return startEpoch + (uint32_t)value * 86400UL;
  if (type == DurationType::Weeks) return startEpoch + (uint32_t)value * 7UL * 86400UL;

  uint16_t year;
  uint8_t month, day, hour, minute, second;
  fromEpoch(startEpoch, year, month, day, hour, minute, second);
  uint16_t totalMonth = year * 12U + (month - 1) + value;
  year = totalMonth / 12U;
  month = (totalMonth % 12U) + 1;
  const uint8_t lastDay = daysInMonth(year, month);
  if (day > lastDay) day = lastDay;
  return toEpoch(year, month, day, hour, minute, second);
}

void Schedule::begin() {
  Wire.begin();
  loadDatabase();
  Serial.print(F("Medication scheduler ready; mode="));
  Serial.println(isTestMode() ? F("TEST") : F("PRODUCTION"));
  Serial.print(F("Active EEPROM schedules: "));
  Serial.println(database_.count);
}

bool Schedule::createSchedule(TimeSchedule interval, DurationType durationType,
                              uint8_t durationValue, uint16_t &createdId) {
  if (!validInterval(interval) || !validateDuration(durationType, durationValue) ||
      database_.count >= kMaxSchedules) return false;
  uint32_t now;
  if (!readRtc(now)) {
    Serial.println(F("ERROR: schedule not saved; DS3231 time is invalid."));
    return false;
  }

  ScheduleRecord &record = database_.records[database_.count++];
  memset(&record, 0, sizeof(record));
  uint16_t candidateId = database_.nextId ? database_.nextId : 1;
  bool idInUse;
  do {
    idInUse = false;
    for (uint8_t i = 0; i + 1 < database_.count; ++i) {
      if (database_.records[i].id == candidateId) {
        idInUse = true;
        ++candidateId;
        if (candidateId == 0) candidateId = 1;
        break;
      }
    }
  } while (idInUse);
  record.id = candidateId;
  database_.nextId = candidateId + 1;
  if (database_.nextId == 0) database_.nextId = 1;
  record.intervalHours = (uint8_t)interval;
  record.durationType = (uint8_t)durationType;
  record.durationValue = durationValue;
  record.status = kStatusActive;
  record.startEpoch = now;
  record.expirationEpoch = expirationFrom(now, durationType, durationValue);
  record.nextReminderEpoch = now + intervalSeconds(interval);
  createdId = record.id;
  saveDatabase();

  Serial.print(F("Saved schedule #")); Serial.print(createdId);
  Serial.print(F(" ")); Serial.print(scheduleName(interval));
  Serial.print(F(" for ")); Serial.print(durationValue);
  Serial.print(' '); Serial.print(durationName(durationType));
  Serial.print(F("; starts ")); printDateTime(record.startEpoch);
  Serial.print(F("; expires ")); printDateTime(record.expirationEpoch);
  Serial.println();
  return true;
}

void Schedule::removeAt(uint8_t index) {
  if (index >= database_.count) return;
  if (acknowledgedId_ == database_.records[index].id) acknowledgedId_ = 0;
  for (uint8_t i = index; i + 1 < database_.count; ++i)
    database_.records[i] = database_.records[i + 1];
  --database_.count;
  memset(&database_.records[database_.count], 0, sizeof(ScheduleRecord));
}

bool Schedule::deleteScheduleAt(uint8_t index) {
  if (index >= database_.count) return false;
  const uint16_t deletedId = database_.records[index].id;
  removeAt(index);
  saveDatabase();
  Serial.print(F("Deleted schedule #")); Serial.println(deletedId);
  return true;
}

uint8_t Schedule::scheduleCount() const { return database_.count; }

bool Schedule::scheduleAt(uint8_t index, ScheduleRecord &record) const {
  if (index >= database_.count) return false;
  record = database_.records[index];
  return true;
}

bool Schedule::current(ScheduleRecord &record) const {
  int8_t found = -1;
  for (uint8_t i = 0; i < database_.count; ++i) {
    const ScheduleRecord &candidate = database_.records[i];
    if (candidate.status != kStatusActive || !candidate.reminderPending) continue;
    if (found < 0 || candidate.pendingEpoch < database_.records[found].pendingEpoch)
      found = i;
  }
  if (found < 0) return false;
  record = database_.records[found];
  return true;
}

bool Schedule::acknowledgeCurrent() {
  ScheduleRecord record;
  if (!current(record)) return false;
  acknowledgedId_ = record.id;
  Serial.print(F("Personnel acknowledged schedule #")); Serial.println(record.id);
  return true;
}

bool Schedule::isAcknowledged(uint16_t id) const {
  return id != 0 && acknowledgedId_ == id;
}

void Schedule::completeCurrent(uint16_t id) {
  for (uint8_t i = 0; i < database_.count; ++i) {
    ScheduleRecord &record = database_.records[i];
    if (record.id == id) {
      record.reminderPending = 0;
      record.pendingEpoch = 0;
      acknowledgedId_ = 0;
      saveDatabase();
      Serial.print(F("Dispensing request completed for schedule #")); Serial.println(id);
      return;
    }
  }
}

bool Schedule::currentTime(uint32_t &epoch) const {
  if (!rtcReady_) return false;
  epoch = currentEpoch_;
  return true;
}

bool Schedule::setTestMode(bool testMode) {
  const uint8_t requested = testMode ? 1 : 0;
  if (database_.mode == requested) return true;
  if (acknowledgedId_ != 0) {
    Serial.println(F("Mode change ignored while dispensing."));
    return false;
  }
  for (uint8_t i = 0; i < database_.count; ++i) {
    if (database_.records[i].reminderPending) {
      Serial.println(F("Mode change ignored while a reminder is pending."));
      return false;
    }
  }
  uint32_t now;
  if (!readRtc(now)) {
    Serial.println(F("ERROR: mode not changed; DS3231 time is invalid."));
    return false;
  }
  database_.mode = requested;
  for (uint8_t i = 0; i < database_.count; ++i)
    database_.records[i].nextReminderEpoch = now +
      intervalSeconds((TimeSchedule)database_.records[i].intervalHours);
  saveDatabase();
  Serial.print(F("Operating mode changed to "));
  Serial.println(testMode ? F("TEST") : F("PRODUCTION"));
  return true;
}

void Schedule::processSchedules(uint32_t now) {
  bool changed = false;
  uint8_t index = 0;
  while (index < database_.count) {
    ScheduleRecord &record = database_.records[index];
    if (now >= record.expirationEpoch) {
      if (record.id == acknowledgedId_) {
        // Finish a dispense already acknowledged before expiring its record.
        ++index;
        continue;
      }
      record.status = kStatusExpired;
      Serial.print(F("Schedule expired and removed from EEPROM: #"));
      Serial.println(record.id);
      removeAt(index);
      changed = true;
      continue;
    }

    const uint32_t interval = intervalSeconds((TimeSchedule)record.intervalHours);
    if (record.reminderPending) {
      if (now >= record.pendingEpoch && now - record.pendingEpoch >= interval) {
        Serial.print(F("Unacknowledged reminder expired safely: #"));
        Serial.println(record.id);
        record.reminderPending = 0;
        record.pendingEpoch = 0;
        if (acknowledgedId_ == record.id) acknowledgedId_ = 0;
        while (record.nextReminderEpoch <= now)
          record.nextReminderEpoch += interval;
        changed = true;
      }
    } else if (now >= record.nextReminderEpoch) {
      const uint32_t lateBy = now - record.nextReminderEpoch;
      if (lateBy >= interval) {
        const uint32_t skipped = lateBy / interval + 1;
        record.nextReminderEpoch += skipped * interval;
        Serial.print(F("Missed reminder skipped; schedule #")); Serial.println(record.id);
      } else {
        record.reminderPending = 1;
        record.pendingEpoch = record.nextReminderEpoch;
        record.nextReminderEpoch += interval;
        Serial.print(F("Reminder due for schedule #")); Serial.println(record.id);
      }
      changed = true;
    }
    ++index;
  }
  if (changed) saveDatabase();
}

void Schedule::loop() {
  const uint32_t tick = millis();
  if (hasPolledRtc_ && (uint32_t)(tick - lastRtcPoll_) < 250UL) return;
  hasPolledRtc_ = true;
  lastRtcPoll_ = tick;

  uint32_t now;
  if (!readRtc(now)) {
    if (rtcReady_) Serial.println(F("ERROR: DS3231 unavailable; schedule clock paused."));
    rtcReady_ = false;
    return;
  }
  rtcReady_ = true;
  currentEpoch_ = now;
  processSchedules(now);
}
