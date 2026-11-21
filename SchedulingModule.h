#ifndef SCHEDULING_MODULE_H
#define SCHEDULING_MODULE_H

#include <Arduino.h>
#include <EEPROM.h>
#include "deviceConfig.h"

struct ScheduleRecord {
  uint16_t id;
  uint8_t intervalHours;
  uint8_t durationType;
  uint8_t durationValue;
  uint8_t status;
  uint8_t reminderPending;
  uint8_t reserved;
  uint32_t startEpoch;
  uint32_t expirationEpoch;
  uint32_t nextReminderEpoch;
  uint32_t pendingEpoch;
};

class Schedule {
 public:
  static constexpr uint8_t kMaxSchedules = 12;

  void begin();
  void loop();

  bool current(ScheduleRecord &record) const;
  bool currentTime(uint32_t &epoch) const;
  bool isAcknowledged(uint16_t id) const;
  bool acknowledgeCurrent();
  void completeCurrent(uint16_t id);

  bool createSchedule(TimeSchedule interval, DurationType durationType,
                      uint8_t durationValue, uint16_t &createdId);
  bool deleteScheduleAt(uint8_t index);
  uint8_t scheduleCount() const;
  bool scheduleAt(uint8_t index, ScheduleRecord &record) const;

  bool setTestMode(bool testMode);
  bool isTestMode() const { return database_.mode != 0; }
  uint32_t intervalSeconds(TimeSchedule interval) const;

  static const char *scheduleName(TimeSchedule interval);
  static const char *durationName(DurationType type);

 private:
  static constexpr uint32_t kDatabaseMagic = 0x50494C4CUL;
  static constexpr uint8_t kDatabaseVersion = 1;
  static constexpr int kEepromAddress = 64;

  struct Database {
    uint32_t magic;
    uint8_t version;
    uint8_t mode;
    uint8_t count;
    uint8_t reserved;
    uint16_t nextId;
    ScheduleRecord records[kMaxSchedules];
    uint16_t checksum;
  };

  Database database_{};
  bool rtcReady_ = false;
  bool hasPolledRtc_ = false;
  uint32_t lastRtcPoll_ = 0;
  uint32_t currentEpoch_ = 0;
  uint16_t acknowledgedId_ = 0;

  bool readRtc(uint32_t &epoch) const;
  void loadDatabase();
  void saveDatabase();
  uint16_t checksum(const Database &db) const;
  void removeAt(uint8_t index);
  bool validateDuration(DurationType type, uint8_t value) const;
  bool validInterval(TimeSchedule interval) const;
  uint32_t expirationFrom(uint32_t startEpoch, DurationType type,
                          uint8_t value) const;
  void processSchedules(uint32_t now);
};

#endif
