#include "SchedulingModule.h"
#include <Wire.h>

namespace {
constexpr uint8_t kEepromMagicAddress = 0;
constexpr uint8_t kEepromMaskAddress = 1;
constexpr uint8_t kEepromModeAddress = 2;
constexpr uint8_t kEepromMagic = 0xB7;
constexpr uint8_t kEepromModeTest = 0x54;
constexpr uint8_t kEepromModeProduction = 0x50;

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

void printClock(uint32_t epoch) {
  const uint32_t daySeconds = epoch % 86400UL;
  const uint32_t days = epoch / 86400UL;
  uint16_t year = 2000;
  uint32_t remaining = days;
  while (remaining >= (leapYear(year) ? 366UL : 365UL)) {
    remaining -= leapYear(year) ? 366UL : 365UL;
    ++year;
  }
  uint8_t month = 1;
  while (remaining >= daysInMonth(year, month)) remaining -= daysInMonth(year, month++);
  const uint8_t day = remaining + 1;
  const uint8_t hour = daySeconds / 3600UL;
  const uint8_t minute = (daySeconds % 3600UL) / 60UL;
  const uint8_t second = daySeconds % 60UL;
  if (month < 10) Serial.print('0'); Serial.print(month); Serial.print('/');
  if (day < 10) Serial.print('0'); Serial.print(day); Serial.print('/');
  Serial.print(year); Serial.print(' ');
  if (hour < 10) Serial.print('0'); Serial.print(hour); Serial.print(':');
  if (minute < 10) Serial.print('0'); Serial.print(minute); Serial.print(':');
  if (second < 10) Serial.print('0'); Serial.print(second);
}

void printEpoch(uint32_t epoch) { printClock(epoch); }
}

const char *Schedule::scheduleName(TimeSchedule schedule) {
  switch (schedule) {
    case TimeSchedule::EveryOneHr: return "EveryOneHr";
    case TimeSchedule::EveryTwoHr: return "EveryTwoHr";
    case TimeSchedule::EveryFourHr: return "EveryFourHr";
    case TimeSchedule::EveryEightHr: return "EveryEightHr";
  }
  return "Unknown";
}

uint32_t Schedule::intervalSeconds(TimeSchedule schedule) const {
if (testMode_) {
  switch (schedule) {
    case TimeSchedule::EveryOneHr: return 75UL;
    case TimeSchedule::EveryTwoHr: return 140UL;
    case TimeSchedule::EveryFourHr: return 300UL;
    case TimeSchedule::EveryEightHr: return 480UL;
  }
} else {
  switch (schedule) {
    case TimeSchedule::EveryOneHr: return 3600UL;
    case TimeSchedule::EveryTwoHr: return 7200UL;
    case TimeSchedule::EveryFourHr: return 14400UL;
    case TimeSchedule::EveryEightHr: return 28800UL;
  }
}
  return 0;
}

void Schedule::loadRegistrations() {
  uint8_t mask = 0;
  if (EEPROM.read(kEepromMagicAddress) == kEepromMagic)
    mask = EEPROM.read(kEepromMaskAddress) & 0x0F;
  const uint8_t storedMode = EEPROM.read(kEepromModeAddress);
  if (storedMode == kEepromModeTest) testMode_ = true;
  else if (storedMode == kEepromModeProduction) testMode_ = false;
  for (uint8_t i = 0; i < kScheduleCount; ++i)
    enabled_[i] = (mask & (1U << i)) != 0;
}

bool Schedule::readRtc(uint32_t &epoch) const {
  Wire.beginTransmission(0x68); // DS3231 I2C address
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)0x68, (uint8_t)7) != 7) return false;

  const uint8_t rawSecond = Wire.read();
  const uint8_t rawMinute = Wire.read();
  const uint8_t rawHour = Wire.read();
  Wire.read(); // weekday
  const uint8_t rawDay = Wire.read();
  const uint8_t rawMonth = Wire.read();
  const uint8_t rawYear = Wire.read();

  // The DS3231 oscillator-stop flag lives in status register 0x0F, not in
  // the seconds register. Treat an unset clock as unavailable until repaired.
  Wire.beginTransmission(0x68);
  Wire.write((uint8_t)0x0F);
  if (Wire.endTransmission(false) != 0 ||
      Wire.requestFrom((uint8_t)0x68, (uint8_t)1) != 1) return false;
  const uint8_t status = Wire.read();
  if (status & 0x80) return false;

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

