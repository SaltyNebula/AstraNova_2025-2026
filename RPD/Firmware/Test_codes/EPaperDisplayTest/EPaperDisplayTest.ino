#include "epd2in9_V2.h"
#include "epdpaint.h"
#include <SPI.h>

// Define colors
#define COLORED 0
#define UNCOLORED 1

// Full screen buffer for 296x128 resolution
// (296 * 128 / 8 = 4736 bytes) - Pico 2 has plenty of RAM for a full buffer
unsigned char image[(EPD_WIDTH * EPD_HEIGHT) / 8];
Paint paint(image, EPD_WIDTH, EPD_HEIGHT);
Epd epd;

void setup() {
  Serial.begin(115200);

  // Added a short delay so you can open Serial Monitor after uploading
  delay(2000);
  Serial.println("Pico 2 e-Paper Test starting...");

  // Initialize the e-paper display
  if (epd.Init() != 0) {
    Serial.println("e-Paper init failed! Check your wiring.");
    return;
  }

  // Clear display hardware to white
  epd.ClearFrameMemory(0xFF);
  epd.DisplayFrame();

  // Set up paint object for drawing in landscape mode
  paint.SetRotate(ROTATE_90); // Landscape
  paint.Clear(UNCOLORED);     // White background

  // Draw some static text (arguments are X, Y, string, font, color)
  paint.DrawStringAt(10, 20, "Pico 2 Display Test", &Font16, COLORED);
  paint.DrawStringAt(10, 50, "Sensor reading:", &Font16, COLORED);

  // Draw a large number to simulate a sensor reading
  paint.DrawStringAt(40, 80, "42.5", &Font24, COLORED);

  // Send our drawn image buffer to the display
  // Note: we always pass the native width and height to SetFrameMemory
  epd.SetFrameMemory(image, 0, 0, EPD_WIDTH, EPD_HEIGHT);
  epd.DisplayFrame();

  // Send the display to sleep to save power and prevent damage
  Serial.println("Display updated. Putting e-Paper to sleep.");
  epd.Sleep();
}

void loop() {
  // We only run this once in setup() to save the e-paper's lifespan
  // Nothing to do in the loop
}
