#ifndef STATE_H
#define STATE_H

#include <Arduino.h>
#include "config.h"

struct Setting {
  uint8_t cup     = 0;
  uint8_t ramen   = 0;
  uint8_t powder  = 0;
  uint8_t cooker  = 0;
  uint8_t outlet  = 0;
};

struct State {
  // Cup
  int cup_amp[MAX_CUP] = {0};
  int cup_stock[MAX_CUP] = {0};
  int cup_dispense[MAX_CUP] = {0};

  // Ramen
  int ramen_amp[MAX_RAMEN] = {0};
  int ramen_stock[MAX_RAMEN] = {0};
  int ramen_lift[MAX_RAMEN] = {0};
  int ramen_loadcell[MAX_RAMEN] = {0};
  // Powder
  int powder_amp[MAX_POWDER] = {0};
  int powder_dispense[MAX_POWDER] = {0};
  // Cooker
  int cooker_amp[MAX_COOKER] = {0};
  int cooker_work[MAX_COOKER] = {0};
  // Outlet
  int outlet_amp[MAX_OUTLET] = {0};
  int outlet_door[MAX_OUTLET] = {0};
  int outlet_sonar[MAX_OUTLET] = {0};
  int outlet_loadcell[MAX_OUTLET] = {0};
  // Door
  int door_sensor1 = 0;
  int door_sensor2 = 0;
};

extern Setting current;
extern State state;

extern const int PPR;
extern const int CPR; 

extern volatile long encoderCount;
extern volatile int direction;
extern unsigned long lastEncoderReportTime; // 52줄 수정됨
extern long lastCount;

extern const int ENCODER_A_PIN;
extern const int ENCODER_B_PIN;

#endif // STATE_H