void Schedule::begin() {
  Wire.begin();
  loadRegistrations();
  Serial.println(F("Medication scheduler starting; DS3231 is the time source."));
  Serial.print(F("Operating mode: "));
  Serial.println(testMode_ ? F("TEST") : F("PRODUCTION"));
  Serial.println(F("Schedule setup: ready state; release after 5-<10s to register, or hold 10s to unregister."));
  Serial.println(F("Mode button pin 39: release 3-<5s for TEST, hold 5s for PRODUCTION."));
}

bool Schedule::setRegistration(TimeSchedule schedule, bool registered) {
  const uint8_t index = (uint8_t)schedule;
  if (index >= kScheduleCount || queued_[index]) return false;
  if (enabled_[index] == registered) {
    Serial.print(F("Schedule already "));
    Serial.print(registered ? F("registered: ") : F("unregistered: "));
    Serial.println(scheduleName(schedule));
    return true;
  }

  uint32_t now;
  if (!readRtc(now)) {
    Serial.println(F("ERROR: cannot change registered schedules until the DS3231 time is valid."));
    return false;
  }

  enabled_[index] = registered;
  if (enabled_[index]) {
    nextDue_[index] = now + intervalSeconds(schedule);
  } else {
    nextDue_[index] = 0;
  }

  uint8_t mask = 0;
  for (uint8_t i = 0; i < kScheduleCount; ++i)
    if (enabled_[i]) mask |= (1U << i);
  EEPROM.update(kEepromMaskAddress, mask);
  EEPROM.update(kEepromMagicAddress, kEepromMagic);

  Serial.print(F("Schedule "));
  Serial.print(scheduleName(schedule));
  Serial.print(enabled_[index] ? F(" registered; interval=") : F(" unregistered; interval="));
  Serial.print(intervalSeconds(schedule));
  Serial.println(F(" seconds."));
  if (enabled_[index]) {
    Serial.print(F("First reminder due: "));
    printEpoch(nextDue_[index]);
    Serial.println();
  }
  return true;
}

bool Schedule::setTestMode(bool testMode) {
  if (testMode_ == testMode) {
    Serial.print(F("Operating mode already "));
    Serial.println(testMode ? F("TEST") : F("PRODUCTION"));
    return true;
  }
  if (count_ != 0) {
    Serial.println(F("Mode change ignored while a reminder is pending or dispensing."));
    return false;
  }

  uint32_t now;
  if (!readRtc(now)) {
    Serial.println(F("ERROR: cannot change mode until DS3231 time is valid."));
    return false;
  }

  testMode_ = testMode;
  EEPROM.update(kEepromModeAddress, testMode_ ? kEepromModeTest : kEepromModeProduction);
  for (uint8_t i = 0; i < kScheduleCount; ++i)
    nextDue_[i] = enabled_[i] ? now + intervalSeconds((TimeSchedule)i) : 0;

  Serial.print(F("Operating mode changed to "));
  Serial.println(testMode_ ? F("TEST") : F("PRODUCTION"));
  printScheduleStatus(now);
  return true;
}

void Schedule::enqueue(TimeSchedule schedule, uint32_t dueAt) {
  const uint8_t index = (uint8_t)schedule;
  if (queued_[index] || count_ >= kScheduleCount) return;
  queue_[tail_] = schedule;
  tail_ = (tail_ + 1) % kScheduleCount;
  ++count_;
  queued_[index] = true;
  acknowledged_[index] = false;
  pendingSince_[index] = dueAt;
  Serial.print(F("Reminder queued: ")); Serial.println(scheduleName(schedule));
}

bool Schedule::current(TimeSchedule &schedule) const {
  if (count_ == 0) return false;
  schedule = queue_[head_];
  return true;
}

bool Schedule::currentTime(uint32_t &epoch) const {
  if (!rtcReady_) return false;
  epoch = currentEpoch_;
  return true;
}

bool Schedule::isRegistered(TimeSchedule schedule) const {
  const uint8_t index = (uint8_t)schedule;
  return index < kScheduleCount && enabled_[index];
}

void Schedule::acknowledgeCurrent() {
  if (count_ == 0) return;
  acknowledged_[(uint8_t)queue_[head_]] = true;
}

