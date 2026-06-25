#include "Waveshare_10Dof-D.h"
#include <Wire.h>

extern "C" {
  typedef struct {   
    uint16_t T1; int16_t T2; int16_t T3;
    uint16_t P1; int16_t P2; int16_t P3;
    int16_t P4; int16_t P5; int16_t P6;
    int16_t P7; int16_t P8; int16_t P9;
    int32_t T_fine;
  } BMP280_HandleTypeDef;
  
  extern BMP280_HandleTypeDef bmp280;
  void bmp280TandPGet(float *temperature, float *pressure);
  uint8_t I2C_ReadOneByte(uint8_t DevAddr, uint8_t RegAddr);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("--- Ultimate Altitude Test ---");

  Wire1.setSDA(26);
  Wire1.setSCL(27);

  IMU_EN_SENSOR_TYPE enMotionSensorType, enPressureType;
  imuInit(&enMotionSensorType, &enPressureType);
  delay(1000); 
}

void loop() {
  // 1. Call the library to update T_fine internally
  float dummyT = 0.0, dummyP = 0.0;
  bmp280TandPGet(&dummyT, &dummyP);
  
  // 2. Read the raw ADC values manually
  uint8_t xlsb = I2C_ReadOneByte(BMP280_ADDR, 0xF9);
  uint8_t lsb = I2C_ReadOneByte(BMP280_ADDR, 0xF8);
  uint8_t msb = I2C_ReadOneByte(BMP280_ADDR, 0xF7);
  
  int32_t adc_P = msb;
  adc_P <<= 8;
  adc_P |= lsb;
  adc_P <<= 8;
  adc_P |= xlsb;
  adc_P >>= 4;

  // 3. Implement the EXACT, OFFICIAL Bosch 64-bit integer formula
  int64_t var1, var2, p;
  var1 = ((int64_t)bmp280.T_fine) - 128000;
  var2 = var1 * var1 * (int64_t)bmp280.P6;
  var2 = var2 + ((var1*(int64_t)bmp280.P5)<<17);
  var2 = var2 + (((int64_t)bmp280.P4)<<35);
  var1 = ((var1 * var1 * (int64_t)bmp280.P3)>>8) + ((var1 * (int64_t)bmp280.P2)<<12);
  var1 = (((((int64_t)1)<<47)+var1))*((int64_t)bmp280.P1)>>33;

  float actualPressurePa = 0;
  if (var1 != 0) {
    p = 1048576 - adc_P;
    p = (((p<<31) - var2)*3125) / var1;
    var1 = (((int64_t)bmp280.P9) * (p>>13) * (p>>13)) >> 25;
    var2 = (((int64_t)bmp280.P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)bmp280.P7)<<4);
    actualPressurePa = (float)p / 256.0;
  }

  Serial.println("-------------------------------------");
  Serial.print("Raw adc_P from I2C: ");
  Serial.println(adc_P);
  Serial.print("TRUE Calculated Pressure (Pa): ");
  Serial.println(actualPressurePa);

  float alt = 44330.0 * (1.0 - pow(actualPressurePa / 101325.0, 0.1903));
  Serial.print("TRUE Absolute Altitude (m): ");
  Serial.println(alt);
  
  delay(1000);
}
