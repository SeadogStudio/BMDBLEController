#include <BMDBLEController.h>
#include <NimBLEDevice.h>

#define SERVICE_UUID "291d567a-6d75-11e6-8b77-86f30ca893d3"

BMDBLEController camera;
bool doConnect = false;
NimBLEAdvertisedDevice* foundDevice = nullptr;

class MyAdvertisedDeviceCallbacks : public NimBLE::NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override { // Corrected
        Serial.printf("Advertised Device Found: %s\n", advertisedDevice->toString().c_str());

        if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(NimBLEUUID(SERVICE_UUID))) {
            Serial.println("Found Blackmagic Camera!");
            NimBLEDevice::getScan()->stop();
            foundDevice = advertisedDevice;
            doConnect = true;
        }
    }
};

void incomingDataCallback(uint8_t* data, size_t length) {
    Serial.print("Incoming Data: ");
    for (int i = 0; i < length; i++) {
        Serial.printf("%02X ", data[i]);
    }
    Serial.println();
}

void statusCallback(uint8_t status) {
    Serial.print("Camera Status: 0x");
    Serial.println(status, HEX);
    if (status & 0x01) {
        Serial.println("Camera is ON");
    }
    if (status & 0x02) {
        Serial.println("Camera is Connected");
    }
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
    Serial.println("Starting BLE scan...");

    camera.begin();
    camera.setIncomingDataCallback(incomingDataCallback);
    camera.setStatusCallback(statusCallback);

    NimBLEScan* pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->start(5, new MyAdvertisedDeviceCallbacks(), false); // Scan for 5 seconds, non-blocking.
}

void loop() {
    if (doConnect) {
        doConnect = false;
        if (camera.connectToCamera(foundDevice)) {
            Serial.println("Connection successful!");
            delay(2000);
            camera.setAperture(5.6f); // Example command
        } else {
            Serial.println("Connection failed.");
        }
    }
    delay(10);
}
