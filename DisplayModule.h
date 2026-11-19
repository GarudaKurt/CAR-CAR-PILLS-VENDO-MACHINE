#ifndef DISPLAY_MODULE_H
#define DISPLAY_MODULE_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "SchedulingModule.h"

class DisplayController {
 public:
  void begin();
  void loop(const Schedule &schedule);

 private:
  LiquidCrystal_I2C lcd_{LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS};
  char displayed_[LCD_ROWS][LCD_COLUMNS + 1] = {{0}};
  uint32_t lastRefresh_ = 0;

  void setLine(uint8_t row, const char *text);
};

#endif
