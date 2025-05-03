// Required Libraries (Install via Library Manager):
// - TFT_eSPI by Bodmer
// - XPT2046_Touchscreen by Paul Stoffregen
// - HX711 Arduino Library by Bogdan Necula
// - SimpleKalmanFilter by Denys Vitali

#include <SPI.h>
#include <Preferences.h>         // For saving calibration
#include <TFT_eSPI.h>            // Display driver
#include <XPT2046_Touchscreen.h> // Touch driver
#include <HX711.h>               // Load cell amplifier driver
#include <SimpleKalmanFilter.h>  // Kalman Filter library
#include <stdio.h>               // For sprintf
#include <stdlib.h>              // For dtostrf

//======================================================================
// START: Configuration (from config.h)
//======================================================================

// --- Touch Calibration Values ---
// !!! IMPORTANT: THESE VALUES ARE EXAMPLES AND LIKELY NEED ADJUSTMENT !!!
// !!! Use a calibration sketch to find the correct values for YOUR screen !!!
#define TOUCH_X_MIN 500
#define TOUCH_X_MAX 3700
#define TOUCH_Y_MIN 400
#define TOUCH_Y_MAX 3700

// --- Pin Definitions ---
// TFT display (Using TFT_eSPI defaults, often VSPI on ESP32)
// These are often defined within the TFT_eSPI library's User_Setup.h
// If you have issues, ensure they match your board/wiring or User_Setup.h
// #define TFT_MOSI 23 // Default VSPI MOSI
// #define TFT_SCLK 18 // Default VSPI SCLK
// #define TFT_CS    5 // Chip select for TFT
// #define TFT_DC    2 // Data/Command for TFT
// #define TFT_RST   4 // Reset for TFT
//#define TFT_BL   15 // Backlight control (Check if your board uses this pin)

// XPT2046 Touch Screen Pins (User Specified)
#define XPT2046_IRQ 36 // Touch interrupt pin
#define XPT2046_CS  33 // Touch chip select pin
// NOTE: XPT2046_Touchscreen library typically uses the default hardware SPI pins
// for MOSI, MISO, CLK unless configured otherwise. Ensure these match your wiring
// if you are using non-default SPI pins.
#define XPT2046_MOSI 32 // Only needed if using non-default SPI pins AND library supports it
#define XPT2046_MISO 39 // Only needed if using non-default SPI pins AND library supports it
#define XPT2046_CLK  25 // Only needed if using non-default SPI pins AND library supports it

// HX711 Load Cell Amplifier
#define HX711_DOUT 22 // Data Out pin from HX711
#define HX711_SCK  27 // Clock pin to HX711


// Grinder Simulation (Onboard LED or external LED)
#ifndef LED_BUILTIN
#define LED_BUILTIN 4 // Define if not already defined for your specific ESP32 board
#endif
#define GRINDER_LED_PIN LED_BUILTIN // Use built-in LED to simulate grinder activity

// --- Scale & Calibration ---
#define CALIBRATION_WEIGHT_G 100.0f // Known weight in grams for calibration
#define PREFS_APP_NAME "grinder"       // Namespace for Preferences storage
#define PREFS_SCALE_FACTOR_KEY "calFactor" // Key for storing calibration factor

// --- Kalman Filter Parameters ---
// Tune these based on observed noise vs response speed needs
// (measurement_uncertainty, estimate_uncertainty, process_noise)
#define KF_MEASUREMENT_ERROR 0.02f // How much uncertainty in the raw HX711 reading?
#define KF_ESTIMATE_ERROR 0.02f    // How much uncertainty in the current filtered estimate?
#define KF_PROCESS_NOISE 0.01f     // How much noise do we expect from the grinding process itself?

// --- Grinding Parameters ---
#define GRIND_CONTINUOUS_THRESHOLD 0.90f // Switch to pulsing below (target * this value)
#define GRIND_PULSE_ON_MS 200            // Duration of grinder ON pulse (milliseconds)
#define GRIND_PULSE_DUTY_CYCLE 0.30f     // Proportion of time the grinder is ON during pulsing (0.0 to 1.0)
#define GRIND_TARGET_TOLERANCE_G 0.1f    // Stop grinding when weight is within this tolerance below target

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

