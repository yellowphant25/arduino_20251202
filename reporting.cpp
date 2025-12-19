#include <Arduino.h>
#include <ArduinoJson.h>
#include "reporting.h"
#include "config.h" 
#include "state.h"
unsigned long ramenPhotoDebounceTime[MAX_RAMEN] = {0};
int ramenPhotoPrevState[MAX_RAMEN] = {0};            
const unsigned long DEBOUNCE_DELAY_MS = 50;          

// =========================================================
// [추가됨] 전류 센서값 정제 함수 (노이즈 필터 + 데드존)
// =========================================================
int filterAmpValue(int pin, int prevValue) {
  int raw = analogRead(pin);

  // 1. 이동 평균 (Low Pass Filter)
  // 이전 값에 가중치(80%)를 더 많이 주어 값이 튀는 것을 방지
  // 공식: New = (Old * 0.8) + (Raw * 0.2)
  long filtered = ((long)prevValue * 80 + (long)raw * 20) / 100;

  // 2. 데드존 (Deadzone) 처리
  // ADC 값 기준 5 미만(약 0.02A 이하 수준의 노이즈)은 0으로 처리하여 UI를 깔끔하게 함
  if (filtered < 5) {
    filtered = 0;
  }

  return (int)filtered;
}

long readHX711(uint8_t dtPin, uint8_t sckPin) {
  long count = 0;
  
  // 데이터가 준비될 때까지 대기 (DT 핀이 LOW가 되면 준비 완료)
  if (digitalRead(dtPin) == HIGH) return 0; 

  // 24비트 데이터 읽기
  for (int i = 0; i < 24; i++) {
    digitalWrite(sckPin, HIGH);
    delayMicroseconds(1); // Due의 속도가 빠르므로 짧은 지연 추가
    count = count << 1;
    digitalWrite(sckPin, LOW);
    delayMicroseconds(1);
    if (digitalRead(dtPin)) count++;
  }

  // 25번째 펄스로 다음 읽기 설정 (Gain 128 기준)
  digitalWrite(sckPin, HIGH);
  delayMicroseconds(1);
  digitalWrite(sckPin, LOW);
  delayMicroseconds(1);

  // 2's complement 처리 (음수 대응)
  if (count & 0x800000) {
    count |= 0xFF000000;
  }
  
  return count;
}

void readAllSensors() {
  uint8_t i;
  unsigned long now = millis();
  int currentReading;

  for (i = 0; i < current.cup; i++) {
    state.cup_amp[i] = analogRead(CUP_CURR_AIN[i]);
    state.cup_stock[i] = digitalRead(CUP_STOCK_IN[i]);
    state.cup_dispense[i] = digitalRead(CUP_ROT_IN[i]);
  }

  for (i = 0; i < current.ramen; i++) { 
    // 1. 센서의 현재 물리적 핀 상태를 읽음
    currentReading = digitalRead(RAMEN_PRESENT_IN[i]); 
    
    // 2. 현재 읽은 값과 직전 감지된 상태가 다를 경우
    if (currentReading != ramenPhotoPrevState[i]) {
      // 값이 변했으므로, 타이머를 재설정하고 상태 저장 (바운싱 시작)
      ramenPhotoDebounceTime[i] = now;
      ramenPhotoPrevState[i] = currentReading;
    }

    if ((now - ramenPhotoDebounceTime[i]) >= DEBOUNCE_DELAY_MS) {
      state.ramen_stock[i] = currentReading; 
    }

    state.ramen_amp[i] = analogRead(RAMEN_EJ_CURR_AIN[i]);
  }

  for (i = 0; i < current.powder; i++) {
    state.powder_amp[i] = filterAmpValue(POWDER_CURR_AIN[i], state.powder_amp[i]);
    state.powder_dispense[i] = (digitalRead(POWDER_MOTOR_OUT[i]) == HIGH) ? 1 : 0; 
  }

  for (i = 0; i < current.cooker; i++) {
    state.cooker_amp[i] = filterAmpValue(COOKER_CURR_AIN[i], state.cooker_amp[i]);
    // state.cooker_work[i] = ...
  }

  for (i = 0; i < current.outlet; i++) {
    state.outlet_amp[i] = analogRead(OUTLET_CURR_AIN[i]);
    if (outletScale[i].is_ready()) {
      Serial.println('loadcell is ready');
         state.outlet_loadcell[i] = (int)outletScale[i].get_units(5);
    } 
  }

  state.door_sensor1 = digitalRead(DOOR_SENSOR1_PIN);
  state.door_sensor2 = digitalRead(DOOR_SENSOR2_PIN);
}

// 에러 전송
void sendError(const char* device, int control, const char* errorMsg) {
  StaticJsonDocument<256> doc;
  
  doc["device"] = device;
  doc["control"] = control;
  doc["error"] = errorMsg;

  Serial.print('['); 
  serializeJson(doc, Serial);
  Serial.println(']');
}

