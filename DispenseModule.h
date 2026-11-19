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
  enum class State : uint8_t { Ready, AwaitButton, Dispensing, SpoonHold, RelayActive };
  State state_ = State::Ready;
  Servo medicationServos_[4];
  Servo spoonServo_;
  uint32_t stateStarted_ = 0;
  uint32_t lastBuzzerToggle_ = 0;
  bool buzzerOn_ = false;
  bool buttonArmed_[4] = {true, true, true, true};
  uint32_t buttonLowSince_[4] = {0, 0, 0, 0};
  uint32_t registrationSince_[4] = {0, 0, 0, 0};
  bool registrationTiming_[4] = {false, false, false, false};
  bool registrationHandled_[4] = {false, false, false, false};
  uint32_t modeButtonSince_ = 0;
  bool modeButtonTiming_ = false;
  bool modeButtonHandled_ = false;
  int8_t activeIndex_ = -1;

  static uint8_t servoPin(uint8_t index);
  static uint8_t sensorPin(uint8_t index);
  static uint8_t buttonPin(uint8_t index);
  void enterAwaitButton(TimeSchedule schedule);
  void startDispensing(TimeSchedule schedule, uint8_t index);
  void finishCurrent(Schedule &schedule);
};

#endif