// --- Espresso Menu Layout ---
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
// const int dripBtnRows = 2; // Implicitly 2 rows for 10 buttons
const int dripBtnW = 52; // Calculated: (320 - (dripBtnCols + 1) * dripBtnSpacing) / dripBtnCols
const int dripBtnH = 85; // Calculated: (240 - dripHeaderH - (2 + 1) * dripBtnSpacing) / 2
const int dripBtnSpacing = 10;
const int dripBtnStartY = 10 + dripHeaderH + dripBtnSpacing;
int dripCupValues[10] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 12}; // Grams per cup ratio is applied later

//======================================================================
// END: Configuration
//======================================================================


//======================================================================
// START: Global Variables & Objects
//======================================================================

// Define screen states
enum ScreenState {
  MAIN_MENU,
  ESPRESSO_MENU,
  DRIP_MENU,
  ON_OFF_SCREEN,
  CALIBRATING,
  GRINDING
};

SPIClass mySpi = SPIClass(VSPI);
// Define global objects
TFT_eSPI tft = TFT_eSPI(); // TFT object
// Use the user-specified CS and IRQ pins for the touch controller
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ); // Touchscreen object
HX711 scale; // HX711 Load cell object
// Define Kalman Filter object globally
SimpleKalmanFilter weightKalmanFilter(KF_MEASUREMENT_ERROR, KF_ESTIMATE_ERROR, KF_PROCESS_NOISE); // Kalman filter object

// Define global state variables
ScreenState currentScreen = MAIN_MENU; // Start at the main menu
bool isOnState = true; // State for the On/Off screen
float calibration_factor = 2280.0f; // Default calibration factor (will be overwritten by Preferences if saved)

Preferences preferences; // Preferences object for non-volatile storage

//======================================================================
// END: Global Variables & Objects
//======================================================================


//======================================================================
// START: UI Drawing Functions
//======================================================================

// Helper function to draw our standard button style
void drawStyledButton(const char* label, int x, int y, int w, int h) {
    tft.fillRect(x, y, w, h, TFT_YELLOW); // Button background
    tft.drawRect(x, y, w, h, TFT_BLACK);  // Button border
    tft.setTextColor(TFT_BLACK);          // Text color
    tft.setTextDatum(MC_DATUM);           // Middle Center Datum for text alignment
    tft.setTextSize(2);                   // Text size
    tft.drawString(label, x + w / 2, y + h / 2); // Draw label centered
}

// Draw the main menu screen with 4 buttons
void drawMainMenuScreen() {
  tft.fillScreen(TFT_WHITE); // Clear screen
  drawStyledButton(mainBtn1Label, mainBtnX1, mainBtnY1, mainBtnW, mainBtnH); // Espresso
  drawStyledButton(mainBtn2Label, mainBtnX2, mainBtnY1, mainBtnW, mainBtnH); // Drip
  drawStyledButton(mainBtn3Label, mainBtnX1, mainBtnY2, mainBtnW, mainBtnH); // Calibrate
  drawStyledButton(mainBtn4Label, mainBtnX2, mainBtnY2, mainBtnW, mainBtnH); // On/Off
}

// Draw the espresso menu screen with 4 dose buttons
void drawEspressoMenuScreen() {
  tft.fillScreen(TFT_WHITE); // Clear screen
  drawStyledButton(espBtn1Label, espBtnX1, espBtnY1, espBtnW, espBtnH); // 18g
  drawStyledButton(espBtn2Label, espBtnX2, espBtnY1, espBtnW, espBtnH); // 18.5g
  drawStyledButton(espBtn3Label, espBtnX1, espBtnY2, espBtnW, espBtnH); // 19g
  drawStyledButton(espBtn4Label, espBtnX2, espBtnY2, espBtnW, espBtnH); // 19.5g
}

// Draw the drip menu screen with a header and 10 cup buttons
void drawDripMenuScreen() {
  tft.fillScreen(TFT_WHITE); // Clear screen
  // Draw Header
  tft.setTextColor(TFT_BLACK);
  tft.setTextDatum(TC_DATUM); // Top Center Datum
  tft.setTextSize(3);         // Larger text for header
  tft.drawString("Cups", tft.width() / 2, 10); // Draw header text

  // Draw 10 buttons
  tft.setTextSize(2); // Reset text size for buttons
  for (int i = 0; i < 10; ++i) {
    int row = i / dripBtnCols; // Calculate row (0 or 1)
    int col = i % dripBtnCols; // Calculate column (0 to 4)
    int btnX = dripBtnSpacing + col * (dripBtnW + dripBtnSpacing); // Calculate button X position
    int btnY = dripBtnStartY + row * (dripBtnH + dripBtnSpacing); // Calculate button Y position
    char cupLabel[4]; // Buffer for cup number string
    sprintf(cupLabel, "%d", dripCupValues[i]); // Format cup number
    drawStyledButton(cupLabel, btnX, btnY, dripBtnW, dripBtnH); // Draw the button
  }
}

