#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>
#include <HX711.h>
#include <SimpleKalmanFilter.h>

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
const int LOADCELL_DOUT_PIN = 3;
const int LOADCELL_SCK_PIN = 2;

// Create HX711 and kalmna filter object
HX711 scale;
SimpleKalmanFilter kf(0.02, 0.02, 0.01); // Adjust parameters as needed

// Calibration factor for the load cell
float calibration_factor = -7050; // Initial calibration factor


SPIClass mySpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

TFT_eSPI tft = TFT_eSPI();

struct Button {
  int x, y, w, h;
  String label;
};

Button mainMenuButtons[] = {
  {40, 60, 80, 40, "Espresso"},
  {160, 60, 80, 40, "Drip"},
  {40, 120, 80, 40, "Calibrate"},
  {160, 120, 80, 40, "Settings"}
};

Button espressoButtons[] = {
  {40, 60, 80, 40, "18g"},
  {160, 60, 80, 40, "18.5g"},
  {40, 120, 80, 40, "19g"},
  {160, 120, 80, 40, "19.5g"}
};

Button dripButtons[] = {
  {20, 40, 60, 40, "3"},
  {90, 40, 60, 40, "4"},
  {160, 40, 60, 40, "5"},
  {230, 40, 60, 40, "6"},
  {20, 100, 60, 40, "7"},
  {90, 100, 60, 40, "8"},
  {160, 100, 60, 40, "9"},
  {230, 100, 60, 40, "10"},
  {20, 160, 60, 40, "11"},
  {90, 160, 60, 40, "12"}
};


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
  
  // scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  // scale.set_scale(calibration_factor);
  // scale.tare(); // Reset the scale to zero

//  // Initial calibration
//   calibrate();

  // Display main menu
  displayMainMenu();

}

void loop(){

//fix below to be activated from the screen

  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();
    printTouchToSerial(p);
    handleTouch(p.x, p.y);
    delay(100);
  }
}

// if (Serial.available()) {
//     char command = Serial.read();
//     if (command == 'c') {
//       calibrate();
//     }
//   }

//   // Read raw value from load cell
//   float raw_weight = scale.get_units(10);

//   // Apply Kalman filter to raw weight
//   float filtered_weight = kf.updateEstimate(raw_weight);

//   // Print the filtered weight
//   Serial.print("Filtered Weight: ");
//   Serial.println(filtered_weight);

//   delay(100);

void printTouchToSerial(TS_Point p) {
  Serial.print("Pressure = ");
  Serial.print(p.z);
  Serial.print(", x = ");
  Serial.print(p.x);
  Serial.print(", y = ");
  Serial.print(p.y);
  Serial.println();
}

void displayMainMenu() {
  tft.fillScreen(TFT_BLACK);
  for (int i = 0; i < 4; i++) {
    drawButton(mainMenuButtons[i]);
  }
}

void displayEspressoMenu() {
  tft.fillScreen(TFT_BLACK);
  for (int i = 0; i < 4; i++) {
    drawButton(espressoButtons[i]);
  }
}

void displayDripMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("Cups:");
  for (int i = 0; i < 10; i++) {
    drawButton(dripButtons[i]);
  }
}

void displayCalibrateScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("Place 100g on scale");
  calibrate();
}

void drawButton(Button btn) {
  tft.fillRect(btn.x, btn.y, btn.w, btn.h, TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(btn.x + 10, btn.y + 10);
  tft.print(btn.label);
}

void handleTouch(int x, int y) {
  for (int i = 0; i < 4; i++) {
    if (isButtonPressed(mainMenuButtons[i], x, y)) {
      if (mainMenuButtons[i].label == "Espresso") {
        displayEspressoMenu();
      } else if (mainMenuButtons[i].label == "Drip") {
        displayDripMenu();
      } else if (mainMenuButtons[i].label == "Calibrate") {
        displayCalibrateScreen();
      } else if (mainMenuButtons[i].label == "Settings") {
        // Handle settings
      }
    }
  }

  for (int i = 0; i < 4; i++) {
    if (isButtonPressed(espressoButtons[i], x, y)) {
      float grindAmount = espressoButtons[i].label.toFloat();
      grind(grindAmount);
    }
  }

  for (int i = 0; i < 10; i++) {
    if (isButtonPressed(dripButtons[i], x, y)) {
      int cups = dripButtons[i].label.toInt();
      float grindAmount = cups * 9.3;
      grind(grindAmount);
    }
  }
}

bool isButtonPressed(Button btn, int x, int y) {
  return (x > btn.x && x < btn.x + btn.w && y > btn.y && y < btn.y + btn.h);
}

void grind(float amount) {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.print("Grinding: ");
  tft.print(amount);
  tft.print("g");

  unsigned long startTime = millis();
  while (millis() - startTime < 3000) {
    if (ts.touched()) {
      displayMainMenu();
      return;
    }
  }

  displayMainMenu();
}

void calibrate() {
  Serial.println("Calibrating...");

  // Read raw value from load cell
  float raw_weight = scale.get_units(10);

  // Known weight for calibration
  float known_weight = 100.0; // 100g weight

  // Calculate new calibration factor
  calibration_factor = raw_weight / known_weight;
  scale.set_scale(calibration_factor);

  Serial.print("New calibration factor: ");
  Serial.println(calibration_factor);
  Serial.println("Calibration complete.");
}
