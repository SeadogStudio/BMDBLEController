#include "BMDBLEController.h"

static BLEUUID bmdServiceUUID(BMD_SERVICE_UUID);
static BLEUUID bmdCharacteristicUUID(BMD_CHARACTERISTIC_UUID);

class MyClientCallback : public BLEClientCallbacks {
    BMDBLEController* bmdController; // Pointer to the BMDBLEController instance

public:
    MyClientCallback(BMDBLEController* controller) : bmdController(controller) {}

    void onConnect(BLEClient* pclient) {
        if (bmdController) {
            bmdController->connected = true;  // Update connected state here
            if (bmdController->onConnectCallback) {
                bmdController->onConnectCallback();
            }
        }
    }

    void onDisconnect(BLEClient* pclient) {
        if (bmdController) {
            bmdController->connected = false;  // Update connected state here
            if (bmdController->onDisconnectCallback) {
                bmdController->onDisconnectCallback();
            }
        }
    }
};


BMDBLEController::BMDBLEController() : connected(false), pServerAddress(nullptr), pRemoteCharacteristic(nullptr), pClient(nullptr),
                                        onConnectCallback(nullptr), onDisconnectCallback(nullptr), onDataReceivedCallback(nullptr)
{
    BLEDevice::init("");
}


bool BMDBLEController::connect(const char* cameraAddress) {
    if (isConnected()) {
        Serial.println("Already connected!");
        return true; // Or false, depending on desired behavior
    }

    if (cameraAddress == nullptr || strlen(cameraAddress) == 0) {
        Serial.println("Error: Invalid camera address.");
        return false;
    }

    BLEAddress addr(cameraAddress);
    pServerAddress = new BLEAddress(addr);
    doConnect = true;

    if (!connectToServer())
    {
        Serial.println("Failed to connect to the camera.");
        doConnect = false;
        return false;
    }
    return true;
}

void BMDBLEController::disconnect() {
    if (pClient) {
        if (pClient->isConnected()) {
            pClient->disconnect();
        }
        //  No need to delete. BLEDevice::deinit() is never called, so we let the stack deinit.
        // delete pClient; 
        // pClient = nullptr;
    }
    connected = false; // Immediately mark as disconnected
}

bool BMDBLEController::isConnected() const {
    return connected;
}
bool BMDBLEController::startScan(uint32_t scanDuration) {
    BLEScan* pBLEScan = BLEDevice::getScan(); //create new scan
	pBLEScan->setActiveScan(true); //active scan uses more power, but gets results faster
	BLEScanResults foundDevices = pBLEScan->start(scanDuration);
	Serial.print("Devices found: ");
	Serial.println(foundDevices.getCount());
	Serial.println("Scan done!");
    return true;
}

