#include <Arduino.h>
#include <ArduinoJson.h>
#include "Protocol.h"  // 자신의 헤더
#include "config.h"    // 핀맵
#include "state.h"     // 전역 변수(current, state) 사용
#include "reporting.h"

// ===== 전역 상태 변수 (idx=0 장비 전용 상태) =====
enum RamenEjectState {
  EJECT_IDLE,
  EJECTING,
  EJECT_RETURNING
};
RamenEjectState ramenEjectStatus = EJECT_IDLE;

bool isPowderDispensing[MAX_POWDER] = { false };
unsigned long powderStartTime[MAX_POWDER] = { 0 };
unsigned long powderDuration[MAX_POWDER] = { 0 };

volatile long cur_encoder1 = 0;
unsigned long start_encoder1 = 0;
unsigned long interval = 1000;

long startCupReleaseTime[MAX_CUP] = {0};
long cupReleaseInterval = 500;

// =======================================================
// === 1. 설정 (Setup) 및 파싱 (Parse) 함수
// =======================================================

void replyCurrentSetting(const Setting& s) {
  StaticJsonDocument<256> doc;
  doc["device"] = "setting";
  if (s.cup) doc["cup"] = s.cup;
  if (s.ramen) doc["ramen"] = s.ramen;
  if (s.powder) doc["powder"] = s.powder;
  if (s.cooker) doc["cooker"] = s.cooker;
  if (s.outlet) doc["outlet"] = s.outlet;

  // 수정: 대괄호로 감싸서 전송
  Serial.print('[');
  serializeJson(doc, Serial);
  Serial.println(']');
}

// ===== 핀모드 설정 (Count 기반 복구) =====
void setupCup(uint8_t n) {
  for (uint8_t i = 0; i < n; i++) {
    pinMode(CUP_MOTOR_OUT[i], OUTPUT);
    pinMode(CUP_ROT_IN[i], INPUT_PULLUP);
    pinMode(CUP_DISP_IN[i], INPUT_PULLUP);
    pinMode(CUP_STOCK_IN[i], INPUT_PULLUP);
  }
}
void setupRamen(uint8_t n) {
  for (uint8_t i = 0; i < n; i++) {
    Serial.print("ramen setup idx : ");
    Serial.println(i);

    pinMode(RAMEN_UP_FWD_OUT[i], OUTPUT);
    pinMode(RAMEN_UP_REV_OUT[i], OUTPUT);
    pinMode(RAMEN_EJ_FWD_OUT[i], OUTPUT);
    pinMode(RAMEN_EJ_REV_OUT[i], OUTPUT);
    pinMode(RAMEN_EJ_TOP_IN[i], INPUT_PULLUP);
    pinMode(RAMEN_EJ_BTM_IN[i], INPUT_PULLUP);
    pinMode(RAMEN_UP_TOP_IN[i], INPUT_PULLUP);
    pinMode(RAMEN_UP_BTM_IN[i], INPUT_PULLUP);
    pinMode(RAMEN_PRESENT_IN[i], INPUT_PULLUP);
  }

  // [수정] 모든 RAMEN_ENCODER 핀 설정 (i 인덱스 사용)
  // for (uint8_t i = 0; i < MAX_RAMEN * 2; i++) {
  //   pinMode(RAMEN_ENCODER[i], INPUT_PULLUP);
  // }
}
void setupPowder(uint8_t n) {
  for (uint8_t i = 0; i < n; i++) {
    pinMode(POWDER_MOTOR_OUT[i], OUTPUT);
  }
}

void setupOutlet(uint8_t n) {
  for (uint8_t i = 0; i < n; i++) {
    pinMode(OUTLET_FWD_OUT[i], OUTPUT);
    pinMode(OUTLET_REV_OUT[i], OUTPUT);
    pinMode(OUTLET_OPEN_IN[i], INPUT_PULLUP);
    pinMode(OUTLET_CLOSE_IN[i], INPUT_PULLUP);
  }
}
void setupCooker(uint8_t n) {
  for (uint8_t i = 0; i < n; i++) {
    if (i < 2) {
      pinMode(COOKER_IND_SIG[i], OUTPUT);
      pinMode(COOKER_WTR_SIG[i], OUTPUT);
    } else {
      pinMode(COOKER_IND_SIG[i], INPUT);
      pinMode(COOKER_WTR_SIG[i], INPUT);
    }
  }
}

