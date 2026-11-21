#include "DispenseModule.h"

namespace {
constexpr uint32_t kDispenseTimeoutMs = 30000UL;
constexpr uint32_t kSpoonHoldMs = 4000UL;
constexpr uint32_t kDisinfectionMs = 10000UL;
constexpr uint32_t kBuzzerPeriodMs = 500UL;
}

uint8_t Controller::servoPin(TimeSchedule interval) const {
  switch (interval) {
    case TimeSchedule::EveryFourHr: return ServoFourHr;
    case TimeSchedule::EverySixHr: return ServoSixHr;
    case TimeSchedule::EveryEightHr: return ServoEightHr;
    case TimeSchedule::EveryTwelveHr: return ServoTwelveHr;
  }
  return 0;
}

uint8_t Controller::sensorPin(TimeSchedule interval) const {
  switch (interval) {
    case TimeSchedule::EveryFourHr: return IRFourHr;
    case TimeSchedule::EverySixHr: return IRSixHr;
    case TimeSchedule::EveryEightHr: return IREightHr;
    case TimeSchedule::EveryTwelveHr: return IRTwelveHr;
  }
  return 0;
}

void Controller::begin() {
  pinMode(IRFourHr, INPUT_PULLUP);
  pinMode(IRSixHr, INPUT_PULLUP);
  pinMode(IREightHr, INPUT_PULLUP);
  pinMode(IRTwelveHr, INPUT_PULLUP);
  medicationServo_.detach();
  spoonServo_.attach(ServoSpoon);
  spoonServo_.write(SPOON_HOME);
  pinMode(Relay, OUTPUT);
  digitalWrite(Relay, LOW);
  pinMode(Buzzer, OUTPUT);
  digitalWrite(Buzzer, LOW);
  pinMode(LEDgreen, OUTPUT);
  pinMode(LEDred, OUTPUT);
  digitalWrite(LEDgreen, HIGH);
  digitalWrite(LEDred, LOW);
  Serial.println(F("Dispensing controller ready; medication servo attaches only after SAVE acknowledgment."));
}

void Controller::enterAwaitAcknowledgment(const ScheduleRecord &record) {
  activeScheduleId_ = record.id;
  state_ = State::AwaitAcknowledgment;
  lastBuzzerToggle_ = millis();
  buzzerOn_ = false;
  digitalWrite(Buzzer, LOW);
  digitalWrite(LEDgreen, LOW);
  digitalWrite(LEDred, HIGH);
  Serial.print(F("Reminder active; waiting for SAVE acknowledgment, schedule #"));
  Serial.println(record.id);
}

void Controller::startDispensing(const ScheduleRecord &record) {
  const TimeSchedule interval = (TimeSchedule)record.intervalHours;
  activeServoPin_ = servoPin(interval);
  activeSensorPin_ = sensorPin(interval);
  activeScheduleId_ = record.id;
  Serial.print(F("Attaching servo pin ")); Serial.println(activeServoPin_);
  medicationServo_.attach(activeServoPin_);
  medicationServo_.write(SERVO_FORWARD);
  Serial.print(F("Servo started for schedule #")); Serial.println(record.id);
  stateStarted_ = millis();
  state_ = State::Dispensing;
  buzzerOn_ = false;
  digitalWrite(Buzzer, LOW);
  digitalWrite(LEDred, LOW);
}

void Controller::finishCurrent(Schedule &schedule, bool success) {
  medicationServo_.detach();
  digitalWrite(Relay, LOW);
  digitalWrite(Buzzer, LOW);
  digitalWrite(LEDred, LOW);
  digitalWrite(LEDgreen, HIGH);
  if (success) Serial.println(F("Dispensing and disinfection workflow complete."));
  schedule.completeCurrent(activeScheduleId_);
  activeScheduleId_ = 0;
  activeServoPin_ = 0;
  activeSensorPin_ = 0;
  state_ = State::Ready;
}

void Controller::loop(Schedule &schedule) {
  ScheduleRecord currentRecord;
  const bool hasCurrent = schedule.current(currentRecord);

  if (state_ == State::Ready && hasCurrent)
    enterAwaitAcknowledgment(currentRecord);

  if (state_ == State::AwaitAcknowledgment) {
    if (!hasCurrent) {
      digitalWrite(Buzzer, LOW);
      digitalWrite(LEDred, LOW);
      digitalWrite(LEDgreen, HIGH);
      activeScheduleId_ = 0;
      state_ = State::Ready;
      return;
    }
    if (currentRecord.id != activeScheduleId_)
      enterAwaitAcknowledgment(currentRecord);
    if ((uint32_t)(millis() - lastBuzzerToggle_) >= kBuzzerPeriodMs) {
      lastBuzzerToggle_ = millis();
      buzzerOn_ = !buzzerOn_;
      digitalWrite(Buzzer, buzzerOn_ ? HIGH : LOW);
    }
    if (schedule.isAcknowledged(currentRecord.id)) startDispensing(currentRecord);
  }

  if (state_ == State::Dispensing) {
    if (digitalRead(activeSensorPin_) == IR_ACTIVE_LEVEL) {
      Serial.print(F("Pill detected by IR pin ")); Serial.print(activeSensorPin_);
      Serial.println(F("; medication servo detached."));
      medicationServo_.detach();
      spoonServo_.write(SPOON_DISPENSE);
      Serial.println(F("Spoon servo to 180 degrees; holding for 4 seconds."));
      stateStarted_ = millis();
      state_ = State::SpoonHold;
    } else if ((uint32_t)(millis() - stateStarted_) >= kDispenseTimeoutMs) {
      Serial.print(F("ERROR: dispense timeout for schedule #"));
      Serial.println(activeScheduleId_);
      medicationServo_.detach();
      finishCurrent(schedule, false);
    }
  }

  if (state_ == State::SpoonHold &&
      (uint32_t)(millis() - stateStarted_) >= kSpoonHoldMs) {
    spoonServo_.write(SPOON_HOME);
    Serial.println(F("Spoon servo returned to 0 degrees; disinfection relay ON."));
    digitalWrite(Relay, HIGH);
    stateStarted_ = millis();
    state_ = State::RelayActive;
  }

  if (state_ == State::RelayActive &&
      (uint32_t)(millis() - stateStarted_) >= kDisinfectionMs) {
    Serial.println(F("Disinfection relay OFF after 10 seconds."));
    digitalWrite(Relay, LOW);
    finishCurrent(schedule, true);
  }
}