// Draw the On/Off screen (a single large button showing current state)
void drawOnOffScreen() {
  uint16_t bgColor = isOnState ? TFT_BLUE : TFT_RED; // Blue for ON, Red for OFF
  int padding = 10;
  int btnX = padding;
  int btnY = padding;
  int btnW = tft.width() - 2 * padding;
  int btnH = tft.height() - 2 * padding;

  tft.fillScreen(TFT_WHITE); // Clear screen background
  tft.fillRect(btnX, btnY, btnW, btnH, bgColor); // Draw the large button

  // Draw "ON" or "OFF" text
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(4);
  tft.drawString(isOnState ? "ON" : "OFF", tft.width() / 2, tft.height() / 2);
}

// Helper function to display a temporary message centered on screen
void displayTemporaryMessage(const char* msg, uint16_t duration) {
  tft.fillScreen(TFT_DARKCYAN); // Use a distinct background color
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(msg, tft.width() / 2, tft.height() / 2); // Draw message centered
  delay(duration); // Wait for the specified duration
  // IMPORTANT: The function that calls this MUST redraw the appropriate screen afterwards
}

// New function to display steps during calibration process
void drawCalibrationStepScreen(const char* line1, const char* line2, bool wait) {
    tft.fillScreen(TFT_DARKCYAN); // Consistent background for calibration
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM); // Center alignment
    tft.setTextSize(2);         // Standard text size

    // Draw the first line of text if provided
    if (line1) {
        tft.drawString(line1, tft.width() / 2, tft.height() / 2 - 20); // Position above center
    }
    // Draw the second line of text if provided
    if (line2) {
        tft.drawString(line2, tft.width() / 2, tft.height() / 2 + 20); // Position below center
    }
    // If 'wait' is true, add a prompt to tap the screen
    if (wait) {
        tft.setTextSize(1); // Smaller text for prompt
        tft.setTextDatum(BC_DATUM); // Bottom Center alignment
        tft.drawString("Tap screen to continue", tft.width() / 2, tft.height() - 10); // Position at bottom
    }
}
//======================================================================
// END: UI Drawing Functions
//======================================================================


//======================================================================
// START: Grinding Function
//======================================================================