// ===== 설정 적용 및 검증 (Setting 시 호출) =====
bool validateRules(const Setting& s, String& why) {
  uint8_t nonzeroCnt = (s.cup ? 1 : 0) + (s.ramen ? 1 : 0) + (s.powder ? 1 : 0) + (s.cooker ? 1 : 0) + (s.outlet ? 1 : 0);
  if (s.cup > MAX_CUP) {
    why = "cup max=4";
    return false;
  }
  if (s.ramen > MAX_RAMEN) {
    why = "ramen max=4";
    return false;
  }
  if (s.powder > MAX_POWDER) {
    why = "powder max=8";
    return false;
  }
  if (s.cooker > MAX_COOKER) {
    why = "cooker max=8";
    return false;
  }
  if (s.outlet > MAX_OUTLET) {
    why = "outlet max=4";
    return false;
  }
  if (nonzeroCnt == 0) {
    why = "no device count set";
    return false;
  }
  if (s.cup > 0 && s.cooker > 0) {
    if (s.ramen == 0 && s.powder == 0 && s.outlet == 0) return true;
    why = "only cup+cooker can be combined";
    return false;
  }
  if (nonzeroCnt == 1) return true;
  why = "invalid combination (only cup+cooker together; others solo)";
  return false;
}

void applySetting(const Setting& s) {
  if (s.cup) setupCup(s.cup);
  if (s.ramen) setupRamen(s.ramen);
  if (s.powder) setupPowder(s.powder);
  if (s.outlet) setupOutlet(s.outlet);
  if (s.cooker) setupCooker(s.cooker);
  current = s;  // 전역 변수 'current'에 적용
}

// =======================================================
// === 2. 비동기 제어 함수 (Start / Check)
// =======================================================

void startCupDispense(uint8_t idx) {
  Serial.print("명령: 용기 배출 시작 (장비: ");
  Serial.print(idx + 1);
  Serial.println(")");
  digitalWrite(CUP_MOTOR_OUT[idx], HIGH);
}

void checkCupDispense() {
    for (uint8_t i = 0; i < current.cup; i++) {
      if (digitalRead(CUP_MOTOR_OUT[i]) == HIGH) {
        if (startCupReleaseTime[i] == 0) {
          startCupReleaseTime[i] = millis();
        }

        long now = millis();
        long elapsedTime = now - startCupReleaseTime[i];
        if (elapsedTime >= cupReleaseInterval) {
          if (digitalRead(CUP_DISP_IN[i]) == LOW) {
          Serial.print("완료: 용기 배출 중지 (장비: ");
          Serial.print(i + 1);
          Serial.println(")");
          digitalWrite(CUP_MOTOR_OUT[i], LOW);

          startCupReleaseTime[i] = 0;
        }
      }
    }
  }
}

void startRamenRise(uint8_t idx) {
  Serial.print("명령: 면 상승 시작 (장비: ");
  Serial.print(idx + 1);
  Serial.println(")");
  // noInterrupts();
  // start_encoder1 = cur_encoder1;
  // interrupts();
  // Serial.print("시작 엔코더 값: ");
  // Serial.println(start_encoder1);
  digitalWrite(RAMEN_UP_FWD_OUT[idx], HIGH);
}

void checkRamenRise() {
  for (uint8_t i = 0; i < current.ramen; i++) {
    if (digitalRead(RAMEN_UP_FWD_OUT[i]) == HIGH) {
      bool stopMotor = false;
      if (i == 0) {
        // long current_encoder_safe;
        // noInterrupts();
        // current_encoder_safe = cur_encoder1;
        // interrupts();
        // if (current_encoder_safe - start_encoder1 > interval) { stopMotor = true; }
      }


      if (digitalRead(RAMEN_PRESENT_IN[i]) == LOW) {
        Serial.println("포토 센서 LOW");
        stopMotor = true;
      } else if (digitalRead(RAMEN_UP_TOP_IN[i]) == HIGH) {
        Serial.println("면상승 상한센서 HIGH");
        stopMotor = true;
      }

      if (stopMotor) {
        Serial.print("완료: 상승 동작 중지 (장비: ");
        Serial.print(i + 1);
        Serial.println(")");
        digitalWrite(RAMEN_UP_FWD_OUT[i], LOW);
        stopMotor = false;
      }
    }
  }
}

