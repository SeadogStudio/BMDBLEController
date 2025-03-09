#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "BMDBLEController.h"
#include "BMDBLEConstants.h"

// OLED Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

// Display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Controller object
BMDBLEController bmdController("ESP32CameraTest");

// Test sequence state machine
enum TestState {
  TEST_INIT,
  TEST_SCAN,
  TEST_CONNECT,
  TEST_WAIT_CONNECTION,
  TEST_GET_INFO,
  TEST_FOCUS_TEST,
  TEST_WB_TEST,
  TEST_COMPLETE,
  TEST_FAILED
};

// Test variables
TestState testState = TEST_INIT;
unsigned long testStateStartTime = 0;
unsigned long lastActionTime = 0;
int testStep = 0;

// Focus test values (0-2048 range)
const int FOCUS_TEST_STEPS = 5;
const uint16_t focusTestValues[FOCUS_TEST_STEPS] = {0, 512, 1024, 1536, 2048};

// White balance test values (Kelvin)
const int WB_TEST_STEPS = 5;
const uint16_t wbTestValues[WB_TEST_STEPS] = {3200, 4500, 5600, 7500, 10000};

// Test status variables
String lensModel = "";
String focalLength = "";
String focusDistance = "";
String whiteBalance = "";
bool focusUpdated = false;
bool wbUpdated = false;

// Forward declarations
void changeTestState(TestState newState);
void displayStatus();
void startTest();
void runTestSequence();

// Parameter update callback
void onParameterUpdate(uint8_t category, uint8_t parameterId, String value) {
  Serial.print("Parameter updated - Category: 0x");
  Serial.print(category, HEX);
  Serial.print(", Parameter: 0x");
  Serial.print(parameterId, HEX);
  Serial.print(", Value: ");
  Serial.println(value);
  
  // Store specific parameters
  if (category == BMD_CAT_EXTENDED_LENS) {
    switch (parameterId) {
      case BMD_PARAM_LENS_MODEL:
        lensModel = value;
        break;
      case BMD_PARAM_LENS_FOCAL_LENGTH:
        focalLength = value;
        break;
      case BMD_PARAM_LENS_FOCUS_DISTANCE:
        focusDistance = value;
        break;
    }
  } else if (category == BMD_CAT_VIDEO && parameterId == BMD_PARAM_WB) {
    whiteBalance = value;
    wbUpdated = true;
  } else if (category == BMD_CAT_LENS && parameterId == BMD_PARAM_FOCUS) {
    focusUpdated = true;
  }
  
  // Update display with new info
  displayStatus();
}

// Connection status callback
void onConnectionChange(bool connected) {
  Serial.print("Connection status changed: ");
  Serial.println(connected ? "Connected" : "Disconnected");
  
  // Update display
  displayStatus();
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  Serial.println("Blackmagic Camera BLE Control Test");
  
  // Initialize I2C for OLED
  Wire.begin(21, 22);
  
  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    while(1); // Don't proceed
  }
  
  // Initial display setup
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("BMD Camera Test");
  display.println("Starting in 2s...");
  display.display();
  
  // Initialize BLE controller
  bmdController.begin();
  
  // Set callbacks
  bmdController.setResponseCallback(onParameterUpdate);
  bmdController.setConnectionCallback(onConnectionChange);
  
  // Wait a moment before starting
  delay(2000);
  
  // Start the test sequence
  startTest();
}

void loop() {
  // Update controller
  bmdController.loop();
  
  // Run test sequence
  runTestSequence();
}

// Start the test
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
    case TEST_INIT:
      Serial.println("INIT");
      break;
    case TEST_SCAN:
      Serial.println("SCAN");
      // Display update
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Scanning for");
      display.println("Blackmagic camera...");
      display.display();
      // Start scanning
      bmdController.scan(10);
      break;
    case TEST_CONNECT:
      Serial.println("CONNECT");
      // Display update
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Connecting to camera");
      display.display();
      break;
    case TEST_WAIT_CONNECTION:
      Serial.println("WAIT_CONNECTION");
      break;
    case TEST_GET_INFO:
      Serial.println("GET_INFO");
      // Display update
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Getting camera info");
      display.display();
      break;
    case TEST_FOCUS_TEST:
      Serial.println("FOCUS_TEST");
      focusUpdated = false;
      // Display update
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Starting focus test");
      display.display();
      break;
    case TEST_WB_TEST:
      Serial.println("WB_TEST");
      wbUpdated = false;
      // Display update
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Starting WB test");
      display.display();
      break;
    case TEST_COMPLETE:
      Serial.println("COMPLETE");
      // Display update
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Test Complete!");
      display.println("");
      if (lensModel.length() > 0) {
        display.print("Lens: ");
        display.println(lensModel);
      }
      display.display();
      break;
    case TEST_FAILED:
      Serial.println("FAILED");
      // Display update
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Test Failed!");
      display.println("Please restart");
      display.display();
      break;
  }
}

