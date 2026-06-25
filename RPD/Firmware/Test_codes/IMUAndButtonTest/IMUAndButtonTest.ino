#include "Waveshare_10Dof-D.h"
#include <Wire.h>

const int BTN_0 = 0;
const int BTN_1 = 1;
const int LED_PIN = 28;

bool lastBtn0State = HIGH;
bool lastBtn1State = HIGH;

void setup() {
  Serial.begin(115200);
  
  pinMode(BTN_0, INPUT_PULLUP);
  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Wait for Serial to initialize for a moment
  delay(2000);

  // Initialize IMU on pins 26 and 27
  Wire1.setSDA(26);
  Wire1.setSCL(27);
  
  IMU_EN_SENSOR_TYPE enMotionSensorType, enPressureType;
  imuInit(&enMotionSensorType, &enPressureType);
}

void loop() {
  // 1. Read IMU data
  IMU_ST_ANGLES_DATA stAngles;
  IMU_ST_SENSOR_DATA stGyroRawData;
  IMU_ST_SENSOR_DATA stAccelRawData;
  IMU_ST_SENSOR_DATA stMagnRawData;
  
  imuDataGet(&stAngles, &stGyroRawData, &stAccelRawData, &stMagnRawData);
  
  // 2. Format IMU data for Serial Plotter
  // The Arduino Serial Plotter expects comma-separated values.
  // We'll plot Roll, Pitch, and Yaw.
  Serial.print("Roll:");
  Serial.print(stAngles.fRoll);
  Serial.print(",Pitch:");
  Serial.print(stAngles.fPitch);
  Serial.print(",Yaw:");
  Serial.println(stAngles.fYaw);

  // 3. Handle button presses
  bool currentBtn0State = digitalRead(BTN_0);
  bool currentBtn1State = digitalRead(BTN_1);

  if (currentBtn0State == LOW && lastBtn0State == HIGH) {
    Serial.println("BUTTON 0 PRESSED! LED ON");
    digitalWrite(LED_PIN, HIGH);
  }
  
  if (currentBtn1State == LOW && lastBtn1State == HIGH) {
    Serial.println("BUTTON 1 PRESSED! LED OFF");
    digitalWrite(LED_PIN, LOW);
  }

  lastBtn0State = currentBtn0State;
  lastBtn1State = currentBtn1State;

  // Plotter update rate (50ms = 20Hz)
  delay(50); 
}
