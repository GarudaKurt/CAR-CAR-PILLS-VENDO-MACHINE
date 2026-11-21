#include "DisplayModule.h"
#include <stdio.h>
#include <string.h>

namespace {
constexpr uint32_t kButtonDebounceMs = 35UL;
constexpr uint32_t kTestModeMinMs = 3000UL;
constexpr uint32_t kProductionModeMinMs = 5000UL;
constexpr uint32_t kMenuHoldMs = 15000UL;

const uint8_t kMenuButtonPins[] = {BtnNext, BtnPrevious, BtnSave, BtnBack};
const uint8_t kIntervalChoices[] = {4, 6, 8, 12};
const uint8_t kDurationValues[3][3] = {{1, 3, 5}, {1, 2, 3}, {1, 6, 12}};

bool leapYear(uint16_t year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

uint8_t daysInMonth(uint16_t year, uint8_t month) {
  static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && leapYear(year) ? 29 : days[month - 1];
}

void formatShortDate(uint32_t epoch, char *output, size_t size) {
  uint32_t days = epoch / 86400UL;
  const uint32_t daySeconds = epoch % 86400UL;
  uint16_t year = 2000;
  while (days >= (leapYear(year) ? 366UL : 365UL)) {
    days -= leapYear(year) ? 366UL : 365UL;
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
  snprintf(output, size, "%02u/%02u %02u:%02u", month, day, hour, minute);
}

uint8_t durationOptionCount(uint8_t category) {
  return category < 3 ? 3 : 0;
}

const char *durationValueName(DurationType type, uint8_t value) {
  switch (type) {
    case DurationType::Days: return value == 1 ? "Day" : "Days";
    case DurationType::Weeks: return value == 1 ? "Week" : "Weeks";
    case DurationType::Months: return value == 1 ? "Month" : "Months";
  }
  return "";
}
}

void DisplayController::begin() {
  lcd_.init();
  lcd_.backlight();
  lcd_.clear();
  for (uint8_t i = 0; i < 4; ++i) {
    pinMode(kMenuButtonPins[i], INPUT_PULLUP);
    lastRaw_[i] = digitalRead(kMenuButtonPins[i]) != LOW;
    stablePressed_[i] = false;
    changedAt_[i] = millis();
  }
  pinMode(BtnMode, INPUT_PULLUP);
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

void DisplayController::updateButtons() {
  for (uint8_t i = 0; i < 4; ++i) {
    pressEvent_[i] = false;
    const bool raw = digitalRead(kMenuButtonPins[i]) != LOW;
    if (raw != lastRaw_[i]) {
      lastRaw_[i] = raw;
      changedAt_[i] = millis();
    }
    if ((uint32_t)(millis() - changedAt_[i]) >= kButtonDebounceMs &&
        stablePressed_[i] != !raw) {
      stablePressed_[i] = !raw;
      pressEvent_[i] = stablePressed_[i];
    }
  }
}

bool DisplayController::currentRecord(Schedule &schedule, ScheduleRecord &record) const {
  return schedule.current(record);
}

void DisplayController::enterMainMenu() {
  screen_ = Screen::MainMenu;
  selection_ = 0;
  Serial.println(F("Schedule menu opened."));
}

void DisplayController::handleModeButton(Schedule &schedule) {
  const bool pressed = digitalRead(BtnMode) == LOW;
  ScheduleRecord pending;
  const bool hasPending = schedule.current(pending);

  if (pressed) {
    if (!modeTiming_) {
      modeTiming_ = true;
      modePressedAt_ = millis();
      modeHandled_ = false;
    }
    if (!modeHandled_ && screen_ == Screen::Normal && !hasPending &&
        (uint32_t)(millis() - modePressedAt_) >= kMenuHoldMs) {
      enterMainMenu();
      modeHandled_ = true;
    }
    return;
  }

  if (!modeTiming_) return;
  const uint32_t heldMs = (uint32_t)(millis() - modePressedAt_);
  if (!modeHandled_ && screen_ == Screen::Normal && !hasPending) {
    if (heldMs >= kTestModeMinMs && heldMs < kProductionModeMinMs)
      schedule.setTestMode(true);
    else if (heldMs >= kProductionModeMinMs && heldMs < kMenuHoldMs)
      schedule.setTestMode(false);
  }
  modeTiming_ = false;
  modeHandled_ = false;
}

void DisplayController::moveSelection(int8_t direction, uint8_t count) {
  if (count == 0) return;
  if (direction > 0) selection_ = (selection_ + 1) % count;
  else selection_ = (selection_ + count - 1) % count;
}

void DisplayController::showNotice(const char *line1, const char *line2,
                                   const char *line3, Screen returnTo,
                                   uint32_t durationMs) {
  strncpy(noticeLines_[0], line1, LCD_COLUMNS);
  strncpy(noticeLines_[1], line2, LCD_COLUMNS);
  strncpy(noticeLines_[2], line3, LCD_COLUMNS);
  for (uint8_t i = 0; i < 3; ++i) noticeLines_[i][LCD_COLUMNS] = '\0';
  noticeReturn_ = returnTo;
  noticeUntil_ = millis() + durationMs;
  screen_ = Screen::Notice;
}

void DisplayController::handleMenuButtons(Schedule &schedule) {
  const bool next = pressEvent_[0];
  const bool previous = pressEvent_[1];
  const bool save = pressEvent_[2];
  const bool back = pressEvent_[3];

  if (screen_ == Screen::Normal) {
    if (save) schedule.acknowledgeCurrent();
    return;
  }
  if (screen_ == Screen::Notice) return;

  if (screen_ == Screen::MainMenu) {
    if (next) moveSelection(1, 3);
    if (previous) moveSelection(-1, 3);
    if (save) {
      if (selection_ == 0) {
        screen_ = Screen::SelectInterval;
        selection_ = 0;
      } else if (selection_ == 1) {
        if (schedule.scheduleCount() == 0) {
          showNotice("NO ACTIVE SCHEDULES", "Use Add Schedule", "", Screen::MainMenu);
        } else {
          screen_ = Screen::ScheduleList;
          viewIndex_ = 0;
        }
      } else {
        screen_ = Screen::Normal;
      }
    }
    if (back) screen_ = Screen::Normal;
    return;
  }

  if (screen_ == Screen::SelectInterval) {
    if (next) moveSelection(1, 4);
    if (previous) moveSelection(-1, 4);
    if (save) {
      intervalSelection_ = selection_;
      selection_ = 0;
      screen_ = Screen::SelectDurationType;
    }
    if (back) screen_ = Screen::MainMenu;
    return;
  }

  if (screen_ == Screen::SelectDurationType) {
    if (next) moveSelection(1, 3);
    if (previous) moveSelection(-1, 3);
    if (save) {
      durationTypeSelection_ = selection_;
      durationValueSelection_ = 0;
      selection_ = 0;
      screen_ = Screen::SelectDurationValue;
    }
    if (back) {
      screen_ = Screen::SelectInterval;
      selection_ = intervalSelection_;
    }
    return;
  }

  if (screen_ == Screen::SelectDurationValue) {
    const uint8_t count = durationOptionCount(durationTypeSelection_);
    if (next) moveSelection(1, count);
    if (previous) moveSelection(-1, count);
    if (save) {
      durationValueSelection_ = selection_;
      screen_ = Screen::SaveConfirm;
    }
    if (back) {
      screen_ = Screen::SelectDurationType;
      selection_ = durationTypeSelection_;
    }
    return;
  }

  if (screen_ == Screen::SaveConfirm) {
    if (save) {
      const TimeSchedule interval = (TimeSchedule)kIntervalChoices[intervalSelection_];
      const DurationType type = (DurationType)durationTypeSelection_;
      const uint8_t value = kDurationValues[durationTypeSelection_][durationValueSelection_];
      uint16_t id = 0;
      if (schedule.createSchedule(interval, type, value, id)) {
        char line2[LCD_COLUMNS + 1];
        char line3[LCD_COLUMNS + 1];
        snprintf(line2, sizeof(line2), "ID:%u Every %uh", id, kIntervalChoices[intervalSelection_]);
        snprintf(line3, sizeof(line3), "%u %s", value,
                 durationValueName(type, value));
        showNotice("SCHEDULE SAVED", line2, line3, Screen::Normal, 3500UL);
      } else {
        showNotice("SAVE FAILED", "RTC invalid or", "schedule storage full", Screen::MainMenu);
      }
    }
    if (back) {
      screen_ = Screen::SelectDurationValue;
      selection_ = durationValueSelection_;
    }
    return;
  }

  if (screen_ == Screen::ScheduleList) {
    const uint8_t count = schedule.scheduleCount();
    if (next && count) viewIndex_ = (viewIndex_ + 1) % count;
    if (previous && count) viewIndex_ = (viewIndex_ + count - 1) % count;
    if (save && count) {
      detailPage_ = 0;
      screen_ = Screen::ScheduleDetails;
    }
    if (back) screen_ = Screen::MainMenu;
    return;
  }

  if (screen_ == Screen::ScheduleDetails) {
    if (next || previous) detailPage_ ^= 1;
    if (save) screen_ = Screen::DeleteConfirm;
    if (back) screen_ = Screen::ScheduleList;
    return;
  }

  if (screen_ == Screen::DeleteConfirm) {
    if (save) {
      const bool removed = schedule.deleteScheduleAt(viewIndex_);
      if (removed && schedule.scheduleCount() > 0) {
        if (viewIndex_ >= schedule.scheduleCount()) viewIndex_ = schedule.scheduleCount() - 1;
        showNotice("SCHEDULE DELETED", "", "", Screen::ScheduleList);
      } else {
        showNotice("SCHEDULE DELETED", "No schedules remain", "", Screen::MainMenu);
      }
    }
    if (back) screen_ = Screen::ScheduleDetails;
  }
}

void DisplayController::renderOptions(const char *title, uint8_t count) {
  setLine(0, title);
  char line[LCD_COLUMNS + 1];
  for (uint8_t row = 0; row < 3; ++row) {
    const uint8_t option = (selection_ + row) % count;
    const char *label = "";
    if (screen_ == Screen::MainMenu) {
      static const char *mainOptions[] = {"Add Schedule", "View Schedules", "Exit"};
      label = mainOptions[option];
    } else if (screen_ == Screen::SelectInterval) {
      snprintf(line, sizeof(line), "Every %u Hours", kIntervalChoices[option]);
      label = line;
    } else if (screen_ == Screen::SelectDurationType) {
      label = Schedule::durationName((DurationType)option);
    } else {
      const uint8_t value = kDurationValues[durationTypeSelection_][option];
      static const char *singular[] = {"Day", "Week", "Month"};
      static const char *plural[] = {"Days", "Weeks", "Months"};
      snprintf(line, sizeof(line), "%u %s", value,
               value == 1 ? singular[durationTypeSelection_] : plural[durationTypeSelection_]);
      label = line;
    }
    char rowText[LCD_COLUMNS + 1];
    snprintf(rowText, sizeof(rowText), "%c %s", row == 0 ? '>' : ' ', label);
    setLine(row + 1, rowText);
  }
}

void DisplayController::renderNormal(Schedule &schedule) {
  char line[LCD_COLUMNS + 1];
  uint32_t now;
  if (schedule.currentTime(now)) {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
    // Convert epoch to the display date with the same calendar rules as the RTC.
    uint32_t days = now / 86400UL;
    const uint32_t daySeconds = now % 86400UL;
    year = 2000;
    while (days >= (leapYear(year) ? 366UL : 365UL)) {
      days -= leapYear(year) ? 366UL : 365UL;
      ++year;
    }
    month = 1;
    while (days >= daysInMonth(year, month)) {
      days -= daysInMonth(year, month);
      ++month;
    }
    day = days + 1;
    hour = daySeconds / 3600UL;
    minute = (daySeconds % 3600UL) / 60UL;
    second = daySeconds % 60UL;
    snprintf(line, sizeof(line), "%04u-%02u-%02u %02u:%02u:%02u",
             year, month, day, hour, minute, second);
    setLine(0, line);
  } else {
    setLine(0, "RTC unavailable");
  }

  setLine(1, schedule.isTestMode() ? "MODE: TEST" : "MODE: PRODUCTION");
  ScheduleRecord pending;
  if (schedule.current(pending)) {
    snprintf(line, sizeof(line), "DUE #%u Every %uh", pending.id, pending.intervalHours);
    setLine(2, line);
    setLine(3, "SAVE: ACKNOWLEDGE");
  } else {
    snprintf(line, sizeof(line), "Active schedules: %u", schedule.scheduleCount());
    setLine(2, line);
    setLine(3, "MODE 15s opens menu");
  }
}

void DisplayController::renderScheduleList(Schedule &schedule) {
  setLine(0, "VIEW SCHEDULES");
  ScheduleRecord record;
  if (!schedule.scheduleAt(viewIndex_, record)) {
    setLine(1, "No active schedules");
    setLine(2, "BACK: Return");
    setLine(3, "");
    return;
  }
  char line[LCD_COLUMNS + 1];
  snprintf(line, sizeof(line), "Schedule %u of %u", viewIndex_ + 1, schedule.scheduleCount());
  setLine(1, line);
  snprintf(line, sizeof(line), "ID:%u Every %uh", record.id, record.intervalHours);
  setLine(2, line);
  setLine(3, "SAVE:Details BACK");
}

void DisplayController::renderScheduleDetails(Schedule &schedule) {
  ScheduleRecord record;
  if (!schedule.scheduleAt(viewIndex_, record)) {
    setLine(0, "Schedule removed");
    setLine(1, "BACK: Return");
    setLine(2, "");
    setLine(3, "");
    return;
  }
  char line[LCD_COLUMNS + 1];
  snprintf(line, sizeof(line), "ID:%u ACTIVE", record.id);
  setLine(0, line);
  if (detailPage_ == 0) {
    snprintf(line, sizeof(line), "Every %u Hours", record.intervalHours);
    setLine(1, line);
    snprintf(line, sizeof(line), "For %u %s", record.durationValue,
             durationValueName((DurationType)record.durationType, record.durationValue));
    setLine(2, line);
    formatShortDate(record.startEpoch, line, sizeof(line));
    char startLine[LCD_COLUMNS + 1];
    snprintf(startLine, sizeof(startLine), "Start %s", line);
    setLine(3, startLine);
  } else {
    formatShortDate(record.expirationEpoch, line, sizeof(line));
    char expiryLine[LCD_COLUMNS + 1];
    snprintf(expiryLine, sizeof(expiryLine), "Expires %s", line);
    setLine(1, expiryLine);
    formatShortDate(record.reminderPending ? record.pendingEpoch : record.nextReminderEpoch,
                    line, sizeof(line));
    char nextLine[LCD_COLUMNS + 1];
    snprintf(nextLine, sizeof(nextLine), "%s %s",
             record.reminderPending ? "Pending" : "Next", line);
    setLine(2, nextLine);
    setLine(3, "SAVE:Delete BACK");
  }
}

void DisplayController::render(Schedule &schedule) {
  char line[LCD_COLUMNS + 1];
  switch (screen_) {
    case Screen::Normal:
      renderNormal(schedule);
      break;
    case Screen::MainMenu:
      renderOptions("MAIN MENU", 3);
      break;
    case Screen::SelectInterval:
      renderOptions("SELECT TIME INTERVAL", 4);
      break;
    case Screen::SelectDurationType:
      renderOptions("SELECT DURATION", 3);
      break;
    case Screen::SelectDurationValue: {
      static const char *titles[] = {"SELECT DAYS", "SELECT WEEKS", "SELECT MONTHS"};
      renderOptions(titles[durationTypeSelection_], 3);
      break;
    }
    case Screen::SaveConfirm: {
      setLine(0, "SAVE SCHEDULE?");
      snprintf(line, sizeof(line), "Every %u Hours", kIntervalChoices[intervalSelection_]);
      setLine(1, line);
      snprintf(line, sizeof(line), "%u %s",
               kDurationValues[durationTypeSelection_][durationValueSelection_],
               durationValueName((DurationType)durationTypeSelection_,
                 kDurationValues[durationTypeSelection_][durationValueSelection_]));
      setLine(2, line);
      setLine(3, "SAVE=YES BACK=NO");
      break;
    }
    case Screen::ScheduleList:
      renderScheduleList(schedule);
      break;
    case Screen::ScheduleDetails:
      renderScheduleDetails(schedule);
      break;
    case Screen::DeleteConfirm: {
      ScheduleRecord record;
      setLine(0, "DELETE SCHEDULE?");
      if (schedule.scheduleAt(viewIndex_, record))
        snprintf(line, sizeof(line), "Schedule ID:%u", record.id);
      else
        snprintf(line, sizeof(line), "Already removed");
      setLine(1, line);
      setLine(2, "SAVE: YES");
      setLine(3, "BACK: NO");
      break;
    }
    case Screen::Notice:
      if ((int32_t)(millis() - noticeUntil_) >= 0) screen_ = noticeReturn_;
      else {
        setLine(0, noticeLines_[0]);
        setLine(1, noticeLines_[1]);
        setLine(2, noticeLines_[2]);
        setLine(3, "");
      }
      break;
  }
}

void DisplayController::loop(Schedule &schedule) {
  updateButtons();
  handleModeButton(schedule);
  handleMenuButtons(schedule);
  if ((uint32_t)(millis() - lastRefresh_) < 200UL) return;
  lastRefresh_ = millis();
  render(schedule);
}
