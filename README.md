# BMDBLEController

A library for controlling Blackmagic Design cameras via Bluetooth Low Energy (BLE) with an ESP32.

## Features

- Automated scan → connect → bond sequence
- Reliable connection handling with auto-reconnection
- Parameter storage for all camera settings
- Display integration with SSD1306 OLED
- Camera control methods:
  - Focus control (both raw and normalized values)
  - Iris/aperture control
  - White balance adjustment
  - Recording start/stop
  - Auto focus
- Different display modes:
  - Basic info display
  - Focus control with visual indicator
  - Recording status with timecode
  - Debug mode for protocol exploration

## Requirements

### Hardware
- ESP32 development board
- SSD1306 OLED display (128x64 pixels, I2C)
- Blackmagic Design camera with Bluetooth capability

### Software Dependencies
- Arduino IDE 1.8.x or 2.x.x
- [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit SSD1306 Library](https://github.com/adafruit/Adafruit_SSD1306)
- ESP32 Arduino core

## Installation

1. Download the library as a ZIP file
2. In Arduino IDE, go to Sketch > Include Library > Add .ZIP Library...
3. Select the downloaded ZIP file
4. Restart Arduino IDE

## Wiring

### OLED Display
- SDA: GPIO21
- SCL: GPIO22
- VCC: 3.3V
- GND: GND

## Usage

```cpp
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BMDBLEController.h>

// Initialize OLED display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Create BMDBLEController instance
BMDBLEController cameraController("ESP32BMDControl");

void setup() {
  Serial.begin(115200);
  
  // Initialize display
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  
  // Initialize controller with display
  cameraController.begin(&display);
  
  // Start scanning for cameras
  cameraController.scan(10); // Scan for 10 seconds
}

void loop() {
  // Required to handle connection and update display
  cameraController.loop();
  
  // Check if connected
  if (cameraController.isConnected()) {
    // Control camera functions
    cameraController.setFocus(0.5);  // Set focus to middle position
    cameraController.setWhiteBalance(5600); // Daylight white balance
    
    // Toggle recording
    if (!cameraController.isRecording()) {
      cameraController.toggleRecording();
    }
  }
  
  delay(10);
}
```

## Examples

The library comes with several example sketches:

1. **BasicExample**: A simple example showing how to connect to a camera and display information.
2. **FocusControl**: Control camera focus using a potentiometer.
3. **RecordingControl**: Toggle recording and monitor timecode.
4. **CameraExplorer**: Explore camera parameters and protocol details.

## Documentation

### Main Class

#### BMDBLEController

```cpp
BMDBLEController(String deviceName = "ESP32BMDControl");
```
Constructor. Creates a new BMDBLEController instance with the specified device name.

```cpp
void begin();
void begin(Adafruit_SSD1306* display);
```
Initialize the controller, optionally with an OLED display.

```cpp
bool scan(uint32_t duration = 10);
```
Scan for Blackmagic cameras for the specified duration (in seconds).

```cpp
bool connect();
```
Connect to a previously found camera.

```cpp
bool disconnect();
```
Disconnect from the camera.

```cpp
bool isConnected();
```
Check if connected to a camera.

```cpp
void clearBondingInfo();
```
Clear saved bonding information for pairing.

```cpp
void setAutoReconnect(bool enabled);
```
Enable or disable automatic reconnection.

```cpp
bool setFocus(uint16_t rawValue);
bool setFocus(float normalizedValue);
```
Set focus using raw (0-2048) or normalized (0.0-1.0) value.

```cpp
bool setIris(float normalizedValue);
```
Set aperture/iris using normalized value (0.0-1.0).

```cpp
bool setWhiteBalance(uint16_t kelvin);
```
Set white balance temperature in Kelvin.

```cpp
bool toggleRecording();
```
Toggle recording state.

```cpp
bool doAutoFocus();
```
Trigger camera auto focus.

```cpp
bool isRecording();
```
Check if the camera is currently recording.

```cpp
bool requestParameter(uint8_t category, uint8_t parameterId, uint8_t dataType);
```
Request a specific parameter from the camera.

```cpp
String getParameterValue(uint8_t category, uint8_t parameterId);
```
Get the value of a parameter as a string.

```cpp
bool hasParameter(uint8_t category, uint8_t parameterId);
```
Check if a parameter exists and has a valid value.

```cpp
void updateDisplay();
```
Manually update the OLED display.

```cpp
void setDisplayMode(uint8_t mode);
```
Set the display mode (BMD_DISPLAY_BASIC, BMD_DISPLAY_FOCUS, BMD_DISPLAY_RECORDING, BMD_DISPLAY_DEBUG).

```cpp
void setResponseCallback(void (*callback)(uint8_t, uint8_t, String));
```
Set a callback function for parameter responses.

```cpp
void loop();
```
Process events and update state. Must be called regularly in the main loop.

## Protocol Information

This library implements the Blackmagic Camera Control Protocol over BLE, as documented by Blackmagic Design. The protocol uses a categorized parameter system with specific UUIDs for service and characteristics.

### UUIDs

- **Service UUID**: 291d567a-6d75-11e6-8b77-86f30ca893d3
- **Outgoing Control UUID**: 5DD3465F-1AEE-4299-8493-D2ECA2F8E1BB
- **Incoming Control UUID**: B864E140-76A0-416A-BF30-5876504537D9
- **Timecode UUID**: 6D8F2110-86F1-41BF-9AFB-451D87E976C8
- **Camera Status UUID**: 7FE8691D-95DC-4FC5-8ABD-CA74339B51B9
- **Device Name UUID**: FFAC0C52-C9FB-41A0-B063-CC76282EB89C

### Parameter Categories

The protocol organizes parameters by category (e.g., Lens, Video, Audio) and parameter ID. Constants for these are defined in `BMDBLEConstants.h`.

## License

This library is released under the MIT License. See the LICENSE file for details.

## Credits

This library was developed based on the Blackmagic Camera Control Protocol documentation and experimentation with Blackmagic cameras.
