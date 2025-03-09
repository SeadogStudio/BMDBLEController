/*
 * BMDBLEController - Basic Test Example
 * 
 * This example demonstrates basic connection to a Blackmagic camera
 * via BLE and simple parameter control.
 * 
 * Hardware Requirements:
 * - ESP32 board
 * - Blackmagic Design camera with BLE support
 */

#include <BMDBLEController.h>

// Create controller instance
BMDBLEController bmdController("ESP32BasicTest");

// Connection state
bool connected = false;

// Parameter callback
void onParameterUpdate(uint8_t category, uint8_t parameterId, uint8_t* data, size_t length) {
  Serial.print("Parameter updated - Category: 0x");
  Serial.print(category, HEX);
  Serial.print(", Parameter: 0x");
  Serial.print(parameterId, HEX);
  Serial.println();
  
  // Process based on category and parameter
  if (category == BMD_CAT_EXTENDED_LENS) {
    if (parameterId == BMD_PARAM_LENS_MODEL && data != nullptr) {
      // Extract string data
      char lensName[64] = {0};
      memcpy(lensName, data, min(length, (size_t)63));
      
      Serial.print("Lens model: ");
      Serial.println(lensName);
    }
  }
}

// Connection callback
void onConnectionStateChange(BMDConnectionState state) {
  Serial.print("Connection state changed: ");
  
  switch (state) {
    case BMD_STATE_DISCONNECTED:
      Serial.println("DISCONNECTED");
      connected = false;
      break;
    case BMD_STATE_SCANNING:
      Serial.println("SCANNING");
      break;
    case BMD_STATE_CONNECTING:
      Serial.println("CONNECTING");
      break;
    case BMD_STATE_CONNECTED:
      Serial.println("CONNECTED");
      connected = true;
      
      // Request lens info after connection
      bmdController.requestParameter(BMD_CAT_EXTENDED_LENS, BMD_PARAM_LENS_MODEL, BMD_TYPE_STRING);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("BMDBLEController - Basic Test Example");
  
  // Initialize controller
  bmdController.begin();
  
  // Set callbacks
  bmdController.setParameterCallback(onParameterUpdate);
  bmdController.setConnectionCallback(onConnectionStateChange);
  
  // Start scanning for cameras
  Serial.println("Scanning for Blackmagic camera...");
  bmdController.scan(10);
  
  // Print available commands
  Serial.println("\nAvailable commands:");
  Serial.println("  c - Connect");
  Serial.println("  d - Disconnect");
  Serial.println("  f - Set focus to middle");
  Serial.println("  w - Set white balance to 5600K");
  Serial.println("  r - Toggle recording");
  Serial.println("  i - Request lens info");
  Serial.println("  s - Scan for cameras");
}

void loop() {
  // Update controller
  bmdController.loop();
  
  // Handle serial commands
  if (Serial.available()) {
    char cmd = Serial.read();
    
    switch (cmd) {
      case 'c':
        if (!bmdController.isConnected()) {
          Serial.println("Connecting to camera...");
          bmdController.connect();
        }
        break;
        
      case 'd':
        if (bmdController.isConnected()) {
          Serial.println("Disconnecting from camera...");
          bmdController.disconnect();
        }
        break;
        
      case 'f':
        if (bmdController.isConnected()) {
          Serial.println("Setting focus to middle position (0.5)...");
          bmdController.setFocus(0.5); // 50% focus
        } else {
          Serial.println("Not connected to camera");
        }
        break;
        
      case 'w':
        if (bmdController.isConnected()) {
          Serial.println("Setting white balance to 5600K...");
          bmdController.setWhiteBalance(5600);
        } else {
          Serial.println("Not connected to camera");
        }
        break;
        
      case 'r':
        if (bmdController.isConnected()) {
          Serial.println("Toggling recording state...");
          bmdController.toggleRecording();
        } else {
          Serial.println("Not connected to camera");
        }
        break;
        
      case 'i':
        if (bmdController.isConnected()) {
          Serial.println("Requesting lens information...");
          bmdController.requestParameter(BMD_CAT_EXTENDED_LENS, BMD_PARAM_LENS_MODEL, BMD_TYPE_STRING);
        } else {
          Serial.println("Not connected to camera");
        }
        break;
        
      case 's':
        Serial.println("Scanning for cameras...");
        bmdController.scan(10);
        break;
    }
  }
  
  // Small delay
  delay(10);
}
