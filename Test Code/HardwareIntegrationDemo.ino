#include "Waveshare_10Dof-D.h"
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "DHT.h"
#include "epd2in9_V2.h"
#include "epdpaint.h"

// --- Pins ---
const int BTN_0 = 0;
const int BTN_1 = 1;
const int LED_PIN = 28;
const int BUZZER_PIN = 7;

// DHT22 Sensor
#define DHTPIN 8
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// e-Paper Display
// e-Paper pins are defined in epdif.h (CS: 9, SCK: 10, MOSI: 11, RST: 12, BUSY: 13, DC: 14, PWR: 15)
unsigned char image[(EPD_WIDTH * EPD_HEIGHT) / 8];
Paint paint(image, EPD_WIDTH, EPD_HEIGHT);
Epd epd;

// SD Card (SPI0)
const int SD_CS = 17;
const int SD_SCK = 18;
const int SD_MOSI = 19;
const int SD_MISO = 16;

bool lastBtn0State = HIGH;
bool lastBtn1State = HIGH;
bool isLogging = false;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("--- Hardware Integration Demo ---");

  // 1. Setup Buttons, LED & Buzzer
  pinMode(BTN_0, INPUT_PULLUP);
  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize DHT22
  dht.begin();

  // 2. Setup IMU (I2C1 mapped to pins 26 and 27)
  Wire1.setSDA(26);
  Wire1.setSCL(27);
  IMU_EN_SENSOR_TYPE enMotionSensorType, enPressureType;
  imuInit(&enMotionSensorType, &enPressureType);
  if (enMotionSensorType != IMU_EN_SENSOR_TYPE_ICM20948) {
    Serial.println("IMU Motion sensor not found!");
  } else {
    Serial.println("IMU Initialized.");
  }

  // 3. Setup SD Card (SPI0) using default pins (16, 17, 18, 19)
  
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!");
  } else {
    Serial.println("SD Card initialized.");
    File dataFile = SD.open("datalog.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.println("Time(ms),Roll,Pitch,Yaw,TempC,Humidity,Btn0,Btn1");
      dataFile.close();
    } else {
      Serial.println("Warning: Could not write header to datalog.csv");
    }
  }

  // 4. Setup e-Paper (SPI1)
  if (epd.Init() != 0) {
      Serial.println("e-Paper init failed!");
  } else {
      Serial.println("e-Paper initialized.");
      epd.ClearFrameMemory(0xFF);
      epd.DisplayFrame();
      paint.SetRotate(ROTATE_90); // Landscape
  }

  Serial.println("Ready!");
  Serial.println("Press Button 0 to toggle logging to SD card.");
  Serial.println("Press Button 1 to blink the LED.");
}

void loop() {
  // Read Buttons
  bool currentBtn0State = digitalRead(BTN_0);
  bool currentBtn1State = digitalRead(BTN_1);

  // Toggle SD logging on Button 0 press
  if (currentBtn0State == LOW && lastBtn0State == HIGH) {
    isLogging = !isLogging;
    digitalWrite(LED_PIN, isLogging ? HIGH : LOW);
    
    // Quick buzzer beep to indicate toggle
    tone(BUZZER_PIN, 2000, 100); 

    Serial.print("Button 0 Pressed! SD Logging is now: ");
    Serial.println(isLogging ? "ON" : "OFF");
  }
  
  // Blink LED on Button 1 press
  if (currentBtn1State == LOW && lastBtn1State == HIGH) {
    Serial.println("Button 1 Pressed! Blinking LED...");
    bool currentLed = digitalRead(LED_PIN);
    
    // Quick blink sequence
    for (int i=0; i<3; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(50);
      digitalWrite(LED_PIN, LOW);
      delay(50);
    }
    
    // Restore previous state
    digitalWrite(LED_PIN, currentLed); 
  }

  lastBtn0State = currentBtn0State;
  lastBtn1State = currentBtn1State;

  // Read IMU
  IMU_ST_ANGLES_DATA stAngles;
  IMU_ST_SENSOR_DATA stGyroRawData;
  IMU_ST_SENSOR_DATA stAccelRawData;
  IMU_ST_SENSOR_DATA stMagnRawData;
  imuDataGet(&stAngles, &stGyroRawData, &stAccelRawData, &stMagnRawData);

  // Read DHT22 (Max 1 reading every 2 seconds)
  static unsigned long lastDhtTime = 0;
  static float t = 0.0;
  static float h = 0.0;
  if (millis() - lastDhtTime >= 2000) {
    lastDhtTime = millis();
    t = dht.readTemperature();
    h = dht.readHumidity();
  }

  // If logging, write to SD card every 100ms (10Hz)
  static unsigned long lastLogTime = 0;
  if (isLogging && millis() - lastLogTime >= 100) {
    lastLogTime = millis();
    File dataFile = SD.open("datalog.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.print(millis()); dataFile.print(",");
      dataFile.print(stAngles.fRoll); dataFile.print(",");
      dataFile.print(stAngles.fPitch); dataFile.print(",");
      dataFile.print(stAngles.fYaw); dataFile.print(",");
      dataFile.print(isnan(t) ? 0.0 : t); dataFile.print(",");
      dataFile.print(isnan(h) ? 0.0 : h); dataFile.print(",");
      dataFile.print(currentBtn0State == LOW ? 1 : 0); dataFile.print(",");
      dataFile.println(currentBtn1State == LOW ? 1 : 0);
      dataFile.close();
    } else {
      Serial.println("Error opening datalog.csv");
    }
  }

  // Also print to Serial Plotter format at 20Hz
  static unsigned long lastPrintTime = 0;
  if (millis() - lastPrintTime >= 50) {
    lastPrintTime = millis();
    Serial.print("Roll:");
    Serial.print(stAngles.fRoll);
    Serial.print(",Pitch:");
    Serial.print(stAngles.fPitch);
    Serial.print(",Yaw:");
    Serial.println(stAngles.fYaw);
  }

  // Update e-Paper Display every 3 seconds
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate >= 3000) {
    lastDisplayUpdate = millis();
    
    paint.Clear(1); // UNCOLORED (white)
    char buf[32];
    paint.DrawStringAt(10, 10, "Payload Telemetry", &Font16, 0); // COLORED (black)
    
    snprintf(buf, sizeof(buf), "Temp: %.1fC", isnan(t) ? 0.0 : t);
    paint.DrawStringAt(10, 35, buf, &Font16, 0);
    
    snprintf(buf, sizeof(buf), "Humidity: %.1f%%", isnan(h) ? 0.0 : h);
    paint.DrawStringAt(10, 55, buf, &Font16, 0);
    
    snprintf(buf, sizeof(buf), "Roll: %.1f", stAngles.fRoll);
    paint.DrawStringAt(10, 75, buf, &Font16, 0);
    
    snprintf(buf, sizeof(buf), "Pitch: %.1f", stAngles.fPitch);
    paint.DrawStringAt(10, 95, buf, &Font16, 0);
    
    snprintf(buf, sizeof(buf), "Logging: %s", isLogging ? "ON" : "OFF");
    paint.DrawStringAt(160, 35, buf, &Font16, 0);
    
    epd.SetFrameMemory(paint.GetImage(), 0, 0, EPD_WIDTH, EPD_HEIGHT);
    epd.DisplayFrame();
  }
}
