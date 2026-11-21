#ifndef DISPENSE_MODULE_H
#define DISPENSE_MODULE_H

#include <Arduino.h>
#include <Servo.h>
#include "SchedulingModule.h"

class Controller {
 public:
  void begin();
  void loop(Schedule &schedule);

 private:
  enum class State : uint8_t { Ready, AwaitAcknowledgment, Dispensing, SpoonHold, RelayActive };
  State state_ = State::Ready;
  Servo medicationServo_;
  Servo spoonServo_;
  uint16_t activeScheduleId_ = 0;
  uint8_t activeServoPin_ = 0;
  uint8_t activeSensorPin_ = 0;
  uint32_t stateStarted_ = 0;
  uint32_t lastBuzzerToggle_ = 0;
  bool buzzerOn_ = false;

  uint8_t servoPin(TimeSchedule interval) const;
  uint8_t sensorPin(TimeSchedule interval) const;
  void enterAwaitAcknowledgment(const ScheduleRecord &record);
  void startDispensing(const ScheduleRecord &record);
  void finishCurrent(Schedule &schedule, bool success);
};

#endif