void Schedule::expireUnacknowledged(uint32_t now) {
  uint8_t position = 0;
  while (position < count_) {
    const uint8_t slot = (head_ + position) % kScheduleCount;
    const uint8_t index = (uint8_t)queue_[slot];
    if (!acknowledged_[index] && now >= pendingSince_[index] &&
        (uint32_t)(now - pendingSince_[index]) >= intervalSeconds((TimeSchedule)index)) {
      Serial.print(F("MISSED reminder expired without personnel acknowledgment: "));
      Serial.println(scheduleName((TimeSchedule)index));
      queued_[index] = false;
      for (uint8_t shift = position; shift + 1 < count_; ++shift) {
        const uint8_t destination = (head_ + shift) % kScheduleCount;
        const uint8_t source = (head_ + shift + 1) % kScheduleCount;
        queue_[destination] = queue_[source];
      }
      tail_ = (tail_ + kScheduleCount - 1) % kScheduleCount;
      --count_;
      // Do not advance position: the next queued entry shifted into this slot.
    } else {
      ++position;
    }
  }
}

void Schedule::completeCurrent() {
  if (count_ == 0) return;
  const TimeSchedule done = queue_[head_];
  head_ = (head_ + 1) % kScheduleCount;
  --count_;
  queued_[(uint8_t)done] = false;
  acknowledged_[(uint8_t)done] = false;
  Serial.print(F("Dispensing workflow complete: ")); Serial.println(scheduleName(done));
}

void Schedule::printScheduleStatus(uint32_t now) {
  for (uint8_t i = 0; i < kScheduleCount; ++i) {
    const TimeSchedule schedule = (TimeSchedule)i;
    Serial.print(scheduleName(schedule)); Serial.print(F(" interval="));
    Serial.print(intervalSeconds(schedule)); Serial.print(F("s "));
    if (!enabled_[i]) {
      Serial.println(F("not registered"));
      continue;
    }
    Serial.print(F("next=")); printEpoch(nextDue_[i]);
    if (queued_[i]) Serial.print(F(" pending/active"));
    Serial.print(F(" (in "));
    Serial.print(nextDue_[i] > now ? nextDue_[i] - now : 0);
    Serial.println(F("s)"));
  }
}

void Schedule::loop() {
  const uint32_t pollStarted = millis();
  if (hasPolledRtc_ && (uint32_t)(pollStarted - lastRtcPoll_) < 250UL) return;
  hasPolledRtc_ = true;
  lastRtcPoll_ = pollStarted;

  uint32_t now;
  if (!readRtc(now)) {
    if (rtcReady_) Serial.println(F("ERROR: DS3231 read failed or time is invalid; scheduling paused."));
    rtcReady_ = false;
    return;
  }
  currentEpoch_ = now;

  if (!rtcReady_) {
    rtcReady_ = true;
    if (!initialized_) {
      for (uint8_t i = 0; i < kScheduleCount; ++i)
        if (enabled_[i]) nextDue_[i] = now + intervalSeconds((TimeSchedule)i);
      initialized_ = true;
      Serial.print(F("DS3231 time: ")); printClock(now); Serial.println();
      printScheduleStatus(now);
    } else {
      Serial.println(F("DS3231 recovered; existing reminder deadlines retained."));
    }
  }

  if ((uint32_t)(millis() - lastClockLog_) >= 1000UL) {
    lastClockLog_ = millis();
    Serial.print(F("DS3231 time: ")); printClock(now); Serial.println();
  }
  if ((uint32_t)(millis() - lastScheduleLog_) >= 60000UL) {
    lastScheduleLog_ = millis();
    printScheduleStatus(now);
  }

  expireUnacknowledged(now);
  for (uint8_t i = 0; i < kScheduleCount; ++i) {
    if (!enabled_[i]) continue;
    if (now < nextDue_[i]) continue;
    const TimeSchedule schedule = (TimeSchedule)i;
    const uint32_t interval = intervalSeconds(schedule);
    const uint32_t missed = (now - nextDue_[i]) / interval;
    if (missed > 0) {
      Serial.print(F("MISSED schedule(s): ")); Serial.print(scheduleName(schedule));
      Serial.print(F(" count=")); Serial.println(missed);
    }
    // Coalesce missed periods into one request. Never build a backlog of doses.
    const uint32_t due = nextDue_[i];
    nextDue_[i] += (missed + 1) * interval;
    if (missed > 0) {
      Serial.print(F("MISSED reminder skipped safely (no automatic dispense): "));
      Serial.println(scheduleName(schedule));
      continue;
    }
    if (queued_[i]) {
      Serial.print(F("Due time coalesced; reminder already pending/active: "));
      Serial.println(scheduleName(schedule));
    } else {
      Serial.print(F("Reminder due at ")); printEpoch(due); Serial.print(F(": "));
      Serial.println(scheduleName(schedule));
      enqueue(schedule, due);
    }
  }
}
