#ifndef REPORTING_H
#define REPORTING_H

#include <Arduino.h>

void readAllSensors();
void checkVolt();
int checkMotorRunning(int currentIdx);
void publishStateJson();

void handleEncoderA();
void handleEncoderB();
void reportEncoderDebug(unsigned long currentMillis);

// 에러 전송
void sendError(const char* device, int control, const char* errorMsg);

#endif // REPORTING_H