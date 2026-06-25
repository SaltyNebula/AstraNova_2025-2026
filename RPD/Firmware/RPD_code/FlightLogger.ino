#include "Waveshare_10Dof-D.h"
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "DHT.h"
#include "epd2in9_V2.h"
#include "epdpaint.h"

extern "C" {
  typedef struct {   
    uint16_t T1; int16_t T2; int16_t T3;
    uint16_t P1; int16_t P2; int16_t P3;
    int16_t P4; int16_t P5; int16_t P6;
    int16_t P7; int16_t P8; int16_t P9;
    int32_t T_fine;
  } BMP280_HandleTypeDef;
  
  extern BMP280_HandleTypeDef bmp280;
  uint8_t I2C_ReadOneByte(uint8_t DevAddr, uint8_t RegAddr);
  void I2C_WriteOneByte(uint8_t DevAddr, uint8_t RegAddr, uint8_t value);
}

// --- Configuration ---
const float LAUNCH_G = 3.0;
const unsigned long LAUNCH_DEBOUNCE_MS = 150;
const unsigned long APOGEE_CONFIRM_MS = 500;          // altitude decreasing this long = apogee passed
const float MIN_LAND_ALT_FT = 330.0;                  // landing only checked below this AGL
const float ALT_STABLE_BAND_FT = 10.0;                // altitude considered flat if change < this over the window
const unsigned long LANDED_CONFIRM_S = 5;
const unsigned long TIME_FAILSAFE_S = 180;            // last-resort backstop; ~2x real flight time
const float ACCEL_1G_RAW = 2048.0;                    // In 16g scale, 1g of force = 2048 raw value.

// --- Pins ---
const int BTN_0 = 0;
const int BTN_1 = 1;
const int LED_PIN = 28;
const int BUZZER_PIN = 7;
const int SD_DETECT_PIN = 20;

// DHT22 Sensor
#define DHTPIN 8
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// e-Paper Display
unsigned char image[(EPD_WIDTH * EPD_HEIGHT) / 8];
Paint paint(image, EPD_WIDTH, EPD_HEIGHT);
Epd epd;

// SD Card (SPI0)
const int SD_CS = 17;

// State Machine
enum FlightState {
  STATE_IDLE,
  STATE_ARMED,
  STATE_ASCENT,
  STATE_DESCENT,
  STATE_LANDED
};

FlightState currentState = STATE_IDLE;
unsigned long launchTime = 0;
File dataFile;

bool lastBtn0State = LOW;
bool lastBtn1State = LOW;

// Telemetry Variables
float currentAltitude = 0.0;
float smoothedAltitude = 0.0;
float maxAltitude = -99999.0;
float groundAltitudeOffset = 0.0;
float currentTemp = 0.0;
float currentHumidity = 0.0;
float imuTemp = 0.0;
bool sdCardPresent = false;
bool displayNeedsUpdate = false;
float displayAltitudeOffset = 0.0;
unsigned long stateConditionStartTime = 0;
float windowMinAlt = 99999.0;
float windowMaxAlt = -99999.0;

