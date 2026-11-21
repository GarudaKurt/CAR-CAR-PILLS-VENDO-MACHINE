#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <Arduino.h>

// Initial mode used only when no valid mode is stored in EEPROM.
#ifndef TEST_MODE
#define TEST_MODE 1
#endif

enum class TimeSchedule : uint8_t {
  EveryFourHr = 4,
  EverySixHr = 6,
  EveryEightHr = 8,
  EveryTwelveHr = 12
};

enum class DurationType : uint8_t { Days, Weeks, Months };

// Preserve the existing 4-hour and 8-hour servo/sensor wiring.
#define ServoFourHr 5
#define ServoSixHr 6
#define ServoEightHr 4
#define ServoTwelveHr 7
#define ServoSpoon 3

#define IRFourHr 11
#define IRSixHr 12
#define IREightHr 10
#define IRTwelveHr 13

// Menu controls and the separate mode/menu-entry button.
#define BtnNext 41
#define BtnPrevious 43
#define BtnSave 45
#define BtnBack 47
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
