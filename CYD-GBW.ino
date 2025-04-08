#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>
#include <stdio.h> // For sprintf
#include <stdlib.h> // For dtostrf (float to string)
#include <HX711.h>
#include <SimpleKalmanFilter.h>

#define TOUCH_X_MIN 500  // Raw minimum X value (touch top-left corner)
#define TOUCH_X_MAX 3700 // Raw maximum X value (touch bottom-right corner)
#define TOUCH_Y_MIN 400  // Raw minimum Y value (touch top-left corner)
#define TOUCH_Y_MAX 3700 // Raw maximum Y value (touch bottom-right corner)

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// --- Main Menu Layout ---
const int mainBtnW = 145;
const int mainBtnH = 105;
const int mainBtnX1 = 10;
const int mainBtnY1 = 10;
const int mainBtnX2 = 165;
const int mainBtnY2 = 125;
const char* mainBtn1Label = "Espresso";
const char* mainBtn2Label = "Drip";
const char* mainBtn3Label = "Calibrate";
const char* mainBtn4Label = "On/Off";

// --- Espresso Menu Layout (Same 2x2 structure) ---
const int espBtnW = 145;
const int espBtnH = 105;
const int espBtnX1 = 10;
const int espBtnY1 = 10;
const int espBtnX2 = 165;
const int espBtnY2 = 125;
const char* espBtn1Label = "18g";
const char* espBtn2Label = "18.5g";
const char* espBtn3Label = "19g";
const char* espBtn4Label = "19.5g";

// --- Drip Menu Layout ---
const int dripHeaderH = 40;
const int dripBtnCols = 5;
const int dripBtnRows = 2;
const int dripBtnW = 52; // (320 - 6 * 10) / 5
const int dripBtnH = 85; // (240 - 10 - dripHeaderH - 10 - 10) / 2
const int dripBtnSpacing = 10;
const int dripBtnStartY = 10 + dripHeaderH + dripBtnSpacing; // Y pos of first row
int dripCupValues[10] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 12}; // Values for the 10 buttons

// Define screen states
enum ScreenState {
  MAIN_MENU,
  ESPRESSO_MENU,
  DRIP_MENU,
  ON_OFF_SCREEN
};
ScreenState currentScreen = MAIN_MENU;

// State variable for On/Off screen
bool isOnState = true; // true = Blue (On?), false = Red (Off?)

void drawMainMenuScreen();
void drawEspressoMenuScreen();
void drawDripMenuScreen();
void drawOnOffScreen();
void handleMainMenuTouch(int x, int y);
void handleEspressoMenuTouch(int x, int y);
void handleDripMenuTouch(int x, int y);
void handleOnOffTouch(int x, int y);
void displayTemporaryMessage(const char* msg, uint16_t duration);
void drawStyledButton(const char* label, int x, int y, int w, int h);

const int LOADCELL_DOUT_PIN = 22;
const int LOADCELL_SCK_PIN = 27;

Create HX711 and kalmna filter object
HX711 scale;
SimpleKalmanFilter kf(0.02, 0.02, 0.01); // Adjust parameters as needed

// Calibration factor for the load cell
float calibration_factor = -7050; // Initial calibration factor


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
  drawMainMenuScreen();

}

void loop(){

static bool touchWasActive = false;
  static unsigned long touchStartTime = 0;
  static int touchStartX = -1, touchStartY = -1;

  bool touchIsActive = ts.touched();

  if (touchIsActive && !touchWasActive) {
    // Touch DOWN event
    touchWasActive = true;
    touchStartTime = millis();
    TS_Point p = ts.getPoint(); // Get point ONCE on initial press
    int screenX = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, tft.width());
    int screenY = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, tft.height());
    touchStartX = constrain(screenX, 0, tft.width() - 1);
    touchStartY = constrain(screenY, 0, tft.height() - 1);
    // Serial.printf("Touch Down at (%d, %d)\n", touchStartX, touchStartY); // Debug

  } else if (touchIsActive && touchWasActive) {
    // Touch HOLD event
    // Check for long press timeout ONLY on submenus/on-off screen
    if (currentScreen != MAIN_MENU && millis() - touchStartTime > 3000) {
      Serial.println("Long press timeout, returning to main menu.");
      currentScreen = MAIN_MENU;
      drawMainMenuScreen();
      touchWasActive = false; // Reset touch state, action handled by timeout
      // Consume any remaining touch data until release
      while(ts.touched()) { delay(20); }
    }

  } else if (!touchIsActive && touchWasActive) {
    // Touch UP event (released normally before timeout)
    touchWasActive = false;
    // Serial.printf("Touch Up at (%d, %d)\n", touchStartX, touchStartY); // Debug
    // Process the tap based on the screen state and touchStartX/Y
    switch (currentScreen) {
        case MAIN_MENU:
          handleMainMenuTouch(touchStartX, touchStartY);
          break;
        case ESPRESSO_MENU:
          handleEspressoMenuTouch(touchStartX, touchStartY);
          break;
        case DRIP_MENU:
          handleDripMenuTouch(touchStartX, touchStartY);
          break;
        case ON_OFF_SCREEN:
          handleOnOffTouch(touchStartX, touchStartY);
          break;
    }
    // Reset touch start coords after processing
    touchStartX = -1;
    touchStartY = -1;

  } else {
    // No touch active
  }

  delay(20); // Main loop delay
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

