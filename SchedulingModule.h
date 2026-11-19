#ifndef SCHEDULING_MODULE_H
#define SCHEDULING_MODULE_H

#include <Arduino.h>
#include <EEPROM.h>
#include "deviceConfig.h"

class Schedule {
 public:
  void begin();
  void loop();

  bool current(TimeSchedule &schedule) const;
  bool currentTime(uint32_t &epoch) const;
  bool isRegistered(TimeSchedule schedule) const;
  void acknowledgeCurrent();
  void completeCurrent();
  bool setRegistration(TimeSchedule schedule, bool registered);
  bool setTestMode(bool testMode);
  bool isTestMode() const { return testMode_; }

  uint32_t intervalSeconds(TimeSchedule schedule) const;
  static const char *scheduleName(TimeSchedule schedule);

 private:
  static constexpr uint8_t kScheduleCount = 4;
  TimeSchedule queue_[kScheduleCount];
  bool queued_[kScheduleCount] = {false, false, false, false};
  bool enabled_[kScheduleCount] = {false, false, false, false};
  bool acknowledged_[kScheduleCount] = {false, false, false, false};
  uint32_t pendingSince_[kScheduleCount] = {0, 0, 0, 0};
  uint8_t head_ = 0;
  uint8_t tail_ = 0;
  uint8_t count_ = 0;
  uint32_t nextDue_[kScheduleCount] = {0, 0, 0, 0};
  bool initialized_ = false;
  bool rtcReady_ = false;
  bool testMode_ = TEST_MODE != 0;
  uint32_t lastClockLog_ = 0;
  uint32_t lastScheduleLog_ = 0;
  uint32_t lastRtcPoll_ = 0;
  uint32_t currentEpoch_ = 0;
  bool hasPolledRtc_ = false;

  bool readRtc(uint32_t &epoch) const;
  void enqueue(TimeSchedule schedule, uint32_t dueAt);
  void expireUnacknowledged(uint32_t now);
  void loadRegistrations();
  void printScheduleStatus(uint32_t now);
};

#endif