// Main function to handle the grinding process
void startGrinding(float targetWeight) {
    currentScreen = GRINDING; // Set the screen state

    // Calculate pulse OFF time based on ON time and duty cycle
    const unsigned long pulseOffTime = (GRIND_PULSE_ON_MS / GRIND_PULSE_DUTY_CYCLE) - GRIND_PULSE_ON_MS;

    // Grinding state variables
    bool grinding = true;          // Flag to control the main grinding loop
    bool continuousPhase = true;   // Start in continuous grinding phase
    unsigned long pulseTimer = 0;  // Timer for pulsing logic
    bool pulseStateIsOn = false;   // Tracks if the grinder is currently ON during pulsing
    float currentWeight = 0.0;     // Filtered weight estimate
    float rawWeight = 0.0;         // Raw weight reading from HX711

    digitalWrite(GRINDER_LED_PIN, LOW); // Ensure grinder (LED) is initially OFF
    Serial.printf("Starting grind. Target: %.1fg\n", targetWeight);

    // --- Initial Grinding Screen Setup ---
    tft.fillScreen(TFT_BLACK); // Use a dark background for grinding screen
    tft.setTextColor(TFT_WHITE, TFT_BLACK); // White text on black background
    tft.setTextDatum(TL_DATUM); // Top Left Datum
    tft.setTextSize(2);
    char titleStr[30];
    sprintf(titleStr, "Grinding: %.1fg", targetWeight); // Display target weight
    tft.drawString(titleStr, 10, 10); // Draw title at top-left
    tft.setTextSize(3);
    tft.drawString("Actual:", 10, 60); // Label for current weight
    int weightYPos = 100; // Y position for the weight display box
    int weightHeight = 40; // Height of the weight display box
    // Draw a rectangle to frame the weight display
    tft.drawRect(10, weightYPos, tft.width() - 20, weightHeight, TFT_DARKGREY);
    // Add instruction to stop grinding
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); // Yellow text for instruction
    tft.setTextDatum(BC_DATUM); // Bottom Center Datum
    tft.drawString("Tap screen to STOP", tft.width() / 2, tft.height() - 10); // Draw stop instruction

    unsigned long lastWeightUpdateTime = 0; // Timer for periodic weight reading
    // Optional: Reset Kalman filter state before starting a new grind?
    // weightKalmanFilter.setEstimateError(KF_ESTIMATE_ERROR); // Reset error estimate if desired

    // --- Main Grinding Loop ---
    while (grinding) {
        unsigned long now = millis(); // Get current time

        // --- Check for Stop Condition (Touch Interrupt) ---
        if (ts.touched()) {
            digitalWrite(GRINDER_LED_PIN, LOW); // Stop grinder (LED)
            Serial.println("Grind interrupted by touch!");
            grinding = false; // Exit the grinding loop

            // Display STOP message
            tft.fillScreen(TFT_RED);
            tft.setTextColor(TFT_WHITE);
            tft.setTextDatum(MC_DATUM);
            tft.setTextSize(4);
            tft.drawString("STOP", tft.width() / 2, tft.height() / 2);
            delay(2000); // Show message for 2 seconds

            // Return to main menu
            currentScreen = MAIN_MENU;
            drawMainMenuScreen();
            // Wait for touch release to prevent immediate re-triggering
            while(ts.touched()) delay(50);
            return; // Exit the startGrinding function
        }

        // --- Read Weight periodically & Apply Kalman Filter ---
        // Read weight less frequently than the main loop runs
        if (now - lastWeightUpdateTime > 100) { // Read every 100ms
            rawWeight = scale.get_units(3); // Get calibrated weight average (3 readings)
            // Handle potential NaN (Not a Number) readings from HX711
            if (isnan(rawWeight)) {
                rawWeight = currentWeight; // Use last known good filtered weight if reading fails
                Serial.println("Warning: HX711 returned NaN, using last filtered weight.");
            }

            // Apply Kalman filter to the raw weight
            currentWeight = weightKalmanFilter.updateEstimate(rawWeight);

            // --- Update weight display on TFT ---
            tft.setTextDatum(MC_DATUM); // Middle Center Datum
            tft.setTextColor(TFT_WHITE, TFT_BLACK); // White text on black background
            tft.setTextSize(3); // Font size for weight
            char currentStr[10];
            dtostrf(currentWeight, 4, 1, currentStr); // Format filtered weight to string (e.g., " 18.2")
            // Clear the previous weight value inside the rectangle before drawing new one
            tft.fillRect(11, weightYPos + 1, tft.width() - 22, weightHeight - 2, TFT_BLACK);
            tft.drawString(currentStr, tft.width() / 2, weightYPos + weightHeight / 2); // Draw centered weight

            lastWeightUpdateTime = now; // Reset the weight update timer
            // Serial.printf("Raw: %.2f g, Filtered: %.2f g\n", rawWeight, currentWeight); // Debug output
        }

        // --- Check for Target Reached ---
        // Use the FILTERED weight for target checking for stability
        if (currentWeight >= targetWeight - GRIND_TARGET_TOLERANCE_G) {
            digitalWrite(GRINDER_LED_PIN, LOW); // Stop grinder (LED)
            Serial.printf("Target weight reached! Final filtered weight: %.2fg\n", currentWeight);
            grinding = false; // Exit the grinding loop

            // --- Success Feedback ---
            tft.fillScreen(TFT_GREEN); // Green background for success
            tft.setTextColor(TFT_BLACK); // Black text
            tft.setTextDatum(MC_DATUM);
            tft.setTextSize(2);
            char finalTargetStr[30], finalActualStr[30];
            sprintf(finalTargetStr, "Target: %.1fg", targetWeight);
            // Display final FILTERED weight
            sprintf(finalActualStr, "Actual: %.1fg", currentWeight);
            tft.drawString(finalTargetStr, tft.width()/2, tft.height()/2 - 20); // Show target
            tft.drawString(finalActualStr, tft.width()/2, tft.height()/2 + 20); // Show actual final weight
            delay(3000); // Show success message for 3 seconds

            // Return to main menu
            currentScreen = MAIN_MENU;
            drawMainMenuScreen();
            return; // Exit the startGrinding function
        }

        // --- Control Grinder (LED) - Non-Blocking Logic ---
        // Use FILTERED weight to decide the grinding phase (continuous or pulsing)
        if (currentWeight < targetWeight * GRIND_CONTINUOUS_THRESHOLD) {
            // --- Continuous Phase ---
            // If we just entered continuous phase, log it
            if (!continuousPhase) {
                 Serial.println("Entering continuous phase.");
                 continuousPhase = true;
            }
            digitalWrite(GRINDER_LED_PIN, HIGH); // Turn grinder (LED) ON
        } else {
            // --- Pulse Phase ---
            // If we just entered pulse phase, log it and set up timer
            if (continuousPhase) {
                Serial.println("Entering pulse phase.");
                digitalWrite(GRINDER_LED_PIN, LOW); // Ensure grinder is OFF initially in pulse phase
                pulseStateIsOn = false;           // Start with pulse OFF
                pulseTimer = now;                 // Initialize pulse timer
                continuousPhase = false;          // Mark as in pulse phase
            }

            // Pulsing Logic (non-blocking)
            if (pulseStateIsOn) {
                // Currently ON: Check if ON time has elapsed
                if (now - pulseTimer >= GRIND_PULSE_ON_MS) {
                    digitalWrite(GRINDER_LED_PIN, LOW); // Turn OFF
                    pulseStateIsOn = false;           // Mark as OFF
                    pulseTimer = now;                 // Reset timer for OFF period
                }
            } else {
                // Currently OFF: Check if OFF time has elapsed
                if (now - pulseTimer >= pulseOffTime) {
                     digitalWrite(GRINDER_LED_PIN, HIGH); // Turn ON
                     pulseStateIsOn = true;            // Mark as ON
                     pulseTimer = now;                  // Reset timer for ON period
                }
            }
        }
         yield(); // Allow background tasks (like WiFi, if used) to run
    } // End while(grinding)
}