/**
 * @brief 
 */
void startRamenInit(uint8_t idx) {
  Serial.print("명령: 면 하강 시작 (장비: ");
  Serial.print(idx + 1);
  Serial.println(")");
  digitalWrite(RAMEN_UP_REV_OUT[idx], HIGH);
}

/**
 * @brief 
 */
void checkRamenInit() {
  for (uint8_t i = 0; i < current.ramen; i++) {
    if (digitalRead(RAMEN_UP_REV_OUT[i]) == HIGH) {
      if (digitalRead(RAMEN_UP_BTM_IN[i]) == HIGH) {
        Serial.print("완료: 하강 동작 중지 (장비: ");
        Serial.print(i + 1);
        Serial.println(")");
        digitalWrite(RAMEN_UP_REV_OUT[i], LOW);
      }
    }
  }
}

/**
 * @brief 
 */
void startRamenEject(uint8_t idx) {
  //
  if (idx == 0) {
    if (ramenEjectStatus == EJECT_IDLE) {
      Serial.print("명령: 면 배출 시작 (장비: ");
      Serial.print(idx + 1);
      Serial.println(")");
      ramenEjectStatus = EJECTING;
      digitalWrite(RAMEN_EJ_FWD_OUT[idx], HIGH);
    } else {
      Serial.print("Warning: Eject command ignored. Status is not IDLE.");
    }
  } else {
    // 2번 장비 이후는 상태머신 없이 즉시 동작 (단순 ON)
    digitalWrite(RAMEN_EJ_FWD_OUT[idx], HIGH);
  }
}

/**
 * @brief 
 */
void checkRamenEject() {
  if (current.ramen > 0) {
    switch (ramenEjectStatus) {
      case EJECTING:
        if (digitalRead(RAMEN_EJ_TOP_IN[0]) == HIGH) {
          Serial.println("상태: 배출 상한 도달. 복귀 시작 (장비: 1)");
          digitalWrite(RAMEN_EJ_FWD_OUT[0], LOW);
          digitalWrite(RAMEN_EJ_REV_OUT[0], HIGH);
          ramenEjectStatus = EJECT_RETURNING;
        }
        break;
      case EJECT_RETURNING:
        if (digitalRead(RAMEN_EJ_BTM_IN[0]) == HIGH) {
          Serial.println("완료: 상승 하한 감지. 배출 복귀 모터 정지 (장비: 1)");
          digitalWrite(RAMEN_EJ_REV_OUT[0], LOW);
          ramenEjectStatus = EJECT_IDLE;
        }
        break;
      default: break;
    }
  }

  // 2. 단순 감시 (idx > 0 포함 모든 장비)
  for (uint8_t i = 0; i < current.ramen; i++) {
    if (digitalRead(RAMEN_EJ_FWD_OUT[i]) == HIGH && digitalRead(RAMEN_EJ_TOP_IN[i]) == HIGH) {
      digitalWrite(RAMEN_EJ_FWD_OUT[i], LOW);
    }
    if (digitalRead(RAMEN_EJ_REV_OUT[i]) == HIGH && digitalRead(RAMEN_EJ_BTM_IN[i]) == HIGH) {
      digitalWrite(RAMEN_EJ_REV_OUT[i], LOW);
    }
  }
}


/**
 * @brief [수정] 스프 배출을 시작 (지정된 장비, 지정된 시간)
 */
void startPowderDispense(uint8_t idx, unsigned long durationMs) {
  if (isPowderDispensing[idx] == false) {
    Serial.print("명령: 스프 배출 시작 (장비: ");
    Serial.print(idx + 1);
    Serial.print(", 시간: ");
    Serial.print(durationMs);
    Serial.println("ms)");

    isPowderDispensing[idx] = true;
    powderDuration[idx] = durationMs;
    powderStartTime[idx] = millis();
    digitalWrite(POWDER_MOTOR_OUT[idx], HIGH);
  }
}

