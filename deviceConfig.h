#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <Arduino.h>

// Set to 0 in this file (or pass -DTEST_MODE=0) for production intervals.
#ifndef TEST_MODE
#define TEST_MODE 1
#endif
// Schedules
enum class TimeSchedule : uint8_t {
  EveryOneHr,
  EveryTwoHr,
  EveryFourHr,
  EveryEightHr
};

// Pin Connections
#define ServoOneHr 7
#define ServoTwoHr 6
#define ServoFourHr 5
#define ServoEightHr 4
#define ServoSpoon 3

// Active-low IR sensors and push buttons use the Mega's internal pull-ups.
#define IROneHr 13
#define IRTwoHr 12
#define IRFourHr 11
#define IREightHr 10

#define BtnOneHr 41
#define BtnTwoHr 43
#define BtnFourHr 45
#define BtnEightHr 47
#define BtnMode 39

#define LEDgreen 49
#define LEDred 48

#define Relay 40
#define Buzzer 46

#define LCD_I2C_ADDRESS 0x27
#define LCD_COLUMNS 20
#define LCD_ROWS 4

constexpr uint8_t SERVO_FORWARD = 180;
constexpr uint8_t SPOON_HOME = 0;
constexpr uint8_t SPOON_DISPENSE = 180;
constexpr uint8_t IR_ACTIVE_LEVEL = LOW;

#endif