// Helper function to draw our standard button style
void drawStyledButton(const char* label, int x, int y, int w, int h) {
    tft.fillRect(x, y, w, h, TFT_YELLOW);
    tft.drawRect(x, y, w, h, TFT_BLACK); // Optional border
    // Use Middle Center Datum for easy text centering with drawString
    tft.setTextColor(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    // Adjust text size based on button size or content? For now, keep fixed size.
    tft.setTextSize(2);
    tft.drawString(label, x + w / 2, y + h / 2);
}

// Helper function for temporary messages
void displayTemporaryMessage(const char* msg, uint16_t duration) {
  tft.fillScreen(TFT_DARKCYAN); // Or another feedback color
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM); // Middle Center
  tft.drawString(msg, tft.width() / 2, tft.height() / 2);
  delay(duration);
  // The caller function MUST handle redrawing the correct screen afterwards
}

// --- Drawing Functions ---

void drawMainMenuScreen() {
  tft.fillScreen(TFT_WHITE);
  drawStyledButton(mainBtn1Label, mainBtnX1, mainBtnY1, mainBtnW, mainBtnH); // Espresso
  drawStyledButton(mainBtn2Label, mainBtnX2, mainBtnY1, mainBtnW, mainBtnH); // Drip
  drawStyledButton(mainBtn3Label, mainBtnX1, mainBtnY2, mainBtnW, mainBtnH); // Calibrate
  drawStyledButton(mainBtn4Label, mainBtnX2, mainBtnY2, mainBtnW, mainBtnH); // On/Off
}

void drawEspressoMenuScreen() {
  tft.fillScreen(TFT_WHITE); // Keep background consistent
  drawStyledButton(espBtn1Label, espBtnX1, espBtnY1, espBtnW, espBtnH); // 18g
  drawStyledButton(espBtn2Label, espBtnX2, espBtnY1, espBtnW, espBtnH); // 18.5g
  drawStyledButton(espBtn3Label, espBtnX1, espBtnY2, espBtnW, espBtnH); // 19g
  drawStyledButton(espBtn4Label, espBtnX2, espBtnY2, espBtnW, espBtnH); // 19.5g
}

void drawDripMenuScreen() {
  tft.fillScreen(TFT_WHITE);
  // Draw Header
  tft.setTextColor(TFT_BLACK);
  tft.setTextDatum(TC_DATUM); // Top Center
  tft.setTextSize(3); // Larger size for header
  tft.drawString("Cups", tft.width() / 2, 10); // Draw header text

  // Draw 10 buttons (2 rows of 5)
  tft.setTextSize(2); // Reset text size for buttons
  for (int i = 0; i < 10; ++i) {
    int row = i / dripBtnCols; // 0 or 1
    int col = i % dripBtnCols; // 0 to 4
    int btnX = dripBtnSpacing + col * (dripBtnW + dripBtnSpacing);
    int btnY = dripBtnStartY + row * (dripBtnH + dripBtnSpacing);
    char cupLabel[4]; // Enough for "12" + null
    sprintf(cupLabel, "%d", dripCupValues[i]);
    drawStyledButton(cupLabel, btnX, btnY, dripBtnW, dripBtnH);
  }
}

void drawOnOffScreen() {
  uint16_t bgColor = isOnState ? TFT_BLUE : TFT_RED;
  // Draw one large button covering most of the screen
  int padding = 10;
  int btnX = padding;
  int btnY = padding;
  int btnW = tft.width() - 2 * padding;
  int btnH = tft.height() - 2 * padding;

  tft.fillScreen(TFT_WHITE); // Background behind button
  tft.fillRect(btnX, btnY, btnW, btnH, bgColor);
  // Optional: Add text like "ON" or "OFF"
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.drawString(isOnState ? "ON" : "OFF", tft.width() / 2, tft.height() / 2);
}
// --- Touch Handling Functions ---