//======================================================================
// END: Grinding Function
//======================================================================


//======================================================================
// START: Touch Handling & Calibration Functions
//======================================================================

// Helper function to wait until the touchscreen is released
void waitForTouchRelease() {
    // Add a small initial delay in case this is called immediately after a press
    delay(50);
    while (ts.touched()) {
        // Yield to allow background tasks while waiting
        yield();
        delay(50); // Check every 50ms
    }
    // Add a debounce delay after release
    delay(100);
}

// Helper function to wait until the touchscreen is pressed
void waitForTouchPress() {
    // Wait for initial press
    while (!ts.touched()) {
        // Yield to allow background tasks while waiting
        yield();
        delay(50); // Check every 50ms
    }
    // Add a debounce delay after press detected
    delay(100);
}


// Function to perform the scale calibration sequence
void runCalibrationSequence() {
    currentScreen = CALIBRATING; // Set state to prevent other touches

    Serial.println("Starting Calibration Sequence");

    // --- Step 1: Tare the scale ---
    drawCalibrationStepScreen("Calibration: Step 1/2", "Remove all weight", true); // Prompt user
    waitForTouchPress();   // Wait for user confirmation
    waitForTouchRelease(); // Wait for release

    drawCalibrationStepScreen("Taring...", nullptr, false); // Display "Taring..." message
    Serial.println("Taring scale... please wait.");
    scale.tare(20); // Tare the scale with multiple readings for stability (e.g., 20)
    Serial.println("Tare complete.");
    delay(500); // Short delay after taring

    // --- Step 2: Weigh the known calibration weight ---
    char weightPrompt[40];
    sprintf(weightPrompt, "Place %.0fg weight", CALIBRATION_WEIGHT_G); // Create prompt string
    drawCalibrationStepScreen("Calibration: Step 2/2", weightPrompt, true); // Prompt user
    waitForTouchPress();   // Wait for user confirmation
    waitForTouchRelease(); // Wait for release

    drawCalibrationStepScreen("Reading weight...", nullptr, false); // Display "Reading..." message
    Serial.println("Reading known weight...");
    // Get the raw average reading from the HX711 (use more readings for accuracy)
    // Calibration should use the raw average, NOT the filtered value
    long reading = scale.read_average(20);
    Serial.printf("Raw reading for calibration: %ld\n", reading);

    // --- Calculate and Save Calibration Factor ---
    if (reading == 0) {
         // Error handling: reading should not be zero with weight on scale
         Serial.println("Error: Raw reading is zero. Calibration failed. Check wiring/HX711.");
         displayTemporaryMessage("Calibration Failed!", 2500);
    } else {
         // Calculate the new calibration factor
         calibration_factor = (float)reading / CALIBRATION_WEIGHT_G;
         Serial.printf("New calibration factor calculated: %f\n", calibration_factor);

         // Apply the new factor immediately to the scale object
         scale.set_scale(calibration_factor);

         // Save the new factor to Preferences (non-volatile memory)
         preferences.begin(PREFS_APP_NAME, false); // Open Preferences in read/write mode
         preferences.putFloat(PREFS_SCALE_FACTOR_KEY, calibration_factor);
         preferences.end(); // Close Preferences
         Serial.println("Calibration factor saved to Preferences.");

         drawCalibrationStepScreen("Calibration Complete!", nullptr, false); // Show completion message
         delay(2000); // Display message for 2 seconds
    }

    // --- Return to main menu ---
    currentScreen = MAIN_MENU;
    drawMainMenuScreen(); // Redraw the main menu
}