bool BMDBLEController::connectToServer() {
    if (pClient == nullptr) {
          pClient = BLEDevice::createClient();
    }
  
    if (!pClient) {
        Serial.println("Failed to create BLE client.");
        return false;
    }
    pClient->setClientCallbacks(new MyClientCallback(this));

    // Connect to the BLE Server.
    if (!pClient->connect(*pServerAddress)) {
          return false;
    }
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(bmdServiceUUID);
    if (pRemoteService == nullptr) {
        Serial.print("Failed to find our service UUID: ");
        Serial.println(bmdServiceUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(bmdCharacteristicUUID);
    if (pRemoteCharacteristic == nullptr) {
        Serial.print("Failed to find our characteristic UUID: ");
        Serial.println(bmdCharacteristicUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Found our characteristic");

    // Read the value of the characteristic.
    if (pRemoteCharacteristic->canRead()) {
        std::string value = pRemoteCharacteristic->readValue();
        Serial.print("The characteristic value was: ");
        Serial.println(value.c_str());
    }

    if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    }

    return true;
}

void BMDBLEController::notifyCallback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify) {

    //Serial.print("Notify callback for characteristic ");
    //Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    //Serial.print(" of data length ");
    //Serial.println(length);
    //Serial.print("data: ");
    //Serial.println((char*)pData);
    if (isNotify && length > 0) {
      //  Serial.print("Notification received: ");
        for (int i = 0; i < length; i++) {
      //      Serial.print(pData[i], HEX);
      //      Serial.print(" ");
        }
      //  Serial.println();
    }
}

bool BMDBLEController::sendCommand(uint16_t command, uint16_t parameter) {
    if (!isConnected()) {
        Serial.println("Error: Not connected.  Cannot send command.");
        return false;
    }

    if (!pRemoteCharacteristic) {
        Serial.println("Error: Characteristic not found.");
        return false;
    }

    uint8_t payload[5];
    payload[0] = 0x01; // Length of the command (including length byte)
    payload[1] = command & 0xFF;
    payload[2] = (command >> 8) & 0xFF;
    payload[3] = parameter & 0xFF;
    payload[4] = (parameter >> 8) & 0xFF;
    payload[0] = sizeof(payload); // Length of the command +1 (including length byte)


    // Check if characteristic is valid AND supports writing
    if (pRemoteCharacteristic->canWrite()) {
	    pRemoteCharacteristic->writeValue(payload, sizeof(payload), false);
        return true;
    } else {
		Serial.println("Error: Characteristic does not support writing.");
        return false;
	}

}


// Higher-level command implementations (examples)
bool BMDBLEController::startRecording() {
    return sendCommand(0x0202, 0x0001); //  Start recording
}

bool BMDBLEController::stopRecording() {
    return sendCommand(0x0202, 0x0000); // Stop recording
}

bool BMDBLEController::setISO(uint16_t isoValue) {
      //0x0000: Auto
      //0x0064:  100
      //0x00C8:  200
      //0x0190:  400
      //0x0320:  800
      //0x0640: 1600
      //0x0C80: 3200
    return sendCommand(0x0301, isoValue);
}

bool BMDBLEController::setShutterAngle(float angle) {
        //0x0000: Auto
        //0x002D: 45.0°
        //0x005A: 90.0°
        //0x00B4: 180.0°
        //0x0168: 360.0°
    uint16_t angleValue = static_cast<uint16_t>(angle * 64 / 9); //angle * (0xFFFF / 360.f)
    return sendCommand(0x0303, angleValue);

}

bool BMDBLEController::setAperture(float apertureValue) {
    //0x0000: Auto
    //0x0038:   1.0
    //0x0042:   1.4
    //0x0044:   1.7
    //0x004B:   2.0
    //0x004F:   2.2
    //0x0053:   2.5
    //0x0058:   2.8
    //0x005C:   3.2
    //0x0060:   3.5
    //0x0063:   4.0
    //0x0067:   4.5
    //0x006B:   5.0
    //0x006F:   5.6
    //0x0075:   6.3
    //0x007A:   7.1
    //0x007F:   8.0
    //0x0088:   9.0
    //0x008C:  10.0
    //0x0090:  11.0
    //0x0096:  13.0
    //0x009B:  14.0
    //0x009F:  16.0
    //0x00A5:  18.0
    //0x00A9:  20.0
    //0x00AD:  22.0
    uint16_t apertureSetParameter = 0;
    if (apertureValue == 1.0) apertureSetParameter = 0x0038;
    else if (apertureValue == 1.4) apertureSetParameter = 0x0042;
    else if (apertureValue == 1.7) apertureSetParameter = 0x0044;
    else if (apertureValue == 2.0) apertureSetParameter = 0x004B;
    else if (apertureValue == 2.2) apertureSetParameter = 0x004F;
    else if (apertureValue == 2.5) apertureSetParameter = 0x0053;
    else if (apertureValue == 2.8) apertureSetParameter = 0x0058;
    else if (apertureValue == 3.2) apertureSetParameter = 0x005C;
    else if (apertureValue == 3.5) apertureSetParameter = 0x0060;
    else if (apertureValue == 4.0) apertureSetParameter = 0x0063;
    else if (apertureValue == 4.5) apertureSetParameter = 0x0067;
    else if (apertureValue == 5.0) apertureSetParameter = 0x006B;
    else if (apertureValue == 5.6) apertureSetParameter = 0x006F;
    else if (apertureValue == 6.3) apertureSetParameter = 0x0075;
    else if (apertureValue == 7.1) apertureSetParameter = 0x007A;
    else if (apertureValue == 8.0) apertureSetParameter = 0x007F;
    else if (apertureValue == 9.0) apertureSetParameter = 0x0088;
    else if (apertureValue == 10.0) apertureSetParameter = 0x008C;
    else if (apertureValue == 11.0) apertureSetParameter = 0x0090;
    else if (apertureValue == 13.0) apertureSetParameter = 0x0096;
    else if (apertureValue == 14.0) apertureSetParameter = 0x009B;
    else if (apertureValue == 16.0) apertureSetParameter = 0x009F;
    else if (apertureValue == 18.0) apertureSetParameter = 0x00A5;
    else if (apertureValue == 20.0) apertureSetParameter = 0x00A9;
    else if (apertureValue == 22.0) apertureSetParameter = 0x00AD;
    else  apertureSetParameter = 0;

    return sendCommand(0x0305, apertureSetParameter);
}

bool BMDBLEController::setWhiteBalance(uint16_t whiteBalanceValue) {
   //0x0000: Auto
   //0x07D0: 2000
   //0x09C4: 2500
   //0x0BB8: 3000
   //0x0C80: 3200
   //0x0D48: 3400
   //0x0E10: 3600
   //0x0ED8: 3800
   //0x0FA0: 4000
   //0x1068: 4200
   //0x1130: 4400
   //0x11F8: 4600
   //0x12C0: 4800
   //0x1388: 5000
   //0x1450: 5200
   //0x1518: 5400
   //0x15E0: 5600
   //0x16A8: 5800
   //0x1770: 6000
   //0x1838: 6200
   //0x1900: 6400
   //0x19C8: 6600
   //0x1A90: 6800
   //0x1B58: 7000
   //0x1C20: 7200
   //0x1CE8: 7400
   //0x1DB0: 7600
   //0x1E78: 7800
   //0x1F40: 8000
   //0x2008: 8200
   //0x20D0: 8400
   //0x2198: 8600
   //0x2260: 8800
   //0x2328: 9000
   //0x23F0: 9200
   //0x24B8: 9400
   //0x2580: 9600
   //0x2648: 9800
    return sendCommand(0x0308, whiteBalanceValue);
}

bool BMDBLEController::setNDFilter(uint8_t ndFilterValue) { //ND filters are integer.
    //0: Clear
    //1: 2 stops
    //2: 4 stops
    //3: 6 stops
    return sendCommand(0x030c, ndFilterValue);
}
bool BMDBLEController::getBatteryLevel()
{
    if (!isConnected()) {
        Serial.println("Error: Not connected.  Cannot send command.");
        return false;
    }

    if (!pRemoteCharacteristic) {
        Serial.println("Error: Characteristic not found.");
        return false;
    }

    // Check if characteristic is valid AND supports reading
    if (pRemoteCharacteristic->canRead()) {
        std::string value = pRemoteCharacteristic->readValue();
        // Parse the battery level from the value string (you'll need to figure out the format)
        // For example, if the value is a single byte representing the percentage:
        if (value.length() > 0) {
            uint8_t batteryLevel = value[0];
            Serial.print("Battery Level: ");
            Serial.print(batteryLevel);
            Serial.println("%");
          return true;
        } else {
            Serial.println("Error: Empty battery level response.");
            return false;
        }
    } else {
        Serial.println("Error: Characteristic does not support reading.");
        return false;
    }
}
