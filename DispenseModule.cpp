#include "DispenseModule.h"

namespace {
constexpr uint32_t kButtonDebounceMs = 35UL;
constexpr uint32_t kRegistrationHoldMs = 5000UL;
constexpr uint32_t kUnregisterHoldMs = 10000UL;
constexpr uint32_t kTestModeHoldMs = 3000UL;
constexpr uint32_t kProductionModeHoldMs = 5000UL;
constexpr uint32_t kDispenseTimeoutMs = 30000UL;
constexpr uint32_t kSpoonHoldMs = 4000UL;
constexpr uint32_t kDisinfectionMs = 10000UL;
constexpr uint32_t kBuzzerPeriodMs = 500UL;
}

uint8_t Controller::servoPin(uint8_t index) {
  static const uint8_t pins[] = {ServoOneHr, ServoTwoHr, ServoFourHr, ServoEightHr};
  return pins[index];
}

uint8_t Controller::sensorPin(uint8_t index) {
  static const uint8_t pins[] = {IROneHr, IRTwoHr, IRFourHr, IREightHr};
  return pins[index];
}

uint8_t Controller::buttonPin(uint8_t index) {
  static const uint8_t pins[] = {BtnOneHr, BtnTwoHr, BtnFourHr, BtnEightHr};
  return pins[index];
}

void Controller::begin() {
  for (uint8_t i = 0; i < 4; ++i) {
    pinMode(sensorPin(i), INPUT_PULLUP);
    pinMode(buttonPin(i), INPUT_PULLUP);
    // Continuous-rotation medication servos stay detached until authorized.
    medicationServos_[i].detach();
  }
  spoonServo_.attach(ServoSpoon);
  spoonServo_.write(SPOON_HOME);
  pinMode(Relay, OUTPUT);
  digitalWrite(Relay, LOW);
  pinMode(Buzzer, OUTPUT);
  digitalWrite(Buzzer, LOW);
  pinMode(BtnMode, INPUT_PULLUP);
  pinMode(LEDgreen, OUTPUT);
  pinMode(LEDred, OUTPUT);
  digitalWrite(LEDgreen, HIGH);
  digitalWrite(LEDred, LOW);
  Serial.println(F("Dispensing controller ready. IR sensors configured active-low."));
}

void Controller::enterAwaitButton(TimeSchedule schedule) {
  state_ = State::AwaitButton;
  stateStarted_ = millis();
  buttonArmed_[(uint8_t)schedule] = digitalRead(buttonPin((uint8_t)schedule)) == HIGH;
  lastBuzzerToggle_ = 0;
  buzzerOn_ = false;
  digitalWrite(LEDgreen, LOW);
  digitalWrite(LEDred, HIGH);
  Serial.print(F("Reminder active; awaiting personnel button for "));
  Serial.println(Schedule::scheduleName(schedule));
}

void Controller::startDispensing(TimeSchedule schedule, uint8_t index) {
  activeIndex_ = index;
  buttonArmed_[index] = false;
  digitalWrite(Buzzer, LOW);
  buzzerOn_ = false;
  digitalWrite(LEDred, LOW);
  Serial.print(F("Button acknowledged: ")); Serial.println(Schedule::scheduleName(schedule));
  Serial.print(F("Attaching medication servo on pin ")); Serial.println(servoPin(index));
  medicationServos_[index].attach(servoPin(index));
  medicationServos_[index].write(SERVO_FORWARD);
  Serial.print(F("Medication servo running: ")); Serial.println(Schedule::scheduleName(schedule));
  stateStarted_ = millis();
  state_ = State::Dispensing;
}

void Controller::finishCurrent(Schedule &schedule) {
  digitalWrite(Relay, LOW);
  digitalWrite(Buzzer, LOW);
  digitalWrite(LEDred, LOW);
  digitalWrite(LEDgreen, HIGH);
  state_ = State::Ready;
  activeIndex_ = -1;
  schedule.completeCurrent();
}