// Run the test sequence state machine
void runTestSequence() {
  if (testState == TEST_INIT || testState == TEST_COMPLETE || testState == TEST_FAILED) {
    return; // Nothing to do in these states
  }
  
  unsigned long currentTime = millis();
  
  switch (testState) {
    case TEST_SCAN:
      // Wait for scan completion (device found or timeout)
      if (bmdController._deviceFound) {
        Serial.println("Blackmagic camera found!");
        changeTestState(TEST_CONNECT);
      } else if (currentTime - testStateStartTime > 15000) {
        // Timeout after 15 seconds
        Serial.println("Scan timeout, no camera found");
        changeTestState(TEST_FAILED);
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
            changeTestState(TEST_FAILED);
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
        changeTestState(TEST_FAILED);
      }
      break;
      
    case TEST_GET_INFO:
      // Request camera information
      if (currentTime - lastActionTime > 1000) {
        if (testStep == 0) {
          Serial.println("Requesting lens model...");
          bmdController.requestParameter(BMD_CAT_EXTENDED_LENS, BMD_PARAM_LENS_MODEL, BMD_TYPE_STRING);
          testStep++;
          lastActionTime = currentTime;
        } else if (testStep == 1) {
          Serial.println("Requesting focal length...");
          bmdController.requestParameter(BMD_CAT_EXTENDED_LENS, BMD_PARAM_LENS_FOCAL_LENGTH, BMD_TYPE_STRING);
          testStep++;
          lastActionTime = currentTime;
        } else if (testStep == 2) {
          Serial.println("Requesting focus distance...");
          bmdController.requestParameter(BMD_CAT_EXTENDED_LENS, BMD_PARAM_LENS_FOCUS_DISTANCE, BMD_TYPE_STRING);
          testStep++;
          lastActionTime = currentTime;
        } else if (testStep == 3) {
          Serial.println("Requesting white balance...");
          bmdController.requestParameter(BMD_CAT_VIDEO, BMD_PARAM_WB, BMD_TYPE_INT16);
          testStep++;
          lastActionTime = currentTime;
        } else if (testStep == 4) {
          // Wait a bit to let responses come in
          if (currentTime - lastActionTime > 2000) {
            changeTestState(TEST_FOCUS_TEST);
          }
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
          changeTestState(TEST_COMPLETE);
        }
      }
      break;
  }
}

// Update display with status information
void displayStatus() {
  // Only update in certain states
  if (testState == TEST_COMPLETE || testState == TEST_FAILED) {
    return;
  }
  
  // Update based on current state
  switch (testState) {
    case TEST_GET_INFO:
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Camera Information:");
      
      if (lensModel.length() > 0) {
        display.print("Lens: ");
        // Truncate if too long
        if (lensModel.length() > 18) {
          display.println(lensModel.substring(0, 18));
        } else {
          display.println(lensModel);
        }
      }
      
      if (focalLength.length() > 0) {
        display.print("Focal: ");
        display.println(focalLength);
      }
      
      if (focusDistance.length() > 0) {
        display.print("Focus: ");
        display.println(focusDistance);
      }
      
      if (whiteBalance.length() > 0) {
        display.print("WB: ");
        display.println(whiteBalance);
      }
      
      display.display();
      break;
      
    case TEST_FOCUS_TEST:
    case TEST_WB_TEST:
      // These are handled in runTestSequence with specific displays
      break;
      
    default:
      // Only update if we're connected
      if (bmdController.isConnected()) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Connected to camera");
        display.print("State: ");
        display.println(testState);
        display.print("Step: ");
        display.println(testStep);
        display.display();
      }
      break;
  }
}
