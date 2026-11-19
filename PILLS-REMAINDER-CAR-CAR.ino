#include "SchedulingModule.h"
#include "DispenseModule.h"
#include "DisplayModule.h"

Schedule medicationSchedule;
Controller dispensingController;
DisplayController userDisplay;

void setup() {
  Serial.begin(115200);
  medicationSchedule.begin();
  dispensingController.begin();
  userDisplay.begin();
}

void loop() {
  medicationSchedule.loop();
  dispensingController.loop(medicationSchedule);
  userDisplay.loop(medicationSchedule);
}
