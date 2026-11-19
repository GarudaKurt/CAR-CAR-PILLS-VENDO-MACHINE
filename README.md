# Pill Reminder and Dispensing Controller

Arduino project for four independently configurable medication schedules. The DS3231 RTC supplies the date and time used to decide when reminders are due. A reminder never starts dispensing on its own: personnel must press the matching medication button.

## Hardware and libraries

- Arduino board with enough digital pins for the assignments below (the current pin map is intended for a Mega-class board).
- DS3231 RTC module connected to the I2C bus.
- 20×4 I2C LCD connected to the same I2C bus.
- `LiquidCrystal_I2C` library for the LCD. The RTC is read directly with Arduino's built-in `Wire` library; a separate `DS3231.h` library is not required.
- Four continuous-rotation medication servos, one Spoon servo, four active-low IR sensors, five push buttons, a buzzer, relay, and LEDs.

The LCD address defaults to `0x27`. If the LCD backpack uses `0x3F`, change `LCD_I2C_ADDRESS` in `deviceConfig.h`. The RTC uses I2C address `0x68`. Connect both modules to the board's SDA/SCL pins and share ground.

The sketch reads the RTC but does not set its clock. Set the DS3231 date/time before use and make sure its oscillator-stop flag is clear; otherwise the scheduler reports the RTC as unavailable and pauses reminders. On an Arduino Mega, SDA/SCL are pins 20/21.

## Pin assignments

| Function | Pin |
| --- | ---: |
| 1-hour medication servo / button / IR | 7 / 41 / 13 |
| 2-hour medication servo / button / IR | 6 / 43 / 12 |
| 4-hour medication servo / button / IR | 5 / 45 / 11 |
| 8-hour medication servo / button / IR | 4 / 47 / 10 |
| Spoon servo | 3 |
| TEST/PRODUCTION mode button | 39 |
| Disinfection relay | 40 |
| Buzzer | 46 |
| Green LED / red LED | 49 / 48 |
| LCD I2C address | `0x27` by default |

Buttons use `INPUT_PULLUP`: connect one side of each button to its assigned pin and the other side to GND. A pressed button reads LOW. IR sensors are also configured active-low; change `IR_ACTIVE_LEVEL` in `deviceConfig.h` if the sensor output polarity differs.

The red LED and buzzer pins were moved from the conflicting original assignments: the original red LED pin overlapped the 1-hour button, and the original buzzer pin overlapped the 2-hour button. Wire them to pins 48 and 46 respectively.

## Button configuration

Perform configuration while the controller is ready, with no reminder waiting and no dispensing operation in progress.

### Register or remove a medication schedule

Use the button associated with the interval you want:

| Button | Schedule |
| --- | --- |
| `BtnOneHr` (41) | Every 1 hour |
| `BtnTwoHr` (43) | Every 2 hours |
| `BtnFourHr` (45) | Every 4 hours |
| `BtnEightHr` (47) | Every 8 hours |

- Hold a schedule button for **5 seconds or more, but less than 10 seconds, then release** to register that schedule.
- Hold it for **10 seconds** to unregister that schedule. Removal happens when the hold reaches 10 seconds.
- A short press under 5 seconds does not change registration while the system is ready.
- Only registered schedules will generate reminders. For example, to use only the 2-hour and 4-hour schedules, register buttons 43 and 45 and ensure buttons 41 and 47 are unregistered.

The registered schedule selection is saved in the Arduino board's internal EEPROM and is retained after restart. Registration starts a fresh interval using the current RTC time. After a restart, enabled schedules also start a fresh interval from the current RTC time; individual next-due timestamps are not saved.

### Select TEST or PRODUCTION mode

Use the separate mode button on pin 39 while the controller is ready:

- Hold for **3 seconds, then release before 5 seconds** to select TEST mode.
- Hold until **5 seconds** to select PRODUCTION mode. PRODUCTION is selected at the 5-second mark, even if the button remains held.
- Holds shorter than 3 seconds do not change mode.

The selected mode is saved in internal EEPROM. Changing mode recalculates the next reminder for every registered schedule from the DS3231's current time. Mode changes are ignored while a reminder is queued or dispensing is in progress.

## Reminder intervals

| Schedule | TEST mode | PRODUCTION mode |
| --- | ---: | ---: |
| EveryOneHr | 1 minute 15 seconds | 1 hour |
| EveryTwoHr | 2 minutes 20 seconds | 2 hours |
| EveryFourHr | 5 minutes | 4 hours |
| EveryEightHr | 8 minutes | 8 hours |

## Reminder and dispensing sequence

1. The scheduler polls the DS3231 and queues a reminder when a registered schedule becomes due.
2. The buzzer sounds until personnel press the matching medication button. If several schedules are due, they are handled one at a time in queue order.
3. The matching medication servo attaches and rotates. Its matching IR sensor stops it by detaching the servo when the pill is detected.
4. The Spoon servo moves to 180°, holds for 4 seconds, and returns to 0°.
5. The disinfection relay turns on for 10 seconds, then turns off and the controller returns to ready state.

The medication servo detaches after 30 seconds if its IR sensor does not detect a pill. That request is ended without repeating the dispense. An unacknowledged reminder expires after one full interval; stale or missed periods are skipped rather than dispensing automatically.

`Servo.detach()` stops the servo control pulses; it does not switch off the servo's power supply. Verify the MG995 behavior on the assembled hardware before relying on the mechanism.

## LCD and Serial Monitor

The LCD shows the DS3231 date/time, the current mode as `MODE: TEST` or `MODE: PRODUCTION`, which intervals are registered, and the pending schedule when a reminder is active. The ready screen shows the schedule-button hold instructions.

Open the Serial Monitor at **115200 baud** for the mode, interval/deadline, reminder, acknowledgment, servo, IR, Spoon, relay, RTC, and error logs.

## Build notes

The sketch is split into `SchedulingModule`, `DispenseModule`, and `DisplayModule`, with pin and LCD-address settings in `deviceConfig.h`. Install the `LiquidCrystal_I2C` Arduino library before compiling. Confirm the library's constructor/init API matches the installed version if your library package differs.