/**
 * @brief 🟢 [복구] "모든" 스프 배출 타이머를 확인 (loop에서 계속 호출)
 */
void checkPowderDispense() {
  for (uint8_t i = 0; i < current.powder; i++) {
    if (isPowderDispensing[i]) {
      if (millis() - powderStartTime[i] >= powderDuration[i]) {
        Serial.print("완료: 시간 경과. 스프 배출 중지 (장비: ");
        Serial.print(i + 1);
        Serial.println(")");
        digitalWrite(POWDER_MOTOR_OUT[i], LOW);
        isPowderDispensing[i] = false;
      }
    }
  }
}

/**
 * @brief [수정] 배출구 오픈 시작 (모든 장비)
 */
void startOutletOpen(int pinIdx) {
  Serial.print("명령: 배출구 오픈 시작 (장비: ");
  Serial.print(pinIdx + 1);
  Serial.println(")");
  digitalWrite(OUTLET_REV_OUT[pinIdx], LOW); 
  digitalWrite(OUTLET_FWD_OUT[pinIdx], HIGH);
}

/**
 * @brief [수정] 배출구 닫기 시작 (모든 장비)
 */
void startOutletClose(int pinIdx) {
  Serial.print("명령: 배출구 닫기 시작 (장비: ");
  Serial.print(pinIdx + 1);
  Serial.println(")");
  digitalWrite(OUTLET_FWD_OUT[pinIdx], LOW);
  digitalWrite(OUTLET_REV_OUT[pinIdx], HIGH);
}

/**
 * @brief 🟢 [복구] 배출구 오픈/닫힘 멈춤 조건을 "모든 장비"에 대해 확인
 * (loop()에서 계속 호출)
 */
void checkOutlet() {
  for (uint8_t i = 0; i < current.outlet; i++) {
    if (digitalRead(OUTLET_FWD_OUT[i]) == HIGH) {
      if (digitalRead(OUTLET_OPEN_IN[i]) == LOW) {
        Serial.print("완료: 배출구 오픈 완료 (장비: ");
        Serial.print(i + 1);
        Serial.println(")");
        digitalWrite(OUTLET_FWD_OUT[i], LOW);
      }
    }

    if (digitalRead(OUTLET_REV_OUT[i]) == HIGH) {
      if (digitalRead(OUTLET_CLOSE_IN[i]) == LOW) {
        Serial.print("완료: 배출구 닫힘 완료 (장비: ");
        Serial.print(i + 1);
        Serial.println(")");
        digitalWrite(OUTLET_REV_OUT[i], LOW);
      }
    }
  }
}

// =======================================================
// === 3. JSON 명령 핸들러 (API 2.x)
// =======================================================

bool handleCupCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.cup) {
    sendError("cup", control, "invalid cup control num");
    return false;
  }

  uint8_t idx = control - 1;

  if (strcmp(func, "startdispense") == 0) {
    startCupDispense(idx);
    Serial.println("cup startdispense");
  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(CUP_MOTOR_OUT[idx], LOW);
    Serial.println("cup stopdispense");
  } else {
    Serial.println("unknown cup function");
  }
  return true;
}

bool handleRamenCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.ramen) {
    sendError("ramen", control, "invalid ramen control num");
    return false;
  }
  uint8_t idx = control - 1;
  Serial.println("start handle ramen");

  if (strcmp(func, "startdispense") == 0) {
    startRamenEject(idx);
    Serial.println("ramen startdispense");
  } else if (strcmp(func, "readydispense") == 0) {
    startRamenRise(idx);
    Serial.println("ramen readydispense");
  } else if (strcmp(func, "initdispense") == 0) {
    startRamenInit(idx);
    Serial.println("ramen initdispense");
  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(RAMEN_EJ_FWD_OUT[idx], LOW);
    digitalWrite(RAMEN_EJ_REV_OUT[idx], LOW);
    digitalWrite(RAMEN_UP_FWD_OUT[idx], LOW);
    digitalWrite(RAMEN_UP_REV_OUT[idx], LOW);
    if (idx == 0) { ramenEjectStatus = EJECT_IDLE; }
    Serial.println("ramen stopdispense (ALL STOP)");
  } else {
    sendError("ramen", control, "unknown ramen function");
  }
  return true;
}

