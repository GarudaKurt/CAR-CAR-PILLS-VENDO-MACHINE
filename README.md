# Pill Reminder and Dispensing Controller

Arduino Mega-class pill reminder controller with a DS3231 RTC, 20×4 I²C LCD, menu-driven schedule setup, EEPROM persistence, buzzer reminders, and servo dispensing.

## Hardware and libraries

- Arduino Mega 2560 or compatible board with the listed pins and internal EEPROM.
- DS3231 RTC connected over I²C. The sketch reads its registers through Arduino's built-in `Wire` library; a separate `DS3231.h` library is not required.
- 20×4 I²C LCD and the `LiquidCrystal_I2C` Arduino library.
- Four continuous-rotation medication servos, one Spoon servo, four active-low IR sensors, five buttons, a buzzer, a relay, and LEDs.

The LCD address defaults to `0x27`. Change `LCD_I2C_ADDRESS` in `deviceConfig.h` to `0x3F` if that is the address of your LCD backpack. The RTC uses I²C address `0x68`. Connect the RTC and LCD to the board's SDA/SCL pins and share ground. On a Mega 2560, SDA/SCL are pins 20/21.

The sketch reads but does not set the DS3231 clock. Set its date and time before use and clear its oscillator-stop flag. Scheduling pauses if the RTC is missing, invalid, or has its oscillator-stop flag set.

## Pin assignments

| Function | Pin |
| --- | ---: |
| NEXT button | 41 Yellow Wire|
| PREVIOUS button | 43 Green Wire|
| SAVE / confirm / reminder acknowledgment button | 45 Blue Wire|
| BACK / cancel button | 47 Violet Wire|
| MODE / schedule-menu button | 39 Orange Wire|
| Every 4 hours servo / IR sensor | 5 Green Wire / 11 Green Wire |
| Every 6 hours servo / IR sensor | 6 Yellow Wire / 12 Yellow Wire |
| Every 8 hours servo / IR sensor | 4 Blue Wire / 10 Blue Wire |
| Every 12 hours servo/ IR sensor | 7 Orange Wire / 13 Orange Wire |
| Spoon servo | 3 Violet Wire |
| Relay / buzzer | 40 Yellow / 42 Green |
| Green LED / red LED | 44 Blue / 46 Green |
| LCD I2C | 21 SDA VIOLET / 22 SCL GRAY |
| RTC I2C | SDA WHITE / SCL BROWN |
All buttons use `INPUT_PULLUP`: connect one side to the assigned pin and the other side to GND. A pressed button reads LOW. The IR sensors are configured active-low; change `IR_ACTIVE_LEVEL` in `deviceConfig.h` if your sensor output polarity differs.

## Button controls

### Mode and menu access

Use the separate MODE button on pin 39 while on the normal screen and when no reminder is pending:

- Hold for **3 seconds to under 5 seconds, then release** to select TEST mode.
- Hold for **5 seconds to under 15 seconds, then release** to select PRODUCTION mode.
- Hold for **15 seconds** to open the schedule main menu. A 15-second hold does not change the mode.
- Holds under 3 seconds do not change the mode.

Mode is retained in EEPROM. Mode changes are ignored while a reminder is pending or dispensing is underway.

### Menu keys

| Button | Menu action |
| --- | --- |
| NEXT, pin 41 | Move to the next option or schedule |
| PREVIOUS, pin 43 | Move to the previous option or schedule |
| SAVE, pin 45 | Confirm, open details, delete after confirmation, or acknowledge a due reminder on the normal screen |
| BACK, pin 47 | Return to the previous screen or cancel |

Buttons are debounced in software. Menu navigation uses short presses. In normal operation, pressing SAVE acknowledges the current due reminder; the buzzer continues until acknowledgment.

## Schedule menu

Open the menu by holding MODE for 15 seconds. The menu includes **Add Schedule**, **View Schedules**, and **Exit**.

### LCD menu flow

