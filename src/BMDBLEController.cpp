/*
 * Blackmagic Camera BLE Controller Test
 * 
 * This sketch demonstrates how to use the BMDBLEController library
 * to control Blackmagic cameras via Bluetooth LE.
 * 
 * Hardware:
 * - ESP32 board
 * - OLED Display (128x64, I2C on pins 21/22, address 0x3C)
 * - Optional: Button on pin 0 (built-in button on many ESP32 dev boards)
 * 
 * The sketch will automatically scan for Blackmagic cameras, connect
 * to the first one found, and run a test sequence.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "BMDBLEController.h"

// OLED Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1  // Reset pin (-1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C
#define BUTTON_PIN 0  // Built-in button on many ESP32 dev boards

// Display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Controller object
BMDBLEController bmdController("ESP32CameraTest");

// Test sequence variables
enum TestState {
  TEST_IDLE,
  TEST_SCAN,
  TEST_CONNECT,
  TEST_WAIT_CONNECTION,
  TEST_GET_INFO,
  TEST_FOCUS_TEST,
  TEST_WB_TEST,
  TEST_COMPLETE
};

TestState testState = TEST_IDLE;
unsigned long testStateStartTime = 0;
unsigned long lastActionTime = 0;
int testStep = 0;
bool buttonPressed = false;
bool lastButtonState = true;

// Test focus values
const int FOCUS_TEST_STEPS = 5;
const uint16_t focusTestValues[FOCUS_TEST_STEPS] = {0, 512, 1024, 1536, 2048};

// Test white balance values
const int WB_TEST_STEPS = 5;
const uint16_t wbTestValues[WB_TEST_STEPS] = {3200, 4500, 5600, 7500, 10000};

// Keep track of parameter updates for testing
bool focusUpdated = false;
bool wbUpdated = false;

// Forward declarations
void startTest();
void changeTestState(TestState newState);
void runTestStep();
void checkButton();
void parameterCallback(uint8_t category, uint8_t parameterId, String value);

void setup() {
  // Initialize serial
  Serial.begin(115200);
  Serial.println("Blackmagic Camera BLE Controller Test");
  
  // Initialize button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize I2C for OLED
  Wire.begin(21, 22);
  
  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    while(1); // Don't proceed
  }
  
  // Initial display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("BMD Camera Test");
  display.println("Press button to start");
  display.println("or wait 5s for auto");
  display.println("start.");
  display.display();
  
  // Initialize controller with display
  bmdController.begin(&display);
  
  // Set parameter callback
  bmdController.setResponseCallback(parameterCallback);
  
  // Auto-start test after 5 seconds
  delay(5000);
  startTest();
}

void loop() {
  // Update controller
  bmdController.loop();
  
  // Check button state
  checkButton();
  
  // Run test state machine
  runTestStep();
}

// Start the test sequence
void startTest() {
  Serial.println("Starting test sequence");
  changeTestState(TEST_SCAN);
}

// Change test state
void changeTestState(TestState newState) {
  testState = newState;
  testStateStartTime = millis();
  testStep = 0;
  lastActionTime = 0;
  
  Serial.print("Test state changed to: ");
  switch (testState) {
    case TEST_IDLE:
      Serial.println("IDLE");
      break;
    case TEST_SCAN:
      Serial.println("SCAN");
      // Start scanning
      bmdController.scan(10);
      break;
    case TEST_CONNECT:
      Serial.println("CONNECT");
      break;
    case TEST_WAIT_CONNECTION:
      Serial.println("WAIT_CONNECTION");
      break;
    case TEST_GET_INFO:
      Serial.println("GET_INFO");
      break;
    case TEST_FOCUS_TEST:
      Serial.println("FOCUS_TEST");
      focusUpdated = false;
      break;
    case TEST_WB_TEST:
      Serial.println("WB_TEST");
      wbUpdated = false;
      break;
    case TEST_COMPLETE:
      Serial.println("COMPLETE");
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Test Complete!");
      display.println("Press button to");
      display.println("restart test");
      display.display();
      break;
  }
}

// Check button state
void checkButton() {
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  // Detect button press (active low)
  if (lastButtonState == true && currentButtonState == false) {
    buttonPressed = true;
  } else {
    buttonPressed = false;
  }
  
  lastButtonState = currentButtonState;
  
  // Handle button press
  if (buttonPressed) {
    if (testState == TEST_IDLE || testState == TEST_COMPLETE) {
      startTest();
    }
  }
}

// Run test step
void runTestStep() {
  // Skip if idle
  if (testState == TEST_IDLE) {
    return;
  }
  
  // Get current time
  unsigned long currentTime = millis();
  
  // Run state-specific actions
  switch (testState) {
    case TEST_SCAN:
      // Wait for scan completion (device found or timeout)
      if (bmdController._deviceFound) {
        Serial.println("Blackmagic camera found!");
        changeTestState(TEST_CONNECT);
      } else if (currentTime - testStateStartTime > 15000) {
        // Timeout after 15 seconds
        Serial.println("Scan timeout, no camera found");
        changeTestState(TEST_IDLE);
        
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("No camera found!");
        display.println("Press button to");
        display.println("restart scan");
        display.display();
      }
      break;
      
    case TEST_CONNECT:
      // Attempt to connect
      if (currentTime - lastActionTime > 1000) {
        Serial.println("Connecting to camera...");
        bool result = bmdController.connect();
        if (result) {
          Serial.println("Connection initiated");
          changeTestState(TEST_WAIT_CONNECTION);
        } else {
          Serial.println("Failed to initiate connection");
          lastActionTime = currentTime;
          
          // Retry for up to 10 seconds
          if (currentTime - testStateStartTime > 10000) {
            changeTestState(TEST_IDLE);
            
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println("Connection failed!");
            display.println("Press button to");
            display.println("restart test");
            display.display();
          }
        }
      }
      break;
      
    case TEST_WAIT_CONNECTION:
      // Wait for connection to complete
      if (bmdController.isConnected()) {
        Serial.println("Connected to camera!");
        changeTestState(TEST_GET_INFO);
      } else if (currentTime - testStateStartTime > 30000) {
        // Timeout after 30 seconds
        Serial.println("Connection timeout");
        changeTestState(TEST_IDLE);
        
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Connection timeout!");
        display.println("Press button to");
        display.println("restart test");
        display.display();
      }
      break;
      
    case TEST_GET_INFO:
      // Request camera information
      if (currentTime - lastActionTime > 1000) {
        if (testStep == 0) {
          Serial.println("Requesting lens info...");
          bmdController.requestParameter(BMD_CAT_EXTENDED_LENS, 0x09, BMD_TYPE_STRING);
          testStep++;
          lastActionTime = currentTime;
        } else if (testStep == 1) {
          Serial.println("Requesting focal length...");
          bmdController.requestParameter(BMD_CAT_EXTENDED_LENS, 0x0B, BMD_TYPE_STRING);
          testStep++;
          lastActionTime = currentTime;
        } else if (testStep == 2) {
          Serial.println("Requesting focus distance...");
          bmdController.requestParameter(BMD_CAT_EXTENDED_LENS, 0x0C, BMD_TYPE_STRING);
          testStep++;
          lastActionTime = currentTime;
        } else if (testStep == 3) {
          Serial.println("Requesting white balance...");
          bmdController.requestParameter(BMD_CAT_VIDEO, BMD_PARAM_WB, BMD_TYPE_INT16);
          testStep++;
          lastActionTime = currentTime;
        } else if (testStep == 4) {
          Serial.println("Switching to focus test mode...");
          bmdController.setDisplayMode(BMD_DISPLAY_FOCUS);
          changeTestState(TEST_FOCUS_TEST);
        }
      }
      break;
      
    case TEST_FOCUS_TEST:
      // Run focus test sequence
      if (currentTime - lastActionTime > 2000) {
        if (testStep < FOCUS_TEST_STEPS) {
          uint16_t focusValue = focusTestValues[testStep];
          Serial.print("Setting focus to: ");
          Serial.println(focusValue);
          
          // Display info
          display.clearDisplay();
          display.setCursor(0, 0);
          display.println("Focus Test");
          display.print("Setting raw value: ");
          display.println(focusValue);
          display.print("Step ");
          display.print(testStep + 1);
          display.print(" of ");
          display.println(FOCUS_TEST_STEPS);
          display.display();
          
          // Set focus
          bmdController.setFocus(focusValue);
          
          testStep++;
          lastActionTime = currentTime;
          focusUpdated = true;
        } else {
          // Move to next test
          Serial.println("Focus test complete");
          changeTestState(TEST_WB_TEST);
        }
      }
      break;
      
    case TEST_WB_TEST:
      // Run white balance test sequence
      if (currentTime - lastActionTime > 2000) {
        if (testStep < WB_TEST_STEPS) {
          uint16_t wbValue = wbTestValues[testStep];
          Serial.print("Setting white balance to: ");
          Serial.print(wbValue);
          Serial.println("K");
          
          // Display info
          display.clearDisplay();
          display.setCursor(0, 0);
          display.println("White Balance Test");
          display.print("Setting WB: ");
          display.print(wbValue);
          display.println("K");
          display.print("Step ");
          display.print(testStep + 1);
          display.print(" of ");
          display.println(WB_TEST_STEPS);
          display.display();
          
          // Set white balance
          bmdController.setWhiteBalance(wbValue);
          
          testStep++;
          lastActionTime = currentTime;
          wbUpdated = true;
        } else {
          // Test complete
          Serial.println("White balance test complete");
          
          // Switch back to basic display mode
          bmdController.setDisplayMode(BMD_DISPLAY_BASIC);
          
          changeTestState(TEST_COMPLETE);
        }
      }
      break;
      
    case TEST_COMPLETE:
      // Nothing to do, wait for button press to restart
      break;
  }
}

// Parameter update callback
void parameterCallback(uint8_t category, uint8_t parameterId, String value) {
  Serial.print("Parameter updated - Category: 0x");
  Serial.print(category, HEX);
  Serial.print(", Parameter: 0x");
  Serial.print(parameterId, HEX);
  Serial.print(", Value: ");
  Serial.println(value);
  
  // Check for specific parameter updates for test progression
  if (category == BMD_CAT_LENS && parameterId == BMD_PARAM_FOCUS) {
    focusUpdated = true;
  } else if (category == BMD_CAT_VIDEO && parameterId == BMD_PARAM_WB) {
    wbUpdated = true;
  }
}
