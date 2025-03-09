/**
 * BMDBLEController - Camera Explorer Example
 * 
 * This example demonstrates how to explore and debug Blackmagic camera
 * parameters. It connects to a camera via BLE and allows you to request
 * specific parameters using the serial monitor. It also displays current
 * parameter values on an OLED display.
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
 * 
 * Serial Commands:
 * - scan             : Scan for cameras
 * - connect          : Connect to found camera
 * - status           : Show connection status
 * - param XX YY      : Request parameter with category XX and ID YY (hex)
 * - mode N           : Set display mode (0-3)
 * - list             : List all received parameters
 * - focus X.X        : Set focus to normalized value (0.0-1.0)
 * - iris X.X         : Set iris/aperture to normalized value (0.0-1.0)
 * - wb XXXX          : Set white balance to XXXX Kelvin (2500-10000)
 * - record           : Toggle recording
 * - af               : Trigger auto focus
 * - help             : Show available commands
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
BMDBLEController cameraController("ESP32CameraExplorer");

// Button state
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Parameter tracking
struct ReceivedParameter {
  uint8_t category;
  uint8_t parameterId;
  uint8_t dataType;
  String value;
  unsigned long timestamp;
};

#define MAX_TRACKED_PARAMS 16
ReceivedParameter receivedParams[MAX_TRACKED_PARAMS];
int paramCount = 0;

void setup() {
  // Initialize serial
  Serial.begin(115200);
  Serial.println("\nBMDBLEController Camera Explorer Example");
  
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
  display.println("Camera Explorer");
  display.println("Starting up...");
  display.display();
  delay(2000);
  
  // Initialize the BMDBLEController with the display
  cameraController.begin(&display);
  
  // Set to debug display mode
  cameraController.setDisplayMode(BMD_DISPLAY_DEBUG);
  
  // Register parameter callback
  cameraController.setResponseCallback(parameterCallback);
  
  // Display help
  printHelp();
}

void loop() {
  // Handle button press
  handleButton();
  
  // Handle serial commands
  handleSerialCommands();
  
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
    // Start scanning or connect
    Serial.println("Button pressed - scanning for camera");
    cameraController.scan(10); // Scan for 10 seconds
  } else {
    // If already connected, cycle display modes
    uint8_t newMode = (cameraController.getDisplayMode() + 1) % 4;
    cameraController.setDisplayMode(newMode);
    Serial.print("Button pressed - display mode switched to: ");
    Serial.println(newMode);
  }
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "scan") {
      Serial.println("Starting scan for Blackmagic cameras...");
      cameraController.scan(10);
    }
    else if (command == "connect") {
      Serial.println("Connecting to camera...");
      cameraController.connect();
    }
    else if (command == "status") {
      printStatus();
    }
    else if (command.startsWith("param ")) {
      handleParamCommand(command);
    }
    else if (command.startsWith("mode ")) {
      handleModeCommand(command);
    }
    else if (command == "list") {
      listAllParameters();
    }
    else if (command.startsWith("focus ")) {
      handleFocusCommand(command);
    }
    else if (command.startsWith("iris ")) {
      handleIrisCommand(command);
    }
    else if (command.startsWith("wb ")) {
      handleWBCommand(command);
    }
    else if (command == "record") {
      Serial.println("Toggling recording state...");
      cameraController.toggleRecording();
    }
    else if (command == "af") {
      Serial.println("Triggering auto focus...");
      cameraController.doAutoFocus();
    }
    else if (command == "help") {
      printHelp();
    }
    else {
      Serial.println("Unknown command. Type 'help' for available commands.");
    }
  }
}

void handleParamCommand(String command) {
  // Parse "param XX YY" where XX and YY are hex values
  int firstSpace = command.indexOf(' ');
  int secondSpace = command.indexOf(' ', firstSpace + 1);
  
  if (secondSpace > 0) {
    String catStr = command.substring(firstSpace + 1, secondSpace);
    String paramStr = command.substring(secondSpace + 1);
    
    // Convert hex strings to integers
    uint8_t category = strtol(catStr.c_str(), NULL, 16);
    uint8_t parameterId = strtol(paramStr.c_str(), NULL, 16);
    
    // Determine data type from category and parameter
    uint8_t dataType = getDataType(category, parameterId);
    
    Serial.print("Requesting parameter - Cat: 0x");
    Serial.print(category, HEX);
    Serial.print(" Param: 0x");
    Serial.print(parameterId, HEX);
    Serial.print(" (DataType: 0x");
    Serial.print(dataType, HEX);
    Serial.println(")");
    
    cameraController.requestParameter(category, parameterId, dataType);
  } else {
    Serial.println("Invalid format. Use: param XX YY (where XX and YY are hex values)");
  }
}

void handleModeCommand(String command) {
  // Parse "mode N" where N is a display mode (0-3)
  int space = command.indexOf(' ');
  if (space > 0) {
    String modeStr = command.substring(space + 1);
    int mode = modeStr.toInt();
    
    if (mode >= 0 && mode <= 3) {
      Serial.print("Setting display mode to: ");
      Serial.println(mode);
      cameraController.setDisplayMode(mode);
    } else {
      Serial.println("Invalid mode. Use 0-3.");
    }
  }
}

void handleFocusCommand(String command) {
  // Parse "focus X.X" where X.X is a normalized focus value (0.0-1.0)
  int space = command.indexOf(' ');
  if (space > 0) {
    String valueStr = command.substring(space + 1);
    float value = valueStr.toFloat();
    
    if (value >= 0.0 && value <= 1.0) {
      Serial.print("Setting focus to: ");
      Serial.println(value, 3);
      cameraController.setFocus(value);
    } else {
      Serial.println("Invalid focus value. Use 0.0-1.0.");
    }
  }
}

void handleIrisCommand(String command) {
  // Parse "iris X.X" where X.X is a normalized iris value (0.0-1.0)
  int space = command.indexOf(' ');
  if (space > 0) {
    String valueStr = command.substring(space + 1);
    float value = valueStr.toFloat();
    
    if (value >= 0.0 && value <= 1.0) {
      Serial.print("Setting iris to: ");
      Serial.println(value, 3);
      cameraController.setIris(value);
    } else {
      Serial.println("Invalid iris value. Use 0.0-1.0.");
    }
  }
}

void handleWBCommand(String command) {
  // Parse "wb XXXX" where XXXX is white balance in Kelvin (2500-10000)
  int space = command.indexOf(' ');
  if (space > 0) {
    String valueStr = command.substring(space + 1);
    int value = valueStr.toInt();
    
    if (value >= 2500 && value <= 10000) {
      Serial.print("Setting white balance to: ");
      Serial.print(value);
      Serial.println("K");
      cameraController.setWhiteBalance(value);
    } else {
      Serial.println("Invalid white balance value. Use 2500-10000K.");
    }
  }
}

void printStatus() {
  Serial.println("\n----- Camera Status -----");
  Serial.print("Connected: ");
  Serial.println(cameraController.isConnected() ? "YES" : "NO");
  
  if (cameraController.isConnected()) {
    Serial.print("Recording: ");
    Serial.println(cameraController.isRecording() ? "YES" : "NO");
    
    // List recent parameter values
    Serial.println("\nRecent parameter values:");
    
    // Lens info
    printParameterIfAvailable(BMD_CAT_EXTENDED_LENS, BMD_PARAM_LENS_MODEL, "Lens Model");
    printParameterIfAvailable(BMD_CAT_EXTENDED_LENS, BMD_PARAM_LENS_FOCAL_LENGTH, "Focal Length");
    printParameterIfAvailable(BMD_CAT_EXTENDED_LENS, BMD_PARAM_LENS_FOCUS_DISTANCE, "Focus Distance");
    
    // Camera settings
    printParameterIfAvailable(BMD_CAT_LENS, BMD_PARAM_FOCUS, "Focus");
    printParameterIfAvailable(BMD_CAT_LENS, BMD_PARAM_IRIS_NORM, "Iris");
    printParameterIfAvailable(BMD_CAT_VIDEO, BMD_PARAM_WB, "White Balance");
    printParameterIfAvailable(BMD_CAT_VIDEO, BMD_PARAM_ISO, "ISO");
    printParameterIfAvailable(BMD_CAT_VIDEO, BMD_PARAM_SHUTTER_ANGLE, "Shutter Angle");
    
    // Other status
    printParameterIfAvailable(0x09, 0x00, "Battery Voltage");
  }
  
  Serial.println("------------------------\n");
}

void printParameterIfAvailable(uint8_t category, uint8_t parameterId, const char* name) {
  if (cameraController.hasParameter(category, parameterId)) {
    String value = cameraController.getParameterValue(category, parameterId);
    Serial.print(name);
    Serial.print(": ");
    Serial.println(value);
  }
}

void listAllParameters() {
  Serial.println("\n----- All Received Parameters -----");
  
  for (int i = 0; i < paramCount; i++) {
    ReceivedParameter& param = receivedParams[i];
    
    Serial.print("Cat: 0x");
    Serial.print(param.category, HEX);
    Serial.print