```text
MAIN MENU
    |
    +-- Add Schedule
    |      |
    |      +-- Select Interval
    |      |     +-- Every 4 Hours
    |      |     +-- Every 6 Hours
    |      |     +-- Every 8 Hours
    |      |     +-- Every 12 Hours
    |      |
    |      +-- Select Duration
    |      |     +-- Days: 1, 3, or 5
    |      |     +-- Weeks: 1, 2, or 3
    |      |     +-- Months: 1, 6, or 12
    |      |
    |      +-- Save Schedule
    |
    +-- View Schedules
    |      +-- View Details
    |      +-- Delete Schedule (confirmation required)
    |
    +-- Exit
```

The 20×4 LCD shows three options at a time when a list is longer than the available rows. The `>` marker identifies the selected option; use NEXT/PREVIOUS to scroll.

To add a schedule:

1. Select **Add Schedule** and press SAVE.
2. Choose one fixed interval: **4, 6, 8, or 12 hours**. Press SAVE. The menu automatically proceeds to duration selection.
3. Choose **Days**, **Weeks**, or **Months** and press SAVE.
4. Choose a duration and press SAVE: Days are 1, 3, or 5; Weeks are 1, 2, or 3; Months are 1, 6, or 12.
5. Review the summary and press SAVE to create the schedule. BACK returns to the previous step.

The schedule starts at the DS3231's current date and time. Its first reminder is due one selected interval later. Its expiration timestamp is the activation time plus the selected duration. Month durations use calendar months and clamp to the last day of the target month when necessary (for example, January 31 plus one month expires on February's last day).

Select **View Schedules** to browse active schedules. Open a schedule with SAVE; NEXT/PREVIOUS switch between its detail pages. From the details page, SAVE opens a delete confirmation. SAVE confirms deletion; BACK cancels. Expired schedules are removed from EEPROM automatically.

The firmware stores up to 12 active schedule records. Each record has a unique ID, interval, duration type/value, RTC start and expiration timestamps, next reminder time, and pending-reminder state. The EEPROM database has a version and checksum. Active schedules and pending reminders are restored after restart; a restored pending reminder still requires a fresh SAVE acknowledgment.

## Reminder intervals

| Selection | PRODUCTION interval | TEST interval |
| --- | ---: | ---: |
| Every 4 Hours | 4 hours | 5 minutes |
| Every 6 Hours | 6 hours | 6 minutes |
| Every 8 Hours | 8 hours | 8 minutes |
| Every 12 Hours | 12 hours | 12 minutes |

TEST mode preserves the selected 4/6/8/12-hour configuration while shortening the runtime interval. The existing 4-hour and 8-hour test mappings (5 and 8 minutes) are retained; the new 6- and 12-hour options use 6 and 12 minutes.

## Dispensing behavior

When a schedule becomes due, the scheduler records one pending reminder and the controller sounds the buzzer. Press SAVE on the normal screen to acknowledge it. The interval selects the matching medication servo and IR sensor. The servo attaches to dispense and detaches when its IR sensor detects a pill. If no pill is detected within 30 seconds, the servo detaches and that request ends without retrying.

After an IR hit, the Spoon moves to 180°, holds for 4 seconds, and returns to 0°. The disinfection relay then runs for 10 seconds and turns off. Multiple due schedules are handled one at a time. Missed intervals are skipped; unacknowledged pending reminders expire after one interval and never cause automatic dispensing.

`Servo.detach()` stops the servo control pulses; it does not switch off servo power. Verify MG995 behavior on the assembled device before relying on it as the only stop mechanism.

## LCD and Serial Monitor

The normal LCD screen shows the DS3231 date/time, `MODE: TEST` or `MODE: PRODUCTION`, the number of active schedules, and any due schedule. Menu screens show the available intervals, duration values, active schedule list, details, and delete confirmation.

Open the Serial Monitor at **115200 baud** for RTC errors, mode changes, EEPROM schedule saves/deletes, reminder activation, acknowledgment, servo operation, IR detection, timeout, Spoon movement, and relay events.

## Code organization

- `SchedulingModule` reads the DS3231, calculates intervals/expiration, maintains pending reminders, and stores schedule records in EEPROM.
- `DisplayModule` handles the LCD, buttons, menus, mode selection, and reminder acknowledgment.
- `DispenseModule` runs the non-blocking servo, IR, Spoon, buzzer, and relay workflow.
- `deviceConfig.h` contains hardware pins and LCD address.
