# Blackmagic Camera BLE Controller Library

A library for controlling Blackmagic Design cameras via Bluetooth Low Energy (BLE) using ESP32 microcontrollers.

## Features

- Connect to Blackmagic cameras via BLE
- Control camera parameters:
  - Focus (raw and normalized values)
  - Iris/aperture
  - White balance (Kelvin)
- Start/stop recording
- Request parameter values
- Store and retrieve parameter data

## Hardware Requirements

- ESP32 microcontroller
- Blackmagic Design camera with BLE support

## Installation

1. Download the library as ZIP
2. In Arduino IDE go to Sketch > Include Library > Add .ZIP Library...
3. Select the downloaded ZIP file

## Usage

```cpp
#include <BMDBLEController.h>

// Create controller instance
BMDBLEController bmdController("ESP32CameraControl");

// Parameter update callback
void onParameterUpdate(uint8_t category, uint8_t parameterId, uint8_t* data, size_t length) {
  Serial.print("Parameter updated - Category: 0x");
  Serial.print(category, HEX);
  Serial.print(", Parameter: 0x");
  Serial.print(parameterId, HEX);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  
  // Initialize controller
  bmdController.begin();
  
  // Set parameter update callback
  bmdController.setParameterCallback(onParameterUpdate);
  
  // Start scanning for cameras
  bmdController.scan(10);
}

void loop() {
  // Update controller
  bmdController.loop();
  
  // Check connection
  if (bmdController.isConnected()) {
    // Control camera
    bmdController.setFocus(0.5); // 50% focus
    bmdController.setWhiteBalance(5600); // 5600K
    
    // Request parameter
    bmdController.requestParameter(BMD_CAT_EXTENDED_LENS, 0x09, BMD_TYPE_STRING); // Lens model
  }
  
  delay(10);
}
