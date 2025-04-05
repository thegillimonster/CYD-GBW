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

const int btnW = 145;
const int btnH = 105;
const int btnX1 = 10;
const int btnY1 = 10;
const int btnX2 = 165;
const int btnY2 = 125;

const char* btn1Label = "Espresso";
const char* btn2Label = "Drip";
const char* btn3Label = "Calibrate";
const char* btn4Label = "On/Off";

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
 if ((screenX >= btnX1 && screenX <= btnX1 + btnW -1) && (screenY >= btnY1 && screenY <= btnY1 + btnH -1)) {
      buttonAction(1); // Espresso
      delay(200); // Debounce
    }
    // Check Button 2 (Drip: X=165..309, Y=10..114)
    else if ((screenX >= btnX2 && screenX <= btnX2 + btnW -1) && (screenY >= btnY1 && screenY <= btnY1 + btnH -1)) {
      buttonAction(2); // Drip
      delay(200); // Debounce
    }
    // Check Button 3 (Calibrate: X=10..154, Y=125..229)
    else if ((screenX >= btnX1 && screenX <= btnX1 + btnW -1) && (screenY >= btnY2 && screenY <= btnY2 + btnH -1)) {
      buttonAction(3); // Calibrate
      delay(200); // Debounce
    }
    // Check Button 4 (On/Off: X=165..309, Y=125..229)
    else if ((screenX >= btnX2 && screenX <= btnX2 + btnW -1) && (screenY >= btnY2 && screenY <= btnY2 + btnH -1)) {
      buttonAction(4); // On/Off
      delay(200); // Debounce
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

void drawCenteredText(const char* text, int x, int y, int w, int h) {
    int16_t textX, textY; // To store top-left coord of text
    uint16_t textW, textH; // To store text width & height
    tft.setTextDatum(MC_DATUM); // Set datum to Middle Center
    // The getTextBounds function is useful but requires more setup.
    // drawString centers text based on the datum automatically at the given x,y
    // Let's calculate the center of the button area
    int centerX = x + w / 2;
    int centerY = y + h / 2;
    tft.drawString(text, centerX, centerY);
}

void drawMainMenu() {
  // Set background to white
  tft.fillScreen(TFT_WHITE);

  // Set text properties for buttons
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(2); // Adjust text size if needed (1, 2, 3...)
                      // Note: drawCenteredText helper uses drawString which respects setTextSize
                      // If using built-in fonts (e.g., with drawCentreString), use font number instead.

  // Draw Button 1 (Espresso)
  tft.fillRect(btnX1, btnY1, btnW, btnH, TFT_YELLOW); // Yellow button background
  tft.drawRect(btnX1, btnY1, btnW, btnH, TFT_BLACK); // Optional: black border
  drawCenteredText(btn1Label, btnX1, btnY1, btnW, btnH);

  // Draw Button 2 (Drip)
  tft.fillRect(btnX2, btnY1, btnW, btnH, TFT_YELLOW);
  tft.drawRect(btnX2, btnY1, btnW, btnH, TFT_BLACK); // Optional border
  drawCenteredText(btn2Label, btnX2, btnY1, btnW, btnH);

  // Draw Button 3 (Calibrate)
  tft.fillRect(btnX1, btnY2, btnW, btnH, TFT_YELLOW);
  tft.drawRect(btnX1, btnY2, btnW, btnH, TFT_BLACK); // Optional border
  drawCenteredText(btn3Label, btnX1, btnY2, btnW, btnH);

  // Draw Button 4 (On/Off)
  tft.fillRect(btnX2, btnY2, btnW, btnH, TFT_YELLOW);
  tft.drawRect(btnX2, btnY2, btnW, btnH, TFT_BLACK); // Optional border
  drawCenteredText(btn4Label, btnX2, btnY2, btnW, btnH);
}

void buttonAction(int buttonNumber) {
  const char* actionLabel = "";
  uint16_t bgColor = TFT_BLACK; // Default background for action screen

  switch (buttonNumber) {
    case 1:
      actionLabel = btn1Label; // Espresso
      bgColor = TFT_BROWN; // Example color
      Serial.printf("Button Action: %s\n", actionLabel);
      // Add action for Espresso here
      break;
    case 2:
      actionLabel = btn2Label; // Drip
      bgColor = TFT_DARKGREY; // Example color
      Serial.printf("Button Action: %s\n", actionLabel);
      // Add action for Drip here
      break;
    case 3:
      actionLabel = btn3Label; // Calibrate
      bgColor = TFT_ORANGE; // Example color
      Serial.printf("Button Action: %s\n", actionLabel);
      // Add action for Calibrate here (e.g., run touch calibration routine)
      break;
    case 4:
      actionLabel = btn4Label; // On/Off
      bgColor = TFT_RED; // Example color
      Serial.printf("Button Action: %s\n", actionLabel);
      // Add action for On/Off here (e.g., toggle state, go to sleep)
      break;
    default: // Should not happen
       Serial.printf("Unknown Button Action: %d\n", buttonNumber);
       return; // Exit if button number is unexpected
  }
  // --- Display Action Feedback ---
  tft.fillScreen(bgColor);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3); // Larger text for feedback
  tft.setTextDatum(MC_DATUM); // Middle Center
  tft.drawString(actionLabel, tft.width() / 2, tft.height() / 2);

  delay(1500); // Show feedback for 1.5 seconds
  drawMainMenu(); // Return to main menu

  // Clear any lingering touch after action
  while (ts.touched()) { delay(10); }
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