// (Removed base altitude variables)

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("--- Flight Logger ---");

  // 1. Setup Buttons, LED & Buzzer
  pinMode(BTN_0, INPUT);
  pinMode(BTN_1, INPUT);
  pinMode(SD_DETECT_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Beep when plugged in
  tone(BUZZER_PIN, 2000, 500); 

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
    Serial.println("IMU Initialized. Configuring to 16G full scale...");
    
    // Switch to User Bank 2
    I2C_WriteOneByte(0x68, 0x7F, 0x20);
    // Write REG_ADD_ACCEL_CONFIG (0x14)
    // 0x30 (DLPCFG_6) | 0x06 (FS_16g) | 0x01 (DLPF) = 0x37
    I2C_WriteOneByte(0x68, 0x14, 0x37);
    // Switch back to User Bank 0
    I2C_WriteOneByte(0x68, 0x7F, 0x00);

    Serial.println("Sensor warm-up complete.");
    // Warm up the filters by reading 50 times
    for (int i = 0; i < 50; i++) {
      int32_t pt, pp, pa;
      pressSensorDataGet(&pt, &pp, &pa);
      delay(20); 
    }
    Serial.println("Sensor warm-up complete.");
  }

  // 3. Setup SD Card (SPI0)
  if (digitalRead(SD_DETECT_PIN) == HIGH) {
    Serial.println("SD Card not inserted!");
    sdCardPresent = false;
    // Beep 3 times
    for (int i=0; i<3; i++) {
      tone(BUZZER_PIN, 1000, 200);
      delay(300);
    }
  } else {
    if (!SD.begin(SD_CS)) {
      Serial.println("SD Card initialization failed!");
      sdCardPresent = false;
      // Beep 3 times
      for (int i=0; i<3; i++) {
        tone(BUZZER_PIN, 1000, 200);
        delay(300);
      }
    } else {
      Serial.println("SD Card initialized.");
      sdCardPresent = true;
      
      // Write the STARTUP base values to the SD card immediately
      dataFile = SD.open("flightlog.csv", FILE_WRITE);
      if (dataFile) {
        dataFile.println("TimeMs,AccelX,AccelY,AccelZ,GyroX,GyroY,GyroZ,DHTTempC,DHTHum,IMUTempC,AltitudeM,Event");
        
        // Grab a quick initial reading
        IMU_ST_ANGLES_DATA stAngles;
        IMU_ST_SENSOR_DATA stGyroRawData;
        IMU_ST_SENSOR_DATA stAccelRawData;
        IMU_ST_SENSOR_DATA stMagnRawData;
        imuDataGet(&stAngles, &stGyroRawData, &stAccelRawData, &stMagnRawData);
        
        // Use our freshly implemented Bosch math for initial altitude
        float initialPressurePa = 101325.0;
        uint8_t xlsb = I2C_ReadOneByte(BMP280_ADDR, 0xF9);
        uint8_t lsb = I2C_ReadOneByte(BMP280_ADDR, 0xF8);
        uint8_t msb = I2C_ReadOneByte(BMP280_ADDR, 0xF7);
        int32_t adc_P = (msb << 12) | (lsb << 4) | (xlsb >> 4);
        int64_t var1, var2, p;
        var1 = ((int64_t)bmp280.T_fine) - 128000;
        var2 = var1 * var1 * (int64_t)bmp280.P6;
        var2 = var2 + ((var1*(int64_t)bmp280.P5)<<17);
        var2 = var2 + (((int64_t)bmp280.P4)<<35);
        var1 = ((var1 * var1 * (int64_t)bmp280.P3)>>8) + ((var1 * (int64_t)bmp280.P2)<<12);
        var1 = (((((int64_t)1)<<47)+var1))*((int64_t)bmp280.P1)>>33;
        if (var1 != 0) {
          p = 1048576 - adc_P;
          p = (((p<<31) - var2)*3125) / var1;
          var1 = (((int64_t)bmp280.P9) * (p>>13) * (p>>13)) >> 25;
          var2 = (((int64_t)bmp280.P8) * p) >> 19;
          p = ((p + var1 + var2) >> 8) + (((int64_t)bmp280.P7)<<4);
          initialPressurePa = (float)p / 256.0;
        }
        currentAltitude = 44330.0 * (1.0 - pow(initialPressurePa / 101325.0, 0.1903));
        smoothedAltitude = currentAltitude;
        
        dataFile.print(millis()); dataFile.print(",");
        dataFile.print(stAccelRawData.s16X); dataFile.print(",");
        dataFile.print(stAccelRawData.s16Y); dataFile.print(",");
        dataFile.print(stAccelRawData.s16Z); dataFile.print(",");
        dataFile.print(stGyroRawData.s16X); dataFile.print(",");
        dataFile.print(stGyroRawData.s16Y); dataFile.print(",");
        dataFile.print(stGyroRawData.s16Z); dataFile.print(",");
        dataFile.print(currentTemp); dataFile.print(",");
        dataFile.print(currentHumidity); dataFile.print(",");
        dataFile.print(0.0); dataFile.print(","); // Imutemp initially 0
        dataFile.print(currentAltitude); dataFile.println(",STARTUP");
        dataFile.close();
      }
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
      displayNeedsUpdate = true; // Trigger first frame draw!
  }

  Serial.println("Ready! System is IDLE.");
}

