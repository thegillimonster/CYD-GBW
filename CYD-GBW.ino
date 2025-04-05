#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>
// #include <HX711.h>
// #include <SimpleKalmanFilter.h>

#define TOUCH_X_MIN 500  // Raw minimum X value (touch top-left corner)
#define TOUCH_X_MAX 3700 // Raw maximum X value (touch bottom-right corner)
#define TOUCH_Y_MIN 400  // Raw minimum Y value (touch top-left corner)
#define TOUCH_Y_MAX 3700 // Raw maximum Y value (touch bottom-right corner)

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
// const int LOADCELL_DOUT_PIN = 3;
// const int LOADCELL_SCK_PIN = 2;

// Create HX711 and kalmna filter object
// HX711 scale;
// SimpleKalmanFilter kf(0.02, 0.02, 0.01); // Adjust parameters as needed

// // Calibration factor for the load cell
// float calibration_factor = -7050; // Initial calibration factor


SPIClass mySpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);

  // Start the SPI for the touch screen and init the TS library
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);
  ts.setRotation(1);

  // Start the tft display and set it to black
  tft.init();
  tft.setRotation(1); //This is the display in landscape

  // Clear the screen before writing to it
  tft.fillScreen(TFT_BLACK);

  int x = 320 / 2; // center of display
  int y = 100;
  int fontSize = 2;

  // Display main menu
  drawMainMenu();

}

void loop(){

if (ts.tirqTouched() && ts.touched()) {
    // Retrieve the touch point data
    TS_Point p = ts.getPoint();

    // --- THIS IS THE ADDED LOGIC ---
    // It's often necessary to map raw touch coordinates to screen coordinates.
    // However, with TFT_eSPI and XPT2046_Touchscreen, if setRotation() is
    // called correctly on both, the coordinates MIGHT already be mapped.
    // Let's assume p.x and p.y are screen coordinates for now.
    // Display size is typically 320x240 in rotation 1.
    // Check your specific display if buttons seem offset.

    int screenX = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, tft.width());
    int screenY = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, tft.height());

    screenX = constrain(screenX, 0, tft.width() -1);
    screenY = constrain(screenY, 0, tft.height() -1);

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
  }
}

void printTouchToSerial(TS_Point p) {
  Serial.print("Pressure = ");
  Serial.print(p.z);
  Serial.print(", x = ");
  Serial.print(p.x);
  Serial.print(", y = ");
  Serial.print(p.y);
  Serial.println();
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

//   // Read raw value from load cell
//   float raw_weight = scale.get_units(10);

//   // Apply Kalman filter to raw weight
//   float filtered_weight = kf.updateEstimate(raw_weight);

//   // Print the filtered weight
//   Serial.print("Filtered Weight: ");
//   Serial.println(filtered_weight);

//   delay(100);

// void grind(float amount) {
//   tft.fillScreen(TFT_WHITE);
//   tft.setTextColor(TFT_BLACK);
//   tft.setTextSize(3);
//   tft.setCursor(10, 10);
//   tft.print("Grinding: ");
//   tft.print(amount);
//   tft.print("g");

//   unsigned long startTime = millis();
//   while (millis() - startTime < 3000) {
//     if (ts.touched()) {
//       displayMainMenu();
//       return;
//     }
//   }

//   displayMainMenu();
// }

// void calibrate() {
//   Serial.println("Calibrating...");

//   // Read raw value from load cell
//   float raw_weight = scale.get_units(10);

//   // Known weight for calibration
//   float known_weight = 100.0; // 100g weight

//   // Calculate new calibration factor
//   calibration_factor = raw_weight / known_weight;
//   scale.set_scale(calibration_factor);

//   Serial.print("New calibration factor: ");
//   Serial.println(calibration_factor);
//   Serial.println("Calibration complete.");
// }
