#include <Arduino.h>
#include <ArduinoJson.h>

// 모듈 헤더파일 포함
#include "config.h"     // 핀맵, 상수
#include "state.h"      // Setting/State 구조체, 전역변수 선언
#include "protocol.h"   // 수신 명령
#include "reporting.h"  // 상태 보고

// ===== 전역 변수 정의 =====
Setting current;
State state;
String rx;
unsigned long lastPublishMs = 0;

// ===== 엔코더 관련 설정 =====
const int ENCODER_A_PIN = 2;
const int ENCODER_B_PIN = 3;

const int PPR = 100;           
const int CPR = PPR * 4;      

volatile long encoderCount = 0;
volatile int  direction = 0;

unsigned long lastEncoderReportTime = 0;
long lastCount = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

#ifdef ARDUINO_ARCH_SAM
  analogReadResolution(10);
#endif

  pinMode(DOOR_SENSOR1_PIN, INPUT);
  pinMode(DOOR_SENSOR2_PIN, INPUT);

  Serial.println(F("[{\"boot\":\"ready\"\"}]"));
  lastPublishMs = millis();

  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);

  // A, B 둘 다 인터럽트 사용 (정밀도 높게)
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), handleEncoderA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B_PIN), handleEncoderB, CHANGE);
}

void loop() {
  if (current.cup > 0) {
    checkCupDispense();  
  }
  if (current.ramen > 0) {
    checkRamenRise();   
    checkRamenInit();   
    checkRamenEject();  
  }
  if (current.powder > 0) {
    checkPowderDispense(); 
  }
  if (current.outlet > 0) {
    checkOutlet();
  }

  /*
  Serial.print("면 배출 상한 센서 : ");
  Serial.println(digitalRead(8));
  Serial.print("면 배출 하한 센서 : ");
  Serial.println(digitalRead(9));
  Serial.print("면 상승 상한 센서 : ");
  Serial.println(digitalRead(10));
  Serial.print("면 상승 하한 센서 :");
  Serial.println(digitalRead(11));
  */

  // ================================================
  // 2. [실시간] JSON 명령 수신 (대괄호 [] 지원 수정됨)
  // ================================================
  while (Serial.available()) {
    char c = Serial.read();

    // 1. 시작 문자 '[' 감지 시: 버퍼 초기화 (새로운 패킷 시작으로 간주)
    if (c == '[') {
      rx = ""; 
      continue; 
    }

    // 2. 종료 문자 ']' 감지 시: 명령 실행
    if (c == ']') {
      if (rx.length() > 0) {
        parseAndDispatch(rx.c_str()); // 수신된 문자열 파싱
        rx = ""; // 버퍼 비우기
      }
    } 
    // 3. 그 외 문자는 버퍼에 저장 (단, '['는 위에서 처리했으므로 제외됨)
    else {
      rx += c;
    }
  }

  unsigned long now = millis();
  if (now - lastPublishMs >= PUBLISH_INTERVAL_MS) {
    lastPublishMs = now;

    if (current.cup > 0 || current.ramen > 0 || current.powder > 0 || current.cooker > 0 || current.outlet > 0) {
      readAllSensors(); // Reporting.cpp 에 정의됨
      publishStateJson();
    } else {
      // setting 안된 경우에 보냄
      state.door_sensor1 = digitalRead(DOOR_SENSOR1_PIN);
      state.door_sensor2 = digitalRead(DOOR_SENSOR2_PIN);

      StaticJsonDocument<128> doorDoc;
      doorDoc["device"] = "door";
      doorDoc["sensor1"] = state.door_sensor1;
      doorDoc["sensor2"] = state.door_sensor2;
      
      // 수정: 대괄호 추가
      Serial.print('[');
      serializeJson(doorDoc, Serial);
      Serial.println(']');
    }
  }
}