void loop() {
  // Read Buttons
  bool currentBtn0State = digitalRead(BTN_0);
  bool currentBtn1State = digitalRead(BTN_1);

  // Read IMU
  IMU_ST_ANGLES_DATA stAngles;
  IMU_ST_SENSOR_DATA stGyroRawData;
  IMU_ST_SENSOR_DATA stAccelRawData;
  IMU_ST_SENSOR_DATA stMagnRawData;
  imuDataGet(&stAngles, &stGyroRawData, &stAccelRawData, &stMagnRawData);

  // Read BMP280 at 10Hz to prevent I2C collisions that cause garbled readings
  static unsigned long lastPressTime = 0;
  if (millis() - lastPressTime >= 100) {
    lastPressTime = millis();
    int32_t pressTemp, pressPressure, pressAltitude;
    pressSensorDataGet(&pressTemp, &pressPressure, &pressAltitude);

    imuTemp = pressTemp / 100.0; // BMP280 temperature is typically returned in 0.01 degrees C

    // BUGFIX: We bypass the Waveshare library's broken pressure math entirely
    // and implement the official Bosch 64-bit compensation formula using the raw I2C data.
    uint8_t xlsb = I2C_ReadOneByte(BMP280_ADDR, 0xF9);
    uint8_t lsb = I2C_ReadOneByte(BMP280_ADDR, 0xF8);
    uint8_t msb = I2C_ReadOneByte(BMP280_ADDR, 0xF7);
    int32_t adc_P = (msb << 12) | (lsb << 4) | (xlsb >> 4);

    int64_t var1, var2, p;
    var1 = ((int64_t)bmp280.T_fine) - 128000;
    var2 = var1 * var1 * (int64_t)bmp280.P6;
    var2 = var2 + ((var1*(int64_t)bmp280.P5)<<17);
    var2 = var2 + (((int64_t)bmp280.P4)<<35);
    var1 = ((var1 * var1 * (int64_t)bmp280.P3)>>8) + ((var1 * (int64_t)bmp280.P2)<<12);
    var1 = (((((int64_t)1)<<47)+var1))*((int64_t)bmp280.P1)>>33;

    float actualPressurePa = 101325.0; // Fallback
    if (var1 != 0) {
      p = 1048576 - adc_P;
      p = (((p<<31) - var2)*3125) / var1;
      var1 = (((int64_t)bmp280.P9) * (p>>13) * (p>>13)) >> 25;
      var2 = (((int64_t)bmp280.P8) * p) >> 19;
      p = ((p + var1 + var2) >> 8) + (((int64_t)bmp280.P7)<<4);
      actualPressurePa = (float)p / 256.0;
    }

    float newAltitude = 44330.0 * (1.0 - pow(actualPressurePa / 101325.0, 0.1903));
    
    // Outlier rejection: Ignore wildly impossible altitude readings
    if (newAltitude > -10000.0 && newAltitude < 100000.0) {
      currentAltitude = newAltitude;
      if (smoothedAltitude == 0.0) {
        smoothedAltitude = currentAltitude;
      } else {
        smoothedAltitude = smoothedAltitude * 0.8 + currentAltitude * 0.2;
      }
    }
  }

  // Calculate Accel Magnitude in Gs
  float accelMagG = sqrt(pow((float)stAccelRawData.s16X, 2) + pow((float)stAccelRawData.s16Y, 2) + pow((float)stAccelRawData.s16Z, 2)) / ACCEL_1G_RAW;
  float currentAglFt = (smoothedAltitude - groundAltitudeOffset) * 3.28084;
  
  if (currentState == STATE_ARMED || currentState == STATE_ASCENT || currentState == STATE_DESCENT) {
    // Continuous logging
    if (dataFile) {
      dataFile.print(millis()); dataFile.print(",");
      dataFile.print(stAccelRawData.s16X); dataFile.print(",");
      dataFile.print(stAccelRawData.s16Y); dataFile.print(",");
      dataFile.print(stAccelRawData.s16Z); dataFile.print(",");
      dataFile.print(stGyroRawData.s16X); dataFile.print(",");
      dataFile.print(stGyroRawData.s16Y); dataFile.print(",");
      dataFile.print(stGyroRawData.s16Z); dataFile.print(",");
      dataFile.print(currentTemp); dataFile.print(",");
      dataFile.print(currentHumidity); dataFile.print(",");
      dataFile.print(imuTemp); dataFile.print(",");
      dataFile.print(smoothedAltitude); dataFile.println(",LOG");
      
      static unsigned long lastFlush = 0;
      if (millis() - lastFlush > 1000) {
        dataFile.flush();
        lastFlush = millis();
      }
    }
  }

  // Failsafe check
  if ((currentState == STATE_ASCENT || currentState == STATE_DESCENT) && (millis() - launchTime > TIME_FAILSAFE_S * 1000)) {
    Serial.println("FAILSAFE: Time exceeded. Forcing LANDED state.");
    currentState = STATE_LANDED;
    if (dataFile) {
      dataFile.flush();
      dataFile.close();
    }
    digitalWrite(LED_PIN, LOW);
    tone(BUZZER_PIN, 2000, 1000);
    displayNeedsUpdate = true;
  }

  // State Machine Logic
  switch (currentState) {
    case STATE_IDLE:
      // Press Button 0 to ARM
      if (currentBtn0State == HIGH && lastBtn0State == LOW) {
        currentState = STATE_ARMED;
        Serial.println("System ARMED. Setting baseline and logging.");
        tone(BUZZER_PIN, 3000, 200); 
        
        groundAltitudeOffset = smoothedAltitude; 
        maxAltitude = 0.0;
        stateConditionStartTime = millis();

        dataFile = SD.open("flightlog.csv", FILE_WRITE);
        if (dataFile) {
          dataFile.println("TimeMs,AccelX,AccelY,AccelZ,GyroX,GyroY,GyroZ,DHTTempC,DHTHum,IMUTempC,AltitudeM,Event");
        }
        displayNeedsUpdate = true;
      }
      break;

    case STATE_ARMED:
      // Launch Detection
      if (accelMagG > LAUNCH_G) {
        if (millis() - stateConditionStartTime >= LAUNCH_DEBOUNCE_MS) {
          currentState = STATE_ASCENT;
          launchTime = millis();
          Serial.println("Launch detected! ASCENT.");
          tone(BUZZER_PIN, 4000, 1000); 
          digitalWrite(LED_PIN, HIGH);
          stateConditionStartTime = millis();
        }
      } else {
        stateConditionStartTime = millis(); // Reset debounce
      }
      break;

    case STATE_ASCENT:
      // Track Max AGL
      if (currentAglFt > maxAltitude) {
        maxAltitude = currentAglFt;
        stateConditionStartTime = millis(); // Reset apogee timer
      } else {
        // Apogee detection
        if (millis() - stateConditionStartTime >= APOGEE_CONFIRM_MS) {
          currentState = STATE_DESCENT;
          Serial.println("Apogee detected! DESCENT.");
          
          windowMinAlt = currentAglFt;
          windowMaxAlt = currentAglFt;
          stateConditionStartTime = millis(); 
        }
      }
      break;

    case STATE_DESCENT:
      // Landing Detection
      if (currentAglFt < windowMinAlt) windowMinAlt = currentAglFt;
      if (currentAglFt > windowMaxAlt) windowMaxAlt = currentAglFt;

      if (currentAglFt < MIN_LAND_ALT_FT && ((windowMaxAlt - windowMinAlt) < ALT_STABLE_BAND_FT) && (accelMagG > 0.8 && accelMagG < 1.2)) {
        if (millis() - stateConditionStartTime >= LANDED_CONFIRM_S * 1000) {
          currentState = STATE_LANDED;
          Serial.println("Landing detected! LANDED.");
          if (dataFile) {
            dataFile.flush();
            dataFile.close();
          }
          digitalWrite(LED_PIN, LOW);
          tone(BUZZER_PIN, 2000, 1000); 
          displayNeedsUpdate = true;
        }
      } else {
        // Reset landing window timer and bounds if conditions fail
        stateConditionStartTime = millis();
        windowMinAlt = currentAglFt;
        windowMaxAlt = currentAglFt;
      }
      break;

    case STATE_LANDED:
      // System is stopped. Max altitude is preserved.
      // Press Button 0 to reset back to IDLE
      if (currentBtn0State == HIGH && lastBtn0State == LOW) {
        currentState = STATE_IDLE;
        maxAltitude = -99999.0;
        Serial.println("System Reset to IDLE.");
        tone(BUZZER_PIN, 1000, 200);
        displayNeedsUpdate = true;
      }
      break;
  }

  // Relative Altitude Reset Logic (Button 1)
  if (currentBtn1State == HIGH && lastBtn1State == LOW) {
    displayAltitudeOffset = smoothedAltitude;
    Serial.println("Display altitude zeroed to relative!");
    tone(BUZZER_PIN, 2500, 100);
    displayNeedsUpdate = true;
  }

  lastBtn0State = currentBtn0State;
  lastBtn1State = currentBtn1State;

  // Read DHT22 slowly (Max 1 reading every 2 seconds)
  // To avoid blocking the fast recording loop too much, we could skip this during RECORDING,
  // but requirements say "must also write all... and dht22".
  static unsigned long lastDhtTime = 0;
  if (millis() - lastDhtTime >= 2000) {
    lastDhtTime = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) currentTemp = t;
    if (!isnan(h)) currentHumidity = h;
  }

  // Update e-Paper Display on demand
  if (displayNeedsUpdate) {
    displayNeedsUpdate = false;
    
    paint.Clear(1); // UNCOLORED (white)
    char buf[32];
    paint.DrawStringAt(10, 10, "Flight Logger", &Font16, 0); // COLORED (black)
    
    const char* stateStr = "IDLE";
    if (currentState == STATE_ARMED) stateStr = "ARMED";
    else if (currentState == STATE_ASCENT) stateStr = "ASCENT";
    else if (currentState == STATE_DESCENT) stateStr = "DESCENT";
    else if (currentState == STATE_LANDED) stateStr = "LANDED";
    
    snprintf(buf, sizeof(buf), "State: %s", stateStr);
    paint.DrawStringAt(10, 35, buf, &Font16, 0);
    
    if (currentState == STATE_IDLE) {
      float relativeAltFt = (smoothedAltitude - displayAltitudeOffset) * 3.28084;
      snprintf(buf, sizeof(buf), "Alt: %.1f ft", relativeAltFt);
      paint.DrawStringAt(10, 55, buf, &Font16, 0);
      snprintf(buf, sizeof(buf), "Max AGL: -- ft");
      paint.DrawStringAt(10, 75, buf, &Font16, 0);
    } else {
      float currentAglFt = (smoothedAltitude - groundAltitudeOffset) * 3.28084;
      snprintf(buf, sizeof(buf), "AGL: %.1f ft", currentAglFt);
      paint.DrawStringAt(10, 55, buf, &Font16, 0);
      snprintf(buf, sizeof(buf), "Max AGL: %.1fft", maxAltitude);
      paint.DrawStringAt(10, 75, buf, &Font16, 0);
    }
    
    if (!sdCardPresent) {
      paint.DrawStringAt(10, 95, "NO SD CARD!", &Font16, 0);
    } else {
      snprintf(buf, sizeof(buf), "Temp: %.1fC", currentTemp);
      paint.DrawStringAt(10, 95, buf, &Font16, 0);
    }
    
    epd.SetFrameMemory(paint.GetImage(), 0, 0, EPD_WIDTH, EPD_HEIGHT);
    epd.DisplayFrame();
  }
  
  // Optional: small delay to not overwhelm I2C if not recording
  if (currentState != STATE_RECORDING) {
    delay(10);
  }
}
