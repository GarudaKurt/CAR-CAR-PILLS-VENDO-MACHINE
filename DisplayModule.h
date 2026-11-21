#ifndef DISPLAY_MODULE_H
#define DISPLAY_MODULE_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "SchedulingModule.h"

class DisplayController {
 public:
  void begin();
  void loop(Schedule &schedule);

 private:
  enum class Screen : uint8_t {
    Normal, MainMenu, SelectInterval, SelectDurationType,
    SelectDurationValue, SaveConfirm, ScheduleList, ScheduleDetails,
    DeleteConfirm, Notice
  };

  LiquidCrystal_I2C lcd_{LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS};
  char displayed_[LCD_ROWS][LCD_COLUMNS + 1] = {{0}};
  char noticeLines_[3][LCD_COLUMNS + 1] = {{0}};
  bool lastRaw_[4] = {true, true, true, true};
  bool stablePressed_[4] = {false, false, false, false};
  uint32_t changedAt_[4] = {0, 0, 0, 0};
  bool pressEvent_[4] = {false, false, false, false};
  uint32_t modePressedAt_ = 0;
  uint32_t noticeUntil_ = 0;
  uint32_t lastRefresh_ = 0;
  bool modeTiming_ = false;
  bool modeHandled_ = false;
  Screen screen_ = Screen::Normal;
  Screen noticeReturn_ = Screen::Normal;
  uint8_t selection_ = 0;
  uint8_t intervalSelection_ = 0;
  uint8_t durationTypeSelection_ = 0;
  uint8_t durationValueSelection_ = 0;
  uint8_t viewIndex_ = 0;
  uint8_t detailPage_ = 0;

  void updateButtons();
  void handleModeButton(Schedule &schedule);
  void handleMenuButtons(Schedule &schedule);
  void render(Schedule &schedule);
  void renderNormal(Schedule &schedule);
  void renderOptions(const char *title, uint8_t count);
  void renderScheduleList(Schedule &schedule);
  void renderScheduleDetails(Schedule &schedule);
  void showNotice(const char *line1, const char *line2, const char *line3,
                  Screen returnTo, uint32_t durationMs = 2500UL);
  void setLine(uint8_t row, const char *text);
  void moveSelection(int8_t direction, uint8_t count);
  void enterMainMenu();
  bool currentRecord(Schedule &schedule, ScheduleRecord &record) const;
};

#endif
