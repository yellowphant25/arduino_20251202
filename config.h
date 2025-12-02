#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ===== 최대치 정의 =====
const uint8_t MAX_CUP     = 4;
const uint8_t MAX_RAMEN   = 4;
const uint8_t MAX_POWDER  = 8;
const uint8_t MAX_COOKER  = 8;
const uint8_t MAX_OUTLET  = 4;

// ===== 1. cup 핀맵 =====
const uint8_t CUP_MOTOR_OUT[4]   = {4, 8, 12, 24};
const uint8_t CUP_ROT_IN[4]      = {5, 9, 13, 25};
const uint8_t CUP_DISP_IN[4]     = {6, 10, 22, 26}; 
const uint8_t CUP_STOCK_IN[4]    = {7, 11, 23, 27};
const uint8_t CUP_COOK_START[4]  = {32, 33, 34, 35};
const uint8_t CUP_SOLENOID[4]    = {36, 37, 38, 39};

const uint8_t CUP_CURR_AIN[4]    = {A0, A1, A2, A3};
const uint8_t CUP_COOK_AIN[4]    = {A6, A7, A8, A9};

// ===== 2. ramen 핀맵 =====
const uint8_t RAMEN_UP_FWD_OUT[4] = {4, 13, 30, 39};
const uint8_t RAMEN_UP_REV_OUT[4] = {5, 22, 31, 40};
const uint8_t RAMEN_EJ_FWD_OUT[4] = {6, 23, 32, 41};
const uint8_t RAMEN_EJ_REV_OUT[4] = {7, 24, 33, 42};
const uint8_t RAMEN_EJ_TOP_IN[4]  = {8, 25, 34, 43};
const uint8_t RAMEN_EJ_BTM_IN[4]  = {9, 26, 35, 44};
const uint8_t RAMEN_UP_TOP_IN[4]  = {10, 27, 36, 45};
const uint8_t RAMEN_UP_BTM_IN[4]  = {11, 28, 37, 46};
const uint8_t RAMEN_PRESENT_IN[4] = {12, 29, 38, 47};
const uint8_t RAMEN_UP_CURR_AIN[4]  = {A0, A2, A4, A6}; // 모터 전류 센서
const uint8_t RAMEN_EJ_CURR_AIN[4]  = {A1, A3, A5, A7}; // 리니어 엑추에이터 전류 센서

const uint8_t RAMEN_ENCODER[8] = {2, 3, 16, 17, 18, 19, 20, 21};

// ===== 3. powder 핀맵 =====
const uint8_t POWDER_MOTOR_OUT[8] = {4,5,6,7,8,9,10,11};
const uint8_t POWDER_CURR_AIN[8]  = {A0,A1,A2,A3,A4,A5,A6,A7};

// ===== 4. outlet 핀맵 =====
const uint8_t OUTLET_FWD_OUT[4]   = {4, 8, 12, 24};
const uint8_t OUTLET_REV_OUT[4]   = {5, 9, 13, 25};
const uint8_t OUTLET_OPEN_IN[4]   = {6,10,22,26};
const uint8_t OUTLET_CLOSE_IN[4]  = {7,11,23,27};
const uint8_t OUTLET_CURR_AIN[4]  = {A0, A3, A6, A9};
const uint8_t OUTLET_LOAD_AIN[4]  = {A1, A4, A7, A10};
const uint8_t OUTLET_USONIC_AIN[4]= {A2, A5, A8, A11};

// ===== 5. cooker 핀맵 =====
const uint8_t COOKER_IND_SIG[4]   = {32,33,34,35};
const uint8_t COOKER_WTR_SIG[4]   = {36,37,38,39};
const uint8_t COOKER_CURR_AIN[4]  = {A6, A7, A8, A9};

// ===== 6. door 핀맵 =====
const uint8_t DOOR_SENSOR1_PIN = 14;
const uint8_t DOOR_SENSOR2_PIN = 15;

// ===== 7. 동작 파라미터 =====
const unsigned long PUBLISH_INTERVAL_MS = 100; // 0.1초

#endif // CONFIG_H