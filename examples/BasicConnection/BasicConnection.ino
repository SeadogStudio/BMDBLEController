#include <BMDBLEController.h>
#include <NimBLEDevice.h> // Use NimBLE

// Replace with your camera's advertised name if you know it, otherwise use service UUID filtering.
// Using the name is generally NOT recommended for production, as it's not unique.
// #define CAMERA_NAME "Blackmagic Camera" // Example name.  This is NOT reliable.
#define SERVICE_UUID "291d567a-6d75-11e6-8b77-86f30ca893d3" //Blackmagic Camera Service

BMDBLEController camera; // Use the new class name
bool doConnect = false;
NimBLEAdvertisedDevice* foundDevice; // Use NimBLEAdvertisedDevice

// Callback for when a BLE device is found.
class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) { // Use pointer
        Serial.printf("Advertised Device: %s \n", advertisedDevice->toString().c_str());

        // Check for the Blackmagic Camera Service UUID.  This is more reliable than the name.
        if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(NimBLEUUID(SERVICE_UUID))) {
            Serial.println("Found Blackmagic Camera!");
            NimBLEDevice::getScan()->stop(); // Stop scanning once we find the camera
            foundDevice = advertisedDevice;    // Store the device pointer directly
            doConnect = true;                // Set the flag to connect in the loop()
        }
    }
};

// Callback for incoming data from the camera.
void incomingDataCallback(uint8_t* data, size_t length) {
    Serial.print("Incoming Data: ");
    for (int i = 0; i < length; i++) {
        Serial.printf("%02X ", data[i]); // Print each byte in hexadecimal format
    }
    Serial.println();

    // In a real application, you would parse this data according to the
    // Blackmagic SDI Camera Control Protocol. This example just prints the raw bytes.
}

// Callback for camera status updates.
void statusCallback(uint8_t status) {
    Serial.print("Camera Status: 0x");
    Serial.println(status, HEX); // Print the status in hexadecimal
    if (status & 0x01) {
        Serial.println("Camera is ON");
    }
    if (status & 0x02) {
        Serial.println("Camera is Connected");
    }
    // Don't check for 0x04 (paired) here.  Check _bonded in the BMDBLEController.
    if (status & 0x08) {
        Serial.println("Versions Verified");
    }
    if (status & 0x10) {
        Serial.println("Initial Payload Received");
    }
    if (status & 0x20) {
        Serial.println("Camera Ready");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    camera.begin(); // Initialize the BMDBLEController
    camera.setIncomingDataCallback(incomingDataCallback); // Set the data callback
    camera.setStatusCallback(statusCallback); // Set the status callback

    NimBLEScan* pBLEScan = NimBLEDevice::getScan(); // Get the BLEScan object (NimBLE)
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); // Set the device discovery callback
    pBLEScan->setActiveScan(true); // Active scan uses more power, but gets results faster
    pBLEScan->setInterval(100); // Set scan interval to 100ms
    pBLEScan->setWindow(99);  // Must be less or equal than interval
     pBLEScan->start(5, false); // Start scanning for 5 seconds (non-blocking)

}

void loop() {
    if (doConnect) { // If a device was found
        if (camera.connectToCamera(foundDevice)) {
            Serial.println("Connect successful");
        } else {
            Serial.println("Failed to connect to camera");
        }
        doConnect = false; // Reset the connection flag
        // Don't delete foundDevice here; NimBLE manages it.
    }

    if (camera.isConnected()) {
        // Example:  Send a command after a delay (e.g., set aperture)
        // You can add more commands here, or trigger them based on
        // other events (button presses, sensor readings, etc.).
        delay(2000); // Wait 2 seconds after connecting
        if (!camera.setAperture(5.6f)) { // Example: Set aperture to f/5.6
            Serial.println("Failed to set aperture");
        }
        camera.disconnect();
    }

    delay(10); // Don't flood the serial port or the BLE connection.
}