// Handle touch events on the Main Menu
void handleMainMenuTouch(int x, int y) {
    // *** DEBUG: Print coordinates received by this handler ***
    Serial.printf("handleMainMenuTouch received: x=%d, y=%d\n", x, y);

    // Check Button 1 (Espresso)
    if ((x >= mainBtnX1 && x < mainBtnX1 + mainBtnW) && (y >= mainBtnY1 && y < mainBtnY1 + mainBtnH)) {
        // *** DEBUG: Print button detection ***
        Serial.println("-> Espresso Button Area DETECTED");
        Serial.println("Espresso button pressed");
        currentScreen = ESPRESSO_MENU;
        drawEspressoMenuScreen();
    }
    // Check Button 2 (Drip)
    else if ((x >= mainBtnX2 && x < mainBtnX2 + mainBtnW) && (y >= mainBtnY1 && y < mainBtnY1 + mainBtnH)) {
        // *** DEBUG: Print button detection ***
        Serial.println("-> Drip Button Area DETECTED");
        Serial.println("Drip button pressed");
        currentScreen = DRIP_MENU;
        drawDripMenuScreen();
    }
    // Check Button 3 (Calibrate)
    else if ((x >= mainBtnX1 && x < mainBtnX1 + mainBtnW) && (y >= mainBtnY2 && y < mainBtnY2 + mainBtnH)) {
        // *** DEBUG: Print button detection ***
        Serial.println("-> Calibrate Button Area DETECTED");
        Serial.println("Calibrate button pressed");
        runCalibrationSequence(); // Start the calibration sequence
    }
    // Check Button 4 (On/Off)
    else if ((x >= mainBtnX2 && x < mainBtnX2 + mainBtnW) && (y >= mainBtnY2 && y < mainBtnY2 + mainBtnH)) {
        // *** DEBUG: Print button detection ***
        Serial.println("-> On/Off Button Area DETECTED");
        Serial.println("On/Off button pressed");
        currentScreen = ON_OFF_SCREEN;
        drawOnOffScreen();
    }
    // *** DEBUG: Handle case where touch is outside known buttons ***
     else {
         Serial.println("-> Touch UP detected outside known main menu button areas.");
     }
}

// Handle touch events on the Espresso Menu
void handleEspressoMenuTouch(int x, int y) {
  float targetWeight = 0.0; // Initialize target weight
  // Check Button 1 (18g)
  if ((x >= espBtnX1 && x < espBtnX1 + espBtnW) && (y >= espBtnY1 && y < espBtnY1 + espBtnH)) {
     targetWeight = 18.0;
  }
  // Check Button 2 (18.5g)
  else if ((x >= espBtnX2 && x < espBtnX2 + espBtnW) && (y >= espBtnY1 && y < espBtnY1 + espBtnH)) {
     targetWeight = 18.5;
  }
  // Check Button 3 (19g)
  else if ((x >= espBtnX1 && x < espBtnX1 + espBtnW) && (y >= espBtnY2 && y < espBtnY2 + espBtnH)) {
     targetWeight = 19.0;
  }
  // Check Button 4 (19.5g)
  else if ((x >= espBtnX2 && x < espBtnX2 + espBtnW) && (y >= espBtnY2 && y < espBtnY2 + espBtnH)) {
     targetWeight = 19.5;
  }

  // If a valid button was pressed (targetWeight > 0)
  if (targetWeight > 0.0) {
      Serial.printf("Espresso dose selected: %.1fg\n", targetWeight);
      startGrinding(targetWeight); // Call the grinding function
  } else {
      Serial.println("Touch on Espresso screen outside buttons.");
  }
}