bool handlePowderCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.powder) {
    sendError("powder", control, "invalid powder control num");
    return false;
  }
  uint8_t idx = control - 1;

  if (strcmp(func, "startdispense") == 0) {

    int time_val = doc["time"] | 0;

    if (time_val <= 0) {
      sendError("powder", control, "Error: 'time' 0 or missing");
      return false;
    }

    unsigned long durationMs = (unsigned long)time_val * 100;

    Serial.print("powder startdispense (장비: ");
    Serial.print(idx + 1);
    Serial.print(", 시간: ");
    Serial.print(durationMs);
    Serial.println(" ms)");

    startPowderDispense(idx, durationMs);
  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(POWDER_MOTOR_OUT[idx], LOW);
    isPowderDispensing[idx] = false;
    Serial.println("powder stopdispense");
  } else {
    sendError("powder", control, "unknown powder function");
  }
  return true;
}

bool handleCookerCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.cooker) {
    sendError("cooker", control, "invalid cooker control num");
    return false;
  }
  uint8_t idx = control - 1;

  if (strcmp(func, "startcook") == 0) {
    int water = doc["water"];
    int timer = doc["timer"];
    if (idx < 2) {
      digitalWrite(COOKER_WTR_SIG[idx], HIGH);
      digitalWrite(COOKER_IND_SIG[idx], HIGH);
    }
    Serial.println("cooker startcook");

  } else if (strcmp(func, "stopcook") == 0) {
    if (idx < 2) {
      digitalWrite(COOKER_WTR_SIG[idx], LOW);
      digitalWrite(COOKER_IND_SIG[idx], LOW);
    }
    Serial.println("cooker stopcook");

  } else {
    sendError("cooker", control, "unknown cooker function");
  }
  return true;
}

bool handleOutletCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  uint8_t idx = control - 1;

  if (strcmp(func, "opendoor") == 0) {
    startOutletOpen(idx);
    Serial.println("outlet opendoor");

  } else if (strcmp(func, "closedoor") == 0) {
    startOutletClose(idx);
    Serial.println("outlet closedoor");

  } else if (strcmp(func, "stopoutlet") == 0) {
    digitalWrite(OUTLET_FWD_OUT[idx], LOW);
    digitalWrite(OUTLET_REV_OUT[idx], LOW);
    Serial.println("outlet stopoutlet");

  } else {
    sendError("outlet", control, "unknown outlet function");
  }
  return true;
}

// =======================================================
// === 4. 메인 파서 (Main Parser)
// =======================================================

bool handleSettingJson(const JsonDocument& doc) {
  Setting next;
  next.cup = doc["cup"] | 0;
  next.ramen = doc["ramen"] | 0;
  next.powder = doc["powder"] | 0;
  next.cooker = doc["cooker"] | 0;
  next.outlet = doc["outlet"] | 0;

  String reason = "";
  if (!validateRules(next, reason)) {
    // 설정 유효성 실패
    sendError("setting", 0, reason.c_str());
  }

  applySetting(next);
  Serial.println("pins configured");
  return true;
}

void checkSensor() { /* ... */
}

bool parseAndDispatch(const char* json) {
  StaticJsonDocument<512> doc;

  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    sendError("system", 0, "json parse fail");
  }

  const char* dev = doc["device"] | "";

  if (strcmp(dev, "setting") == 0) {
    return handleSettingJson(doc);
  } else if (strcmp(dev, "query") == 0) {
    replyCurrentSetting(current);
    return true;
  } else if (strcmp(dev, "cup") == 0) {
    return handleCupCommand(doc);
  } else if (strcmp(dev, "ramen") == 0) {
    return handleRamenCommand(doc);
  } else if (strcmp(dev, "powder") == 0) {
    return handlePowderCommand(doc);
  } else if (strcmp(dev, "cooker") == 0) {
    return handleCookerCommand(doc);
  } else if (strcmp(dev, "outlet") == 0) {
    return handleOutletCommand(doc);
  } else {
    sendError("system", 0, "unsupported device field");
  }
}