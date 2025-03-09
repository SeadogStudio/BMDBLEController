/**
 * BMDBLEController - Basic Example
 * 
 * This example demonstrates how to use the BMDBLEController library
 * to connect to a Blackmagic camera via BLE and display information
 * on an OLED display. It manages camera connection, reconnection,
 * and shows how to control basic camera functions.
 * 
 * Hardware:
 * - ESP32 development board
 * - SSD1306 OLED display (128x64 pixels) on I2C
 * - Pushbutton connected to pin 0 (builtin button on most ESP32 dev boards)
 * 
 * Connection:
 * - OLED SDA: GPIO21
 * - OLED SCL: GPIO22
 * - OLED VCC: 3.3V
 * - OLED GND: GND
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

// Button pin
#define BUTTON_PIN 0

// Create BMDBLEController instance
BMDBLEController cameraController("ESP32BMDControl");

// Button state
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Display variables
uint8_t displayMode = BMD_DISPLAY_BASIC;
bool scanning = false;

void setup() {
  // Initialize serial
  Serial.begin(115200);
  Serial.println("BMDBLEController Basic Example");
  
  // Initialize button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true); // Don't proceed
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("BMDBLEController");
  display.println("Starting up...");
  display.display();
  delay(2000);
  
  // Initialize the BMDBLEController with the display
  cameraController.begin(&display);
  
  // Register parameter callback
  cameraController.setResponseCallback(parameterCallback);

  Serial.println("Ready! Press button to scan for camera");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("BMDBLEController");
  display.println("Press button to scan");
  display.display();
}

void loop() {
  // Handle button press
  handleButton();
  
  // Run library loop
  cameraController.loop();
  
  // Short delay
  delay(10);
}

void handleButton() {
  // Read button state with debounce
  int reading = digitalRead(BUTTON_PIN);
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Button press detected
    if (reading == LOW && lastButtonState == HIGH) {
      processButtonPress();
    }
  }
  
  lastButtonState = reading;
}

void processButtonPress() {
  // Button action depends on the current state
  if (!cameraController.isConnected()) {
    if (!scanning) {
      // Start scanning
      Serial.println("Button pressed - starting scan");
      cameraController.scan(10); // Scan for 10 seconds
      scanning = true;
    } else {
      // Try to connect if a device was found
      Serial.println("Button pressed - connecting");
      cameraController.connect();
      scanning = false;
    }
  } else {
    // Cycle through display modes
    displayMode = (displayMode + 1) % 4; // 4 display modes (0-3)
    cameraController.setDisplayMode(displayMode);
    Serial.print("Button pressed - display mode switched to: ");
    Serial.println(displayMode);
  }
}

// Callback function for parameter updates
void parameterCallback(uint8_t category, uint8_t parameterId, String value) {
  Serial.print("Parameter updated - Cat: 0x");
  Serial.print(category, HEX);
  Serial.print(" Param: 0x");
  Serial.print(parameterId, HEX);
  Serial.print(" Value: ");
  Serial.println(value);
  
  // Handle specific parameters if needed
  if (category == BMD_CAT_LENS && parameterId == BMD_PARAM_FOCUS) {
    // Focus was updated
    Serial.println("Focus updated");
  } else if (category == BMD_CAT_VIDEO && parameterId == BMD_PARAM_WB) {
    // White balance was updated
    Serial.println("White balance updated");
  } else if (category == BMD_CAT_TRANSPORT && parameterId == 0x01) {
    // Transport mode updated
    if (value.indexOf("Record") != -1) {
      Serial.println("Camera started recording");
    } else if (value.indexOf("Preview") != -1) {
      Serial.println("Camera stopped recording");
    }
  }
}