// Handle touch events on the Drip Menu
void handleDripMenuTouch(int x, int y) {
  int selectedCups = -1; // Initialize selected cups to invalid value
  // Check the 10 drip buttons
  for (int i = 0; i < 10; ++i) {
    int row = i / dripBtnCols;
    int col = i % dripBtnCols;
    int btnX = dripBtnSpacing + col * (dripBtnW + dripBtnSpacing);
    int btnY = dripBtnStartY + row * (dripBtnH + dripBtnSpacing);

    // Check if touch coordinates fall within the current button's bounds
    if ((x >= btnX && x < btnX + dripBtnW) && (y >= btnY && y < btnY + dripBtnH)) {
      selectedCups = dripCupValues[i]; // Store the number of cups for the pressed button
      break; // Exit loop once a button is found
    }
  }

  // If a valid button was pressed (selectedCups != -1)
  if (selectedCups != -1) {
    // Calculate target weight (Example: 9.3g per cup - ADJUST AS NEEDED)
    float targetWeight = (float)selectedCups * 9.3f;
    Serial.printf("Drip cups selected: %d. Target weight: %.1fg\n", selectedCups, targetWeight);
    startGrinding(targetWeight); // Call the grinding function
  } else {
      Serial.println("Touch on Drip screen outside buttons.");
  }
}

// Handle touch events on the On/Off Screen
void handleOnOffTouch(int x, int y) {
    int padding = 10;
    int btnX = padding;
    int btnY = padding;
    int btnW = tft.width() - 2 * padding;
    int btnH = tft.height() - 2 * padding;

    // Check if touch is within the large button area
    if ((x >= btnX && x < btnX + btnW) && (y >= btnY && y < btnY + btnH)) {
        isOnState = !isOnState; // Toggle the state
        Serial.printf("On/Off toggled. New state: %s\n", isOnState ? "ON (Blue)" : "OFF (Red)");
        drawOnOffScreen(); // Redraw the screen immediately to reflect the new state
    } else {
        Serial.println("Touch on On/Off screen outside button area.");
    }
}
//======================================================================
// END: Touch Handling & Calibration Functions
//======================================================================


//======================================================================
// START: Arduino Setup and Loop
//======================================================================

void setup() {
  Serial.begin(115200); // Start serial communication
  Serial.println("\n\nStarting Grinder Control System v2 (Kalman)...");
  Serial.printf("Compile Time: %s %s\n", __DATE__, __TIME__);
  

  // --- Initialize Preferences & Load Calibration ---
  preferences.begin(PREFS_APP_NAME, true); // Open Preferences in read-only mode first
  // Get the stored calibration factor, using the default if not found
  calibration_factor = preferences.getFloat(PREFS_SCALE_FACTOR_KEY, calibration_factor);
  Serial.printf("Loaded Calibration Factor from Preferences: %f\n", calibration_factor);
  preferences.end(); // Close Preferences

  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  // --- Initialize TFT Display ---
  tft.init();
  tft.setRotation(1); // Set landscape mode (adjust 0-3 if needed)
  tft.fillScreen(TFT_BLACK); // Initial clear screen
  Serial.println("TFT Initialized.");

  // --- Initialize Touch Screen ---
  // Note: Uses XPT2046_CS and XPT2046_IRQ passed to constructor.
  // Assumes default hardware SPI pins (VSPI on many ESP32) are used for MOSI/MISO/CLK.
  ts.begin(mySpi);
  ts.setRotation(1); // Match TFT rotation
  Serial.println("Touchscreen Initialized.");

  // --- Initialize Scale (HX711) ---
  Serial.println("Initializing HX711...");
  scale.begin(HX711_DOUT, HX711_SCK);
  delay(500); // Allow HX711 to settle

      Serial.println("HX711 detected.");
      scale.set_scale(calibration_factor); // Apply the loaded/default calibration factor
      scale.tare(10); // Tare the scale with 10 readings
      Serial.println("HX711 ready and tared.");

      // Perform an initial read and feed it to the Kalman filter to stabilize it
      float initialRead = scale.get_units(5); // Read average of 5
      if (isnan(initialRead)) {
        Serial.println("Warning: Initial HX711 reading is NaN. Using 0.0 for Kalman init.");
        initialRead = 0.0;
      }
       // Initialize Kalman filter with the first stable reading
       weightKalmanFilter.setEstimateError(KF_ESTIMATE_ERROR); // Reset estimate error
       weightKalmanFilter.setProcessNoise(KF_PROCESS_NOISE);   // Reset process noise
       // Update multiple times to let it settle?
       for (int i=0; i<5; ++i) {
           weightKalmanFilter.updateEstimate(initialRead);
           delay(20);
       }
       float filteredInitial = weightKalmanFilter.updateEstimate(initialRead);
       Serial.printf("Initial raw reading: %.2f g, Initial filtered: %.2f g\n", initialRead, filteredInitial);


  // --- Initialize Backlight & Grinder LED Pin ---
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // Turn backlight ON
  pinMode(GRINDER_LED_PIN, OUTPUT);
  digitalWrite(GRINDER_LED_PIN, LOW); // Ensure grinder LED is OFF
  Serial.println("Peripherals Initialized.");

  // --- Draw Initial Screen ---
  currentScreen = MAIN_MENU;
  drawMainMenuScreen();
  Serial.println("Setup complete. Entering main loop.");
}


