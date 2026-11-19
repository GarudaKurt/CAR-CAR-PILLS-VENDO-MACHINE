#include "DisplayModule.h"
#include <stdio.h>
#include <string.h>

namespace {
bool isLeapYear(uint16_t year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

uint8_t daysInMonth(uint16_t year, uint8_t month) {
  static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && isLeapYear(year) ? 29 : days[month - 1];
}

void formatDateTime(uint32_t epoch, char *output, size_t capacity) {
  uint32_t days = epoch / 86400UL;
  const uint32_t daySeconds = epoch % 86400UL;
  uint16_t year = 2000;
  while (days >= (isLeapYear(year) ? 366UL : 365UL)) {
    days -= isLeapYear(year) ? 366UL : 365UL;
    ++year;
  }
  uint8_t month = 1;
  while (days >= daysInMonth(year, month)) {
    days -= daysInMonth(year, month);
    ++month;
  }
  const uint8_t day = days + 1;
  const uint8_t hour = daySeconds / 3600UL;
  const uint8_t minute = (daySeconds % 3600UL) / 60UL;
  const uint8_t second = daySeconds % 60UL;
  snprintf(output, capacity, "%04u-%02u-%02u %02u:%02u:%02u",
           year, month, day, hour, minute, second);
}

const char *shortScheduleName(TimeSchedule schedule) {
  switch (schedule) {
    case TimeSchedule::EveryOneHr: return "1h";
    case TimeSchedule::EveryTwoHr: return "2h";
    case TimeSchedule::EveryFourHr: return "4h";
    case TimeSchedule::EveryEightHr: return "8h";
  }
  return "?";
}
}

void DisplayController::begin() {
  lcd_.init();
  lcd_.backlight();
  lcd_.clear();
  for (uint8_t row = 0; row < LCD_ROWS; ++row) {
    displayed_[row][0] = '\0';
    setLine(row, "Medication reminder");
  }
}

void DisplayController::setLine(uint8_t row, const char *text) {
  char next[LCD_COLUMNS + 1];
  memset(next, ' ', LCD_COLUMNS);
  next[LCD_COLUMNS] = '\0';
  size_t length = strlen(text);
  if (length > LCD_COLUMNS) length = LCD_COLUMNS;
  memcpy(next, text, length);
  if (strncmp(displayed_[row], next, LCD_COLUMNS) == 0) return;
  memcpy(displayed_[row], next, LCD_COLUMNS + 1);
  lcd_.setCursor(0, row);
  lcd_.print(next);
}

void DisplayController::loop(const Schedule &schedule) {
  if ((uint32_t)(millis() - lastRefresh_) < 250UL) return;
  lastRefresh_ = millis();

  char line[LCD_COLUMNS + 1];
  uint32_t now;
  if (schedule.currentTime(now)) {
    formatDateTime(now, line, sizeof(line));
    setLine(0, line);
  } else {
    setLine(0, "RTC unavailable");
  }

  if (schedule.isTestMode()) setLine(1, "MODE: TEST");
  else setLine(1, "MODE: PRODUCTION");

  TimeSchedule pending;
  if (schedule.current(pending)) {
    snprintf(line, sizeof(line), "DUE: %s", Schedule::scheduleName(pending));
    setLine(2, line);
    snprintf(line, sizeof(line), "Press %s btn to ack", shortScheduleName(pending));
    setLine(3, line);
  } else {
    snprintf(line, sizeof(line), "1h:%c 2h:%c 4h:%c 8h:%c",
             schedule.isRegistered(TimeSchedule::EveryOneHr) ? 'Y' : 'N',
             schedule.isRegistered(TimeSchedule::EveryTwoHr) ? 'Y' : 'N',
             schedule.isRegistered(TimeSchedule::EveryFourHr) ? 'Y' : 'N',
             schedule.isRegistered(TimeSchedule::EveryEightHr) ? 'Y' : 'N');
    setLine(2, line);
    setLine(3, "5-9s add / 10s del");
  }
}