void publishStateJson() {
  StaticJsonDocument<512> doc;
  uint8_t i;
  bool isFirst = true; // 첫 번째 요소인지 확인하여 콤마(,) 처리를 하기 위한 플래그

  Serial.print('['); 

  // 1. Cup
  for (i = 0; i < current.cup; i++) {
    if (!isFirst) Serial.print(',');
    isFirst = false;

    doc.clear();
    doc["device"] = "cup";
    doc["control"] = i + 1;
    doc["amp"] = checkMotorRunning(i);
    doc["stock"] = state.cup_stock[i];
    doc["dispense"] = state.cup_dispense[i];
    serializeJson(doc, Serial);
  }

  // 2. Ramen
  for (i = 0; i < current.ramen; i++) {
    if (!isFirst) Serial.print(',');
    isFirst = false;

    int slideInStatus = digitalRead(RAMEN_EJ_TOP_IN[i]); // 기본값은 센서 값

    // 인덱스 0 장비에 한하여, 복귀 중(EJECT_RETURNING)일 경우 강제 1 유지
    // (BTM_IN이 1이 되기 전까지 슬라이딩 중으로 간주)
    if (ramenEjectStatus == EJECT_RETURNING) {
      slideInStatus = 1; 
    } else {
      slideInStatus = 0;
    }

    doc.clear();
    doc["device"] = "ramen";
    doc["control"] = i + 1;
    doc["liftup"] = digitalRead(RAMEN_UP_TOP_IN[i]);
    doc["liftdown"] = digitalRead(RAMEN_UP_BTM_IN[i]);
    doc["slidein"] = digitalRead(RAMEN_EJ_BTM_IN[i]); // 면 배출 상한센서
    doc["slideout"] = slideInStatus;
    doc["detect"] = state.ramen_stock[i];
    doc["lift"] = state.ramen_lift[i];
    serializeJson(doc, Serial);
  }

  // 3. Powder
  for (i = 0; i < current.powder; i++) {
    if (!isFirst) Serial.print(',');
    isFirst = false;

    doc.clear();
    doc["device"] = "powder";
    doc["control"] = i + 1;
    doc["amp"] = checkMotorRunning(i);
    doc["dispense"] = state.powder_dispense[i];
    serializeJson(doc, Serial);
  }

  // 4. Cooker
  for (i = 0; i < current.cooker; i++) {
    if (!isFirst) Serial.print(',');
    isFirst = false;

    doc.clear();
    doc["device"] = "cooker";
    doc["control"] = i + 1;
    doc["amp"] = state.cooker_amp[i];
    doc["work"] = state.cooker_work[i];
    serializeJson(doc, Serial);
  }

  // 5. Outlet
  for (i = 0; i < current.outlet; i++) {
    if (!isFirst) Serial.print(',');
    isFirst = false;

    doc.clear();
    doc["device"] = "outlet";
    doc["control"] = i + 1;
    doc["amp"] = checkMotorRunning(i);
    doc["opendoor"] = digitalRead(OUTLET_OPEN_IN[i]);
    doc["closedoor"] = digitalRead(OUTLET_CLOSE_IN[i]);
    doc["sonar"] = state.outlet_sonar[i];
    doc["loadcell"] = state.outlet_loadcell[i];
    serializeJson(doc, Serial);
  }

  // 6. Door (조건부 전송: Cup 또는 Cooker가 1개 이상일 때만) [수정됨]
  if (current.cup > 0 || current.cooker > 0) {
    if (!isFirst) Serial.print(','); // 앞선 데이터가 있다면 콤마 추가
    
    doc.clear();
    doc["device"] = "door";
    doc["sensor1"] = state.door_sensor1;
    doc["sensor2"] = state.door_sensor2;
    serializeJson(doc, Serial);
  }

  // 통합된 JSON 배열 종료
  Serial.println(']'); 
}

void checkVolt() {
  int v = analogRead(A3);
  
  Serial.print("current vol : ");
  Serial.println(v);
}

int checkMotorRunning(int currentIdx) {
  int sendingMotorState = 0;

  if (current.cup > 0 && currentIdx < current.cup) {
    sendingMotorState = digitalRead(CUP_MOTOR_OUT[currentIdx]);
  }

  else if (current.powder > 0 && currentIdx < current.powder) {
    sendingMotorState = digitalRead(POWDER_MOTOR_OUT[currentIdx]);
  }

  else if (current.outlet > 0 && currentIdx < current.outlet) {
    int fwd = digitalRead(OUTLET_FWD_OUT[currentIdx]);
    int rev = digitalRead(OUTLET_REV_OUT[currentIdx]);

    sendingMotorState = (fwd == 0 && rev == 0) ? 0 : (fwd == 1 ? 1 : -1);
  }

  return sendingMotorState;
}