void loop() {
  // Static variables to track touch state across loop iterations
  static bool touchWasActive = false; // Was the screen being touched in the previous loop?
  static unsigned long touchStartTime = 0; // When did the current touch start?
  static int touchStartX = -1, touchStartY = -1; // Where did the current touch start (mapped coords)?

  // --- Special Handling for Blocking States ---
  // If calibrating or grinding, the respective functions handle their own loops and touch checks.
  // The main loop should just yield to allow background tasks.
  if (currentScreen == CALIBRATING || currentScreen == GRINDING) {
      yield(); // Allow background tasks
      delay(20); // Small delay to prevent busy-waiting
      return;    // Skip normal touch handling
  }

  // --- Normal Touch Handling for Menus ---
  bool touchIsActive = ts.touched(); // Check if the screen is currently being touched

  // --- Touch Down Event ---
  // Triggered once when the screen transitions from not touched to touched
  if (touchIsActive && !touchWasActive) {
    touchWasActive = true; // Mark touch as active
    touchStartTime = millis(); // Record the start time of the touch
    TS_Point p = ts.getPoint(); // Get the raw touch data

    // *** DEBUG: Print raw touch data ***
    Serial.printf("Raw Touch Down: x=%d, y=%d, z=%d\n", p.x, p.y, p.z);

    // Map raw coordinates to screen coordinates
    int screenX = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, tft.width());
    int screenY = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, tft.height());
    // Constrain coordinates to be within screen bounds
    touchStartX = constrain(screenX, 0, tft.width() - 1);
    touchStartY = constrain(screenY, 0, tft.height() - 1);

    // *** DEBUG: Print mapped touch data ***
    Serial.printf("Mapped Touch Down: screenX=%d, screenY=%d\n", touchStartX, touchStartY);

  // --- Touch Hold Event ---
  // Triggered repeatedly while the screen remains touched
  } else if (touchIsActive && touchWasActive) {
    // Implement long-press behavior (e.g., return to main menu from submenus)
    // Exclude main menu from long-press return
    if (currentScreen != MAIN_MENU && currentScreen != ON_OFF_SCREEN && // Add other screens if needed
        millis() - touchStartTime > 3000) // Check if held for more than 3 seconds
    {
      Serial.println("Long press timeout detected, returning to main menu.");
      currentScreen = MAIN_MENU;
      drawMainMenuScreen();
      touchWasActive = false; // Reset touch state as action is handled
      // Wait for touch release to prevent immediate re-trigger after returning
      waitForTouchRelease();
    }

  // --- Touch Up Event ---
  // Triggered once when the screen transitions from touched to not touched
  } else if (!touchIsActive && touchWasActive) {
    touchWasActive = false; // Mark touch as inactive

    // *** DEBUG: Print mapped touch coordinates on release ***
    Serial.printf("Mapped Touch Up at: x=%d, y=%d\n", touchStartX, touchStartY);

    // Process the touch based on the current screen state
    // Pass the START coordinates of the touch to the handlers
    switch (currentScreen) {
        case MAIN_MENU:     handleMainMenuTouch(touchStartX, touchStartY); break;
        case ESPRESSO_MENU: handleEspressoMenuTouch(touchStartX, touchStartY); break;
        case DRIP_MENU:     handleDripMenuTouch(touchStartX, touchStartY); break;
        case ON_OFF_SCREEN: handleOnOffTouch(touchStartX, touchStartY); break;
        // CALIBRATING and GRINDING states have their touch handling within their functions
        default:
            Serial.println("Touch Up detected in unhandled screen state.");
            break; // Should not happen in normal operation
    }
    // Reset the touch start coordinates after processing the touch up event
    touchStartX = -1;
    touchStartY = -1;

  // --- No Touch Active ---
  } else {
    // Nothing to do if there's no touch activity
  }

  delay(20); // Small delay in the main loop to prevent excessive CPU usage
}

//======================================================================
// END: Arduino Setup and Loop
//======================================================================
