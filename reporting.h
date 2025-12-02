#ifndef REPORTING_H
#define REPORTING_H

#include <Arduino.h>

void readAllSensors();
void checkVolt();
void publishStateJson();

void handleEncoderA();
void handleEncoderB();
void reportEncoderDebug(unsigned long currentMillis);

#endif // REPORTING_H