void handleMainMenuTouch(int x, int y) {
  // Check Button 1 (Espresso)
  if ((x >= mainBtnX1 && x < mainBtnX1 + mainBtnW) && (y >= mainBtnY1 && y < mainBtnY1 + mainBtnH)) {
    Serial.println("Espresso button pressed");
    currentScreen = ESPRESSO_MENU;
    drawEspressoMenuScreen();
  }
  // Check Button 2 (Drip)
  else if ((x >= mainBtnX2 && x < mainBtnX2 + mainBtnW) && (y >= mainBtnY1 && y < mainBtnY1 + mainBtnH)) {
    Serial.println("Drip button pressed");
    currentScreen = DRIP_MENU;
    drawDripMenuScreen();
  }
  // Check Button 3 (Calibrate)
  else if ((x >= mainBtnX1 && x < mainBtnX1 + mainBtnW) && (y >= mainBtnY2 && y < mainBtnY2 + mainBtnH)) {
    Serial.println("Calibrate button pressed");
    displayTemporaryMessage("Calibrating...", 1500);
    drawMainMenuScreen(); // Return to main after message
  }
  // Check Button 4 (On/Off)
  else if ((x >= mainBtnX2 && x < mainBtnX2 + mainBtnW) && (y >= mainBtnY2 && y < mainBtnY2 + mainBtnH)) {
    Serial.println("On/Off button pressed");
    currentScreen = ON_OFF_SCREEN;
    drawOnOffScreen();
  }
}

void handleEspressoMenuTouch(int x, int y) {
  const char* gramsLabel = nullptr;
  // Check Button 1 (18g)
  if ((x >= espBtnX1 && x < espBtnX1 + espBtnW) && (y >= espBtnY1 && y < espBtnY1 + espBtnH)) {
     gramsLabel = espBtn1Label;
  }
  // Check Button 2 (18.5g)
  else if ((x >= espBtnX2 && x < espBtnX2 + espBtnW) && (y >= espBtnY1 && y < espBtnY1 + espBtnH)) {
     gramsLabel = espBtn2Label;
  }
  // Check Button 3 (19g)
  else if ((x >= espBtnX1 && x < espBtnX1 + espBtnW) && (y >= espBtnY2 && y < espBtnY2 + espBtnH)) {
     gramsLabel = espBtn3Label;
  }
  // Check Button 4 (19.5g)
  else if ((x >= espBtnX2 && x < espBtnX2 + espBtnW) && (y >= espBtnY2 && y < espBtnY2 + espBtnH)) {
     gramsLabel = espBtn4Label;
  }

  if (gramsLabel != nullptr) {
      char msg[30];
      sprintf(msg, "Grinding %s", gramsLabel);
      Serial.println(msg);
      displayTemporaryMessage(msg, 2000);
      currentScreen = MAIN_MENU; // Return to main menu
      drawMainMenuScreen();
  }
}

void handleDripMenuTouch(int x, int y) {
  int selectedCups = -1;
  // Check the 10 drip buttons
  for (int i = 0; i < 10; ++i) {
    int row = i / dripBtnCols;
    int col = i % dripBtnCols;
    int btnX = dripBtnSpacing + col * (dripBtnW + dripBtnSpacing);
    int btnY = dripBtnStartY + row * (dripBtnH + dripBtnSpacing);

    if ((x >= btnX && x < btnX + dripBtnW) && (y >= btnY && y < btnY + dripBtnH)) {
      selectedCups = dripCupValues[i];
      break; // Found the button
    }
  }

  if (selectedCups != -1) {
    float grams = (float)selectedCups * 9.3;
    char msg[40];
    char floatStr[10];
    // Use dtostrf for reliable float to string conversion: (float, width, precision, char_array)
    dtostrf(grams, 4, 1, floatStr); // e.g., " 18.6" or "111.6" (adjust width if needed)

    sprintf(msg, "Grinding %s grams", floatStr);
    Serial.printf("%d cups selected. %s\n", selectedCups, msg);
    displayTemporaryMessage(msg, 2000);
    currentScreen = MAIN_MENU; // Return to main menu
    drawMainMenuScreen();
  }
}

void handleOnOffTouch(int x, int y) {
  int padding = 10;
  int btnX = padding;
  int btnY = padding;
  int btnW = tft.width() - 2 * padding;
  int btnH = tft.height() - 2 * padding;

  // Check if touch is within the large button area
  if ((x >= btnX && x < btnX + btnW) && (y >= btnY && y < btnY + btnH)) {
      isOnState = !isOnState; // Toggle state
      Serial.printf("On/Off toggled. New state: %s\n", isOnState ? "ON (Blue)" : "OFF (Red)");
      drawOnOffScreen(); // Redraw the screen with the new color/state
  }
}
