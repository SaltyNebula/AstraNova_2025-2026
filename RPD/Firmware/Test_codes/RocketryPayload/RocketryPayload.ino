#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include "DHT.h"
#include "Waveshare_10Dof-D.h"
#include "epd2in9_V2.h"
#include "epdpaint.h"

// --- Pin Definitions ---
// Buttons & LED
const int BTN_0 = 0;
const int BTN_1 = 1;
const int LED_PIN = 28;

// SD Card (SPI0)
const int SD_CS = 17;
const int SD_SCK = 18;
const int SD_MOSI = 19;
const int SD_MISO = 20;

// DHT22
#define DHTPIN 16
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// e-Paper
// Buffer size: 296x128 pixels / 8 bits per pixel = 4736 bytes
unsigned char image[(EPD_WIDTH * EPD_HEIGHT) / 8];
Paint paint(image, EPD_WIDTH, EPD_HEIGHT);
Epd epd;

// IMU State Variables
IMU_EN_SENSOR_TYPE enMotionSensorType, enPressureType;

// Timers
unsigned long lastDisplayUpdate = 0;
unsigned long lastSdLog = 0;
const unsigned long DISPLAY_INTERVAL = 3000; // 3 seconds
const unsigned long SD_LOG_INTERVAL = 10000; // 10 seconds

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Starting Rocketry Payload System...");

  // 1. Init DHT22
  dht.begin();
  
  // 1.5 Init Buttons & LED
  pinMode(BTN_0, INPUT_PULLUP);
  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // 2. Init IMU (I2C1 mapped to 26 and 27 to prevent conflict with SD Card and Buttons)
  Wire1.setSDA(26);
  Wire1.setSCL(27);
  imuInit(&enMotionSensorType, &enPressureType);
  if (enMotionSensorType != IMU_EN_SENSOR_TYPE_ICM20948) {
      Serial.println("IMU Motion sensor not found!");
  } else {
      Serial.println("IMU initialized successfully.");
  }

  // 3. Init SD Card (SPI0)
  SPI.setRX(SD_MISO);
  SPI.setTX(SD_MOSI);
  SPI.setSCK(SD_SCK);
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!");
  } else {
    Serial.println("SD Card initialized.");
    File dataFile = SD.open("flight.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.println("TimeMs,TempC,Humidity,Roll,Pitch,Yaw,AccelX,AccelY,AccelZ,GyroX,GyroY,GyroZ,MagX,MagY,MagZ,Pressure,Alt");
      dataFile.close();
    }
  }

  // 4. Init e-Paper (SPI1 - configured in epdif.cpp)
  if (epd.Init() != 0) {
      Serial.println("e-Paper init failed!");
  } else {
      Serial.println("e-Paper initialized.");
      epd.ClearFrameMemory(0xFF);
      epd.DisplayFrame();
      paint.SetRotate(ROTATE_90); // Landscape
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // Handle Display Update (every 3s)
  if (currentMillis - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    lastDisplayUpdate = currentMillis;
    updateDisplay();
  }

  // Handle SD Card Logging (every 10s)
  if (currentMillis - lastSdLog >= SD_LOG_INTERVAL) {
    lastSdLog = currentMillis;
    logDataToSD(currentMillis);
  }
}

void updateDisplay() {
  // Read DHT Data
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  
  // Read IMU Data
  IMU_ST_ANGLES_DATA stAngles;
  IMU_ST_SENSOR_DATA stGyroRawData;
  IMU_ST_SENSOR_DATA stAccelRawData;
  IMU_ST_SENSOR_DATA stMagnRawData;
  int32_t s32PressureVal = 0, s32TemperatureVal = 0, s32AltitudeVal = 0;

  imuDataGet(&stAngles, &stGyroRawData, &stAccelRawData, &stMagnRawData);
  pressSensorDataGet(&s32TemperatureVal, &s32PressureVal, &s32AltitudeVal);

  // Paint buffer
  paint.Clear(1); // 1 is UNCOLORED (white)
  
  char buf[32];
  paint.DrawStringAt(10, 10, "Payload Telemetry", &Font16, 0); // 0 is COLORED (black)
  
  snprintf(buf, sizeof(buf), "Temp: %.1fC", isnan(t) ? 0.0 : t);
  paint.DrawStringAt(10, 35, buf, &Font16, 0);
  
  snprintf(buf, sizeof(buf), "Alt: %.1f m", (float)s32AltitudeVal / 100.0);
  paint.DrawStringAt(10, 55, buf, &Font16, 0);

  snprintf(buf, sizeof(buf), "Pitch: %.1f", stAngles.fPitch);
  paint.DrawStringAt(10, 75, buf, &Font16, 0);
  
  snprintf(buf, sizeof(buf), "Roll: %.1f", stAngles.fRoll);
  paint.DrawStringAt(10, 95, buf, &Font16, 0);
  
  // Full screen update
  epd.SetFrameMemory(paint.GetImage(), 0, 0, EPD_WIDTH, EPD_HEIGHT);
  epd.DisplayFrame();
}

void logDataToSD(unsigned long timestamp) {
  // Read DHT Data
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  
  // Read IMU Data
  IMU_ST_ANGLES_DATA stAngles;
  IMU_ST_SENSOR_DATA stGyroRawData;
  IMU_ST_SENSOR_DATA stAccelRawData;
  IMU_ST_SENSOR_DATA stMagnRawData;
  int32_t s32PressureVal = 0, s32TemperatureVal = 0, s32AltitudeVal = 0;

  imuDataGet(&stAngles, &stGyroRawData, &stAccelRawData, &stMagnRawData);
  pressSensorDataGet(&s32TemperatureVal, &s32PressureVal, &s32AltitudeVal);

  File dataFile = SD.open("flight.csv", FILE_WRITE);
  if (dataFile) {
    dataFile.print(timestamp); dataFile.print(",");
    dataFile.print(isnan(t) ? 0.0 : t); dataFile.print(",");
    dataFile.print(isnan(h) ? 0.0 : h); dataFile.print(",");
    dataFile.print(stAngles.fRoll); dataFile.print(",");
    dataFile.print(stAngles.fPitch); dataFile.print(",");
    dataFile.print(stAngles.fYaw); dataFile.print(",");
    dataFile.print(stAccelRawData.s16X); dataFile.print(",");
    dataFile.print(stAccelRawData.s16Y); dataFile.print(",");
    dataFile.print(stAccelRawData.s16Z); dataFile.print(",");
    dataFile.print(stGyroRawData.s16X); dataFile.print(",");
    dataFile.print(stGyroRawData.s16Y); dataFile.print(",");
    dataFile.print(stGyroRawData.s16Z); dataFile.print(",");
    dataFile.print(stMagnRawData.s16X); dataFile.print(",");
    dataFile.print(stMagnRawData.s16Y); dataFile.print(",");
    dataFile.print(stMagnRawData.s16Z); dataFile.print(",");
    dataFile.print((float)s32PressureVal / 100); dataFile.print(",");
    dataFile.println((float)s32AltitudeVal / 100);
    
    dataFile.close();
    Serial.println("Data logged to SD");
  } else {
    Serial.println("Error opening flight.csv");
  }
}
