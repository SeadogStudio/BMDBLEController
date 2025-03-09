/**
 * BMDBLEController - Focus Control Example
 * 
 * This example demonstrates how to use the BMDBLEController library
 * to control camera focus. It connects to a Blackmagic camera via BLE
 * and allows focus control using a potentiometer. It also shows lens
 * information on an OLED display.
 * 
 * Hardware:
 * - ESP32 development board
 * - SSD1306 OLED display (128x64 pixels) on I2C
 * - Potentiometer connected to pin 34 (ADC)
 * - Pushbutton connected to pin 0 (builtin button on most ESP32 dev boards)
 * 
 * Connection:
 * - OLED SDA: GPIO21
 * - OLED SCL: GPIO22
 * - OLED VCC: 3.3V
 * - OLED GND: GND
 * - Potentiometer: GPIO34 (analog input)
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
// Potentiometer pin
#define POT_PIN 34

// Create BMDBLEController instance
BMDBLEController cameraController("ESP32FocusControl");

// Button state
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Focus control variables
int lastPotValue = 0;
unsigned long lastFocusUpdate = 0;
const unsigned long FOCUS_UPDATE_INTERVAL = 100; // Update focus every 100ms
bool focusControlEnabled = false;

void setup() {
  // Initialize serial
  Serial.begin(115200);
  Serial.println("BMDBLEController Focus Control Example");
  
  // Initialize button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize analog pin for potentiometer
  pinMode(POT_PIN, INPUT);
  
  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true); // Don't proceed
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Focus Control");
  display.println("Starting up...");
  display.display();
  delay(2000);
  
  // Initialize the BMDBLEController with the display
  cameraController.begin(&display);
  
  // Set to focus display mode
  cameraController.setDisplayMode(BMD_DISPLAY_FOCUS);
  
  // Start scanning immediately
  cameraController.scan(10);
  Serial.println("Scanning for Blackmagic camera...");
}

void loop() {
  // Handle button press
  handleButton();
  
  // Handle focus control if enabled
  if (focusControlEnabled && cameraController.isConnected()) {
    handleFocusControl();
  }
  
  // Run library loop
  cameraController.loop();
  
  // Short delay
  delay(10);
}

void handleButton() {
  // Read button state with debounce
  int reading = digitalRead(BUTTON_
