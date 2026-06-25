#include <SD.h>
#include <SPI.h>

// On the Raspberry Pi Pico / Pico 2, standard SPI CS (SS) is usually 17.
// On standard Arduino Uno, it is usually 10.
// On Arduino Mega, it is usually 53.
// Please change this to match the pin connected to the CS (Chip Select) pin of
// your SD card module.
const int chipSelect = 17;

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);

  // Wait a little bit for the Serial Monitor to be opened
  delay(2000);
  Serial.println("Initializing SD card...");

  // Configure SPI pins for SD Card (MISO: 20, MOSI: 19, SCK: 18, CS: 17)
  SPI.setRX(20);
  SPI.setTX(19);
  SPI.setSCK(18);

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    while (1)
      ;
  }
  Serial.println("card initialized.");

  // Create a new CSV file. FILE_WRITE will create the file if it doesn't exist,
  // or append to it if it does.
  // Note: File names should be short (8.3 format) for FAT16/FAT32
  // compatibility.
  File dataFile = SD.open("datalog.csv", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    Serial.println("Writing to datalog.csv...");

    // Write the CSV Header
    dataFile.println("Time(ms),Sensor1,Sensor2,Sensor3");

    // Write 10 rows of mock data
    for (int i = 0; i < 10; i++) {
      unsigned long timeStamp = millis();
      int sensor1 = random(0, 100);
      int sensor2 = random(0, 100);
      int sensor3 = random(0, 100);

      // Write data separated by commas
      dataFile.print(timeStamp);
      dataFile.print(",");
      dataFile.print(sensor1);
      dataFile.print(",");
      dataFile.print(sensor2);
      dataFile.print(",");
      dataFile.println(sensor3); // println on the last one to add a new line

      delay(100); // Small delay between readings
    }

    // Close the file to save the data to the SD card
    dataFile.close();
    Serial.println("Finished writing CSV data.");
    Serial.println("You can now remove the SD card and read 'datalog.csv' on "
                   "your laptop.");
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.csv");
  }
}

void loop() {
  // Nothing to do here, we just run the test once in setup
}