void Controller::loop(Schedule &schedule) {
  TimeSchedule currentSchedule;
  const bool hasCurrent = schedule.current(currentSchedule);

  if (digitalRead(BtnMode) == HIGH) {
    if (modeButtonTiming_ && !modeButtonHandled_ &&
        state_ == State::Ready && !hasCurrent) {
      const uint32_t heldMs = (uint32_t)(millis() - modeButtonSince_);
      if (heldMs >= kTestModeHoldMs && heldMs < kProductionModeHoldMs)
        schedule.setTestMode(true);
      else if (heldMs >= kProductionModeHoldMs)
        schedule.setTestMode(false);
    }
    modeButtonTiming_ = false;
    modeButtonHandled_ = false;
  } else if (state_ == State::Ready && !hasCurrent) {
    if (!modeButtonTiming_) {
      modeButtonTiming_ = true;
      modeButtonSince_ = millis();
    }
    if ((uint32_t)(millis() - modeButtonSince_) >= kProductionModeHoldMs &&
        !modeButtonHandled_) {
      schedule.setTestMode(false);
      modeButtonHandled_ = true;
    }
  } else {
    modeButtonTiming_ = false;
  }

  // Re-arm a button only after release; this prevents repeated dispensing on a hold.
  for (uint8_t i = 0; i < 4; ++i) {
    if (digitalRead(buttonPin(i)) == HIGH) {
      if (registrationTiming_[i] && !registrationHandled_[i] &&
          state_ == State::Ready && !hasCurrent) {
        const uint32_t heldMs = (uint32_t)(millis() - registrationSince_[i]);
        if (heldMs >= kUnregisterHoldMs) {
          schedule.setRegistration((TimeSchedule)i, false);
          registrationHandled_[i] = true;
        } else if (heldMs >= kRegistrationHoldMs) {
          schedule.setRegistration((TimeSchedule)i, true);
          registrationHandled_[i] = true;
        }
      }
      buttonArmed_[i] = true;
      buttonLowSince_[i] = 0;
      registrationTiming_[i] = false;
      registrationHandled_[i] = false;
    }
  }

  if (state_ == State::Ready && !hasCurrent) {
    for (uint8_t i = 0; i < 4; ++i) {
      if (digitalRead(buttonPin(i)) != LOW || registrationHandled_[i]) continue;
      if (!registrationTiming_[i]) {
        registrationTiming_[i] = true;
        registrationSince_[i] = millis();
      }
      if ((uint32_t)(millis() - registrationSince_[i]) >= kUnregisterHoldMs) {
        schedule.setRegistration((TimeSchedule)i, false);
        buttonArmed_[i] = false;
        registrationHandled_[i] = true;
        registrationTiming_[i] = false;
      }
    }
  } else {
    for (uint8_t i = 0; i < 4; ++i) registrationTiming_[i] = false;
  }

  if (state_ == State::Ready && hasCurrent) enterAwaitButton(currentSchedule);

  if (state_ == State::AwaitButton && hasCurrent) {
    if ((uint32_t)(millis() - lastBuzzerToggle_) >= kBuzzerPeriodMs) {
      lastBuzzerToggle_ = millis();
      buzzerOn_ = !buzzerOn_;
      digitalWrite(Buzzer, buzzerOn_ ? HIGH : LOW);
    }
    const uint8_t index = (uint8_t)currentSchedule;
    if (buttonArmed_[index] && digitalRead(buttonPin(index)) == LOW) {
      if (buttonLowSince_[index] == 0) buttonLowSince_[index] = millis();
      if ((uint32_t)(millis() - buttonLowSince_[index]) >= kButtonDebounceMs) {
        schedule.acknowledgeCurrent();
        startDispensing(currentSchedule, index);
      }
    }
  }

  if (state_ == State::Dispensing && activeIndex_ >= 0) {
    if (digitalRead(sensorPin(activeIndex_)) == IR_ACTIVE_LEVEL) {
      Serial.print(F("IR detected pill; detaching medication servo on pin "));
      Serial.println(servoPin(activeIndex_));
      medicationServos_[activeIndex_].detach();
      Serial.print(F("Spoon servo moving to ")); Serial.println(SPOON_DISPENSE);
      spoonServo_.write(SPOON_DISPENSE);
      stateStarted_ = millis();
      state_ = State::SpoonHold;
    } else if ((uint32_t)(millis() - stateStarted_) >= kDispenseTimeoutMs) {
      medicationServos_[activeIndex_].detach();
      Serial.print(F("ERROR: IR sensor timeout for "));
      Serial.print(Schedule::scheduleName((TimeSchedule)activeIndex_));
      Serial.println(F("; servo detached, no repeat dispense queued."));
      finishCurrent(schedule);
    }
  }

  if (state_ == State::SpoonHold && (uint32_t)(millis() - stateStarted_) >= kSpoonHoldMs) {
    Serial.print(F("Spoon servo returning to ")); Serial.println(SPOON_HOME);
    spoonServo_.write(SPOON_HOME);
    Serial.println(F("Disinfection relay ON for 10 seconds."));
    digitalWrite(Relay, HIGH);
    stateStarted_ = millis();
    state_ = State::RelayActive;
  }

  if (state_ == State::RelayActive && (uint32_t)(millis() - stateStarted_) >= kDisinfectionMs) {
    Serial.println(F("Disinfection relay OFF."));
    finishCurrent(schedule);
  }
}
