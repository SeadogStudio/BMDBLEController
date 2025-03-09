/**
 * BMDBLEController - Recording Control Example
 * 
 * This example demonstrates how to use the BMDBLEController library
 * to control camera recording and monitor timecode. It connects to
 * a Blackmagic camera via BLE and displays recording information
 * on an OLED display.
 * 
 * Hardware:
 * - ESP32 development board
 * - SSD1306 OLED display (128x64 pixels) on I2C
 * - 2 Pushbuttons:
 *   - Button 1 connected to pin 0 (builtin button on most ESP32 dev boards)
 *   - Button 2 connected to pin 4 (for recording control)
 * 
 * Connection:
 * - OLED SDA: GPIO21
 * - OLED SCL: GPIO22
 * - OLED VCC: 3.3V
 * - OLED GND: GND
 * - Recording button: GPIO4
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BMDBLEController.h>

// OLED Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1    // Reset pin # (or -1 if sharing reset pin)
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Button pins
#define SCAN_BUTTON_PIN 0
#define REC_BUTTON_PIN 4

// Create BMDBLEController instance
BMDBLEController cameraController("ESP32RecordingControl");

// Button states
int lastScanButtonState = HIGH;
int lastRecButtonState = HIGH;
unsigned long lastScanDebounceTime = 0;
unsigned long lastRecDebounceTime = 0;
unsigned long debounceDelay = 50;

void setup() {
  // Initialize serial
  Serial.begin(115200);
  Serial.println("BMDBLEController Recording Control Example");
  
  // Initialize buttons
  pinMode(SCAN_BUTTON_PIN, INPUT_PULLUP);
  pinMode(REC_BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true); // Don't proceed
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Recording Control");
  display.println("Starting up...");
  display.display();
  delay(2000);
  
  // Initialize the BMDBLEController with the display
  cameraController.begin(&display);
  
  // Set to recording display mode
  cameraController.setDisplayMode(BMD_DISPLAY_RECORDING);
  
  // Register parameter callback
  cameraController.setResponseCallback(parameterCallback);
  
  Serial.println("Ready! Press scan button to scan for camera");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Recording Control");
  display.println("Press button to scan");
  display.display();
}

void loop() {
  // Handle button presses
  handleScanButton();
  handleRecButton();
  
  // Run library loop
  cameraController.loop();
  
  // Short delay
  delay(10);
}

void handleScanButton() {
  // Read button state with debounce
  int reading = digitalRead(SCAN_BUTTON_PIN);
  
  if (reading != lastScanButtonState) {
    lastScanDebounceTime = millis();
  }
  
  if ((millis() - lastScanDebounceTime) > debounceDelay) {
    // Button press detected
    if (reading == LOW && lastScanButtonState == HIGH) {
      processScanButtonPress();
    }
  }
  
  lastScanButtonState = reading;
}

void handleRecButton() {
  // Read button state with debounce
  int reading = digitalRead(REC_BUTTON_PIN);
  
  if (reading != lastRecButtonState) {
    lastRecDebounceTime = millis();
  }
  
  if ((millis() - lastRecDebounceTime) > debounceDelay) {
    // Button press detected
    if (reading == LOW && lastRecButtonState == HIGH) {
      processRecButtonPress();
    }
  }
  
  lastRecButtonState = reading;
}

void processScanButtonPress() {
  // Button action depends on the current state
  if (!cameraController.isConnected()) {
    // Start scanning or connect
    Serial.println("Scan button pressed - scanning for camera");
    cameraController.scan(10); // Scan for 10 seconds
  } else {
    // If already connected, cycle display modes
    uint8_t newMode = (cameraController.getDisplayMode() + 1) % 4;
    cameraController.setDisplayMode(newMode);
    Serial.print("Scan button pressed - display mode switched to: ");
    Serial.println(newMode);
  }
}

void processRecButtonPress() {
  // Only act if connected
  if (cameraController.isConnected()) {
    // Toggle recording
    Serial.println("Rec button pressed - toggling recording state");
    cameraController.toggleRecording();
  } else {
    // Try to connect if camera was found but not connected
    Serial.println("Rec button pressed - connecting to camera");
    cameraController.connect();
  }
}

// Callback function for parameter updates
void parameterCallback(uint8_t category, uint8_t parameterId, String value) {
  // Only log important parameters to reduce serial output
  if (category == BMD_CAT_TRANSPORT && parameterId == BMD_PARAM_TRANSPORT_MODE) {
    Serial.print("Transport mode updated: ");
    Serial.println(value);
    
    // Highlight recording changes
    if (value.indexOf("Record") != -1) {
      Serial.println("=== RECORDING STARTED ===");
    } else if (value.indexOf("Preview") != -1) {
      Serial.println("=== RECORDING STOPPED ===");
    }
  } 
  else if (category == BMD_CAT_EXTENDED_LENS && parameterId == BMD_PARAM_LENS_CLIP_FILENAME) {
    Serial.print("Current clip: ");
    Serial.println(value);
  }
}
