#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include <XPT2046_Touchscreen.h> // Touch screen library

// Define the pins for the touch screen controller
#define TOUCH_CS 21 // Chip select pin for touch screen
#define TOUCH_IRQ 36 // IRQ pin for touch screen

// Define TFT pins (typical for ESP32 with integrated display)
// You might need to adjust these based on your specific ESP32 board variant
// if it's not the standard "Cheap Yellow Display" (CYD)
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4 // Or -1 if not connected
#define TFT_BL   15 // Backlight control pin (adjust if different)

// Create TFT object
TFT_eSPI tft = TFT_eSPI();

// Create Touch Screen object
// Use the same SPI pins as the TFT (usually shared)
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ); // Chip select, IRQ pin

void setup(void) {
  Serial.begin(115200);
  Serial.println("Starting...");

  // Initialize TFT display
  tft.init();
  tft.setRotation(1); // Set screen orientation (0-3) - 1 is often landscape 320x240

  // Initialize Touch Screen
  ts.begin();
  ts.setRotation(1); // IMPORTANT: Match touch rotation to TFT rotation

  // Turn on the backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // HIGH usually turns backlight on

  // Draw the initial menu
  drawMainMenu();

  Serial.println("Setup complete. Waiting for touch.");
}

void loop() {
  // Check if the screen is currently being touched
  if (ts.touched()) {
    // Retrieve the touch point data
    TS_Point p = ts.getPoint();

    // --- THIS IS THE ADDED LOGIC ---
    // It's often necessary to map raw touch coordinates to screen coordinates.
    // However, with TFT_eSPI and XPT2046_Touchscreen, if setRotation() is
    // called correctly on both, the coordinates MIGHT already be mapped.
    // Let's assume p.x and p.y are screen coordinates for now.
    // Display size is typically 320x240 in rotation 1.
    // Check your specific display if buttons seem offset.

    int screenX = p.x;
    int screenY = p.y;

    // Print coordinates for debugging (optional)
    Serial.printf("Touch detected at Screen X: %d, Screen Y: %d (Raw: X=%d, Y=%d, Z=%d)\n", screenX, screenY, p.x, p.y, p.z);

    // Check if the touch coordinates fall within any button bounds
    // Button 1: x=10..110, y=10..110
    if ((screenX >= 10 && screenX <= 110) && (screenY >= 10 && screenY <= 110)) {
      buttonAction(1);
      // Simple debounce: Wait briefly after detecting a press in a button area
      // to avoid multiple triggers from a single touch.
      delay(200); 
    }
    // Button 2: x=120..220, y=10..110
    else if ((screenX >= 120 && screenX <= 220) && (screenY >= 10 && screenY <= 110)) {
      buttonAction(2);
      delay(200);
    }
    // Button 3: x=10..110, y=120..220
    else if ((screenX >= 10 && screenX <= 110) && (screenY >= 120 && screenY <= 220)) {
      buttonAction(3);
      delay(200);
    }
    // Button 4: x=120..220, y=120..220
    else if ((screenX >= 120 && screenX <= 220) && (screenY >= 120 && screenY <= 220)) {
      buttonAction(4);
      delay(200);
    }
    // --- END OF ADDED LOGIC ---

    /* Original code - can be removed or kept for debugging raw values
    Serial.print("Raw X = "); Serial.print(p.x);
    Serial.print("\tRaw Y = "); Serial.print(p.y);
    Serial.print("\tPressure = "); Serial.println(p.z);
    */

    // It's good practice to wait until the touch is released before processing
    // the next touch, but the delay above provides basic debouncing for now.
    // A more robust method checks `while(ts.touched()) { delay(10); }` after
    // detecting the initial press.

  } // End of if(ts.touched())

  // Small delay to prevent loop() from running too fast when not touched
  delay(50); 
}

void drawMainMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(2);

  // Draw Button 1
  tft.fillRect(10, 10, 100, 100, TFT_BLUE);
  tft.drawCentreString("1", 10 + 50, 10 + 40, 4); // x, y, font_number

  // Draw Button 2
  tft.fillRect(120, 10, 100, 100, TFT_RED);
  tft.drawCentreString("2", 120 + 50, 10 + 40, 4);

  // Draw Button 3
  tft.fillRect(10, 120, 100, 100, TFT_GREEN);
  tft.drawCentreString("3", 10 + 50, 120 + 40, 4);

  // Draw Button 4
  tft.fillRect(120, 120, 100, 100, TFT_YELLOW);
  tft.setTextColor(TFT_BLACK, TFT_YELLOW); // Text color that contrasts yellow
  tft.drawCentreString("4", 120 + 50, 120 + 40, 4);
}

void buttonAction(int buttonNumber) {
  Serial.print("Button ");
  Serial.print(buttonNumber);
  Serial.println(" pressed!");

  // Add actions for each button here
  switch (buttonNumber) {
    case 1:
      // Action for button 1
      tft.fillScreen(TFT_BLUE);
      tft.setTextColor(TFT_WHITE);
      tft.drawCentreString("Button 1 Action", tft.width()/2, tft.height()/2, 4);
      delay(2000); // Show message for 2 seconds
      drawMainMenu(); // Go back to main menu
      break;
    case 2:
      // Action for button 2
      tft.fillScreen(TFT_RED);
      tft.setTextColor(TFT_WHITE);
      tft.drawCentreString("Button 2 Action", tft.width()/2, tft.height()/2, 4);
      delay(2000);
      drawMainMenu();
      break;
    case 3:
      // Action for button 3
       tft.fillScreen(TFT_GREEN);
      tft.setTextColor(TFT_BLACK);
      tft.drawCentreString("Button 3 Action", tft.width()/2, tft.height()/2, 4);
      delay(2000);
      drawMainMenu();
      break;
    case 4:
      // Action for button 4
      tft.fillScreen(TFT_YELLOW);
      tft.setTextColor(TFT_BLACK);
      tft.drawCentreString("Button 4 Action", tft.width()/2, tft.height()/2, 4);
      delay(2000);
      drawMainMenu();
      break;
  }
}
