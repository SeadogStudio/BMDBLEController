#include <BMDBLEController.h>
#include <NimBLEDevice.h> // Use NimBLE

// Use the Blackmagic Camera Service UUID for reliable identification.
#define SERVICE_UUID "291d567a-6d75-11e6-8b77-86f30ca893d3"

BMDBLEController camera;
bool doConnect = false;
NimBLEAdvertisedDevice* foundDevice = nullptr; // Store a *pointer*, not a copy

// Callback for when a BLE device is found.
class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks { // Correct inheritance
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) { // Use pointer
        Serial.printf("Advertised Device: %s \n", advertisedDevice->toString().c_str());

        // Check for the Blackmagic Camera Service UUID.
        if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(NimBLEUUID(SERVICE_UUID))) {
            Serial.println("Found Blackmagic Camera!");
            NimBLEDevice::getScan()->stop(); // Stop scanning
            foundDevice = advertisedDevice;   // Store the *pointer*
            doConnect = true;
        }
    }
};

// Callback for incoming data from the camera.
void incomingDataCallback(uint8_t* data, size_t length) {
    Serial.print("Incoming Data: ");
    for (int i = 0; i < length; i++) {
        Serial.printf("%02X ", data[i]); // Print each byte in hexadecimal
    }
    Serial.println();

    // Add your logic here to parse the incoming data.
}

// Callback for camera status updates.
void statusCallback(uint8_t status) {
    Serial.print("Camera Status: 0x");
    Serial.println(status, HEX);
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
    camera.setIncomingDataCallback(incomingDataCallback); // Set data callback
    camera.setStatusCallback(statusCallback); // Set status callback

    NimBLEScan* pBLEScan = NimBLEDevice::getScan(); // Get the scan object
    //pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); // NO - set in start()
    pBLEScan->setActiveScan(true); // Active scan uses more power, but gets results faster
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);  // Must be less or equal than interval
    pBLEScan->start(5, new MyAdvertisedDeviceCallbacks(), false); // Start scanning (non-blocking), set callback here.

}

void loop() {
    if (doConnect) {
        if (camera.connectToCamera(foundDevice)) {
            Serial.println("Connect successful");
        } else {
            Serial.println("Failed to connect to camera");
        }
        doConnect = false;
        // Do NOT delete foundDevice here. NimBLE manages it.
    }

    if (camera.isConnected()) {
        // Example:  Send a command after a delay (e.g., set aperture)
        delay(2000);
        if (!camera.setAperture(5.6f)) {
            Serial.println("Failed to set aperture");
        }
        // You can add more commands here.
    }

    delay(10); // Don't flood the serial port or the BLE connection.
}
