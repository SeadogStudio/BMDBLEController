#include "BMDBLEController.h"

// Static member initialization (no longer needed with separate callback class)
// BMDBLEController* BMDBLEController::_instance = nullptr;

// Custom callback class for NimBLE client events
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override {
        Serial.println("Connected");
        // After connecting, a new connection ID is assigned, so we should re-subscribe.
        BMDBLEController* pController = (BMDBLEController*)pClient->getUserData(); // Get the BMDBLEController instance
        if (pController) {
            pController->_connected = true;
             // Check for bonding *after* connection is established.
            if (pClient->getPeerInfo().isBonded()) {
                Serial.println("Already bonded!");
                pController->_bonded = true;
            }

            if(pController->_callbacksSet) {
                if (pController->_pIncomingCharacteristic) {
                    pController->_pIncomingCharacteristic->subscribe(true, BMDBLEController::_incomingDataNotifyCallback, true);
                }
                if (pController->_pStatusCharacteristic) {
                    pController->_pStatusCharacteristic->subscribe(true, BMDBLEController::_statusNotifyCallback, true);
                }
            }
        }
    }

    void onDisconnect(NimBLEClient* pClient) override {
        Serial.print("Disconnected - code: ");
        Serial.println(pClient->getDisconnectReason());
        BMDBLEController* pController = (BMDBLEController*)pClient->getUserData();
        if (pController) {
            pController->_connected = false;
            pController->_bonded = false;
        }
    }

    // Called when security requests are initiated or completed.
    uint32_t onPassKeyRequest() override{
        Serial.println("Client Passkey Request");
        return 123456;
    }

    bool onConfirmPIN(uint32_t pass_key) override{
        Serial.print("Confirm or reject passkey: ");
        Serial.println(pass_key);
        //  Here, you would get input from the user (e.g., buttons, keypad)
        //  to confirm if the passkey matches what's displayed on the camera.
        //  For this example, we'll auto-confirm.  Replace this with your
        //  actual input handling.
        return true;  // true indicates the passkey is correct.
    }

    void onAuthenticationComplete(ble_gap_conn_desc* desc) override{
        if(desc->sec_state.encrypted) {
            Serial.println("Encrypt connection complete - bonded");
        } else {
            Serial.println("Encrypt connection failed - not bonded");
        }
    }
};


BMDBLEController::BMDBLEController() :
  _pClient(nullptr),
  _pOutgoingCharacteristic(nullptr),
  _pIncomingCharacteristic(nullptr),
  _pStatusCharacteristic(nullptr),
  _connected(false),
  _bonded(false),
  _callbacksSet(false),
  _incomingDataCallback(nullptr),
  _statusCallback(nullptr)
{
    //_instance = this; // No longer needed
}

bool BMDBLEController::begin() {
  NimBLEDevice::init(""); // Initialize NimBLE
  NimBLEDevice::setSecurityAuth(true, true, true); // Enable bonding, MITM.  CRUCIAL for persistence.
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_DISPLAY); // ESP32 can input and display
  return true;
}

bool BMDBLEController::connectToCamera(NimBLEAdvertisedDevice* device) {
    if (_connected) {
        return false; // Already connected, prevent multiple connections
    }

    // Check if we already have a client object for this device
    _pClient = NimBLEDevice::getDisconnectedClient();
    if(!_pClient){
        _pClient = NimBLEDevice::createClient();
    }

    if (!_pClient) {
        return false; // Failed to create client
    }

    // Set connection parameters - this can improve connection reliability
    _pClient->setConnectTimeout(10); // Set connection timeout to 10 seconds (default is 30)
    _pClient->setConnectionParams(12,12,0,51); //15ms, 15ms, 0, 5.1 seconds

    // Set the connect callback using our custom class
    _pClient->setClientCallbacks(new ClientCallbacks());
    _pClient->setUserData(this); //VERY IMPORTANT

    if (!_pClient->connect(device, false)) { // Connect, don't scan again if we fail.
        NimBLEDevice::deleteClient(_pClient); // Clean up the client if connection fails.
        _pClient = nullptr;
        return false; // Failed to connect
    }

    NimBLERemoteService* pRemoteService = _pClient->getService(BLACKMAGIC_CAMERA_SERVICE_UUID);
    if (pRemoteService == nullptr) {
        _pClient->disconnect();
        return false; // Service not found
    }

    _pOutgoingCharacteristic = pRemoteService->getCharacteristic(OUTGOING_CHARACTERISTIC_UUID);
    _pIncomingCharacteristic = pRemoteService->getCharacteristic(INCOMING_CHARACTERISTIC_UUID);
    _pStatusCharacteristic = pRemoteService->getCharacteristic(CAMERA_STATUS_CHARACTERISTIC_UUID);

    if (!_pOutgoingCharacteristic || !_pIncomingCharacteristic || !_pStatusCharacteristic) {
        _pClient->disconnect();
        return false; // One or more characteristics not found
    }

     // Set callbacks if they are defined.  Crucially, do this *before* registering for notifications.
    if (_incomingDataCallback) {
        _pIncomingCharacteristic->subscribe(true, _incomingDataNotifyCallback, true); // Use subscribe() with NimBLE
    }
    if (_statusCallback) {
        _pStatusCharacteristic->subscribe(true, _statusNotifyCallback, true); // Use subscribe()
    }
    _callbacksSet = true; // Set the flag to indicate callbacks are registered
    return true;
}


void BMDBLEController::disconnect() {
  if (_connected && _pClient) {
        if(_pClient->isConnected()){
            _pClient->disconnect();
        }
        NimBLEDevice::deleteClient(_pClient);  // VERY IMPORTANT: Release resources
        _pClient = nullptr;
        _connected = false;
        _bonded = false;
        _callbacksSet = false;
  }
}

bool BMDBLEController::isConnected() {
  return _connected;
}

bool BMDBLEController::isBonded() {
  return _bonded;
}


bool BMDBLEController::sendCommand(uint8_t* command, size_t length) {
    if (!_connected || !_pOutgoingCharacteristic) {
        return false; // Not connected or characteristic not found
    }
    // Use writeValue with the 'response' parameter set to false for best performance.
    _pOutgoingCharacteristic->writeValue(command, length, false); //Corrected: return type is void
    return true; //Assume success if connected.
}

bool BMDBLEController::sendCommand(uint8_t destination, uint8_t commandId, uint8_t category, uint8_t parameter, uint8_t dataType, uint8_t operationType, uint8_t* data, size_t dataLength)
{
    if (!_connected || !_pOutgoingCharacteristic) {
        return false;
    }

    // Calculate the total command length (header + data + padding)
    size_t commandLength = 3 + dataLength; // Header (3 bytes) + Data
    size_t paddedLength = (commandLength + 3) & ~0x03; // Round up to the nearest 4-byte boundary
    uint8_t paddingBytes = paddedLength - commandLength;

    // Create a buffer for the command packet
    uint8_t* commandPacket = new uint8_t[paddedLength];

    // Construct the header
    commandPacket[0] = destination;       // Destination Device
    commandPacket[1] = (uint8_t)dataLength;      // Command Length (data length only)
    commandPacket[2] = commandId;         // Command ID
    // Byte 3 is reserved, and already 0 due to the new allocation

    // Construct command 0 data
    if (commandId == 0)
    {
        commandPacket[3] = category;        // Category
        commandPacket[4] = parameter;       // Parameter
        commandPacket[5] = dataType;        // Data Type
        commandPacket[6] = operationType;   // Operation Type
        // Copy the data
        if (data && dataLength > 0) {
            memcpy(&commandPacket[7], data, dataLength);
        }

    } else {
        // Copy the data for other commands
        if (data && dataLength > 0) {
            memcpy(&commandPacket[3], data, dataLength);
        }
    }

    // Add padding
    for (size_t i = 0; i < paddingBytes; ++i) {
        commandPacket[commandLength + i] = 0x00;
    }

    // Send the command
    //bool result = _pOutgoingCharacteristic->writeValue(commandPacket, paddedLength, false); //Corrected return type is void.
    _pOutgoingCharacteristic->writeValue(commandPacket, paddedLength, false);
    // Clean up
    delete[] commandPacket;

    return true; //Assume success if connected
}



void BMDBLEController::setIncomingDataCallback(IncomingDataCallback callback) {
    _incomingDataCallback = callback;
     // If already connected and callbacks are not yet set, set them now.
    if(_connected && !_callbacksSet && _pIncomingCharacteristic){
        _pIncomingCharacteristic->subscribe(true, _incomingDataNotifyCallback, true); // Use subscribe()
    }
}

void BMDBLEController::setStatusCallback(StatusCallback callback) {
    _statusCallback = callback;
    if(_connected && !_callbacksSet && _pStatusCharacteristic){
        _pStatusCharacteristic->subscribe(true, _statusNotifyCallback, true); // Use subscribe()
    }
}

// Static callback function for incoming data notifications (NimBLE version)
void BMDBLEController::_incomingDataNotifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    // Call the user-provided callback (if set) using the instance pointer
    if (_instance && _instance->_incomingDataCallback) {
        _instance->_incomingDataCallback(pData, length);
    }
}

// Static callback function for status notifications (NimBLE version)
void BMDBLEController::_statusNotifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    // Call the user-provided callback (if set) using the instance pointer
    if (_instance && _instance->_statusCallback) {
        if(length > 0){
            uint8_t status = pData[0];
            // Don't set _bonded here.  Wait for the onConnect callback.
            _instance->_statusCallback(status);
        }
    }
}


// Helper function to convert float to 5.11 fixed-point
uint16_t BMDBLEController::_floatToFixed16(float value) {
    return (uint16_t)(value * 2048.0f);
}

// --- Convenience Function Implementations (Examples) ---

bool BMDBLEController::setAperture(float fStop) {
    // Calculate the 5.11 fixed-point representation
    // Aperture Value (AV) = sqrt(2^fnumber)
    float apertureValue = sqrt(pow(2,fStop));
    uint16_t fixedValue = _floatToFixed16(apertureValue);
    // Split the 16-bit value into two bytes (little-endian)
    uint8_t data[2] = { (uint8_t)(fixedValue & 0xFF), (uint8_t)(fixedValue >> 8) };
    return sendCommand(0, 0, 0, 2, 128, 0, data, 2); // Camera 0, Lens, Aperture (f-stop), fixed16, assign
}

bool BMDBLEController::setISO(int iso) {
    // Convert int to byte array (little-endian)
    uint8_t data[4] = {
        (uint8_t)(iso & 0xFF),
        (uint8_t)((iso >> 8) & 0xFF),
        (uint8_t)((iso >> 16) & 0xFF),
        (uint8_t)((iso >> 24) & 0xFF)
    };
    return sendCommand(0, 0, 1, 14, 3, 0, data, 4); // Camera 0, Video, ISO, signed32, assign
}

bool BMDBLEController::setWhiteBalance(int kelvin, int tint) {
    // Split kelvin and tint into byte arrays (little-endian)
    uint8_t data[4] = {
        (uint8_t)(kelvin & 0xFF),
        (uint8_t)((kelvin >> 8) & 0xFF),
        (uint8_t)(tint & 0xFF),
        (uint8_t)((tint >> 8) & 0xFF)
    };
    return sendCommand(0, 0, 1, 2, 2, 0, data, 4);  //Camera 0, Video, White Balance, signed16, assign
}

bool BMDBLEController::setShutterAngle(float angle)
{
    uint32_t angleValue = (uint32_t)(angle*100); //as per documentation
    uint8_t data[4] = {
        (uint8_t)(angleValue & 0xFF),
        (uint8_t)((angleValue >> 8) & 0xFF),
        (uint8_t)((angleValue >> 16) & 0xFF),
        (uint8_t)((angleValue >> 24) & 0xFF)
    };
    return sendCommand(0, 0, 1, 11, 3, 0, data, 4); //Camera 0, Video, Shutter Angle, signed32, assign
}

bool BMDBLEController::setNDFilter(float stop)
{
    uint16_t stopValue = _floatToFixed16(stop);
    uint8_t data[2] = { (uint8_t)(stopValue & 0xFF), (uint8_t)(stopValue >> 8) };
    return sendCommand(0, 0, 1, 16, 128, 0, data, 2); //Camera 0, Video, ND Filter Stop, fixed16, assign
}

bool BMDBLEController::autoFocus()
{
    return sendCommand(0, 0, 0, 1, 0, 0, nullptr, 0); //Camera 0, Lens, Instantaneous autofocus, void, assign
}

bool BMDBLEController::autoExposure(uint8_t mode)
{
    return sendCommand(0, 0, 1, 10, 1, 0, &mode, 1); //Camera 0, Video, Auto exposure mode, int8, assign
}

bool BMDBLEController::setRecordingFormat(uint8_t fileFrameRate, uint8_t sensorFrameRate, uint16_t frameWidth, uint16_t frameHeight, uint8_t flags)
{
    uint8_t data[9] = {
        fileFrameRate,
        sensorFrameRate,
        (uint8_t)(frameWidth & 0xFF),
        (uint8_t)((frameWidth >> 8) & 0xFF),
        (uint8_t)(frameHeight & 0xFF),
        (uint8_t)((frameHeight >> 8) & 0xFF),
        (uint8_t)(flags & 0xFF),
        0, //Padding
        0
    };
    return sendCommand(0, 0, 1, 9, 2, 0, data, 9); //Camera 0, Video, Recording Format, int16, assign
}

bool BMDBLEController::setTransportMode(uint8_t mode, int8_t speed)
{
    uint8_t data[2] = {
        mode,
        (uint8_t)speed
    };

    return sendCommand(0, 0, 10, 1, 1, 0, data, 2); //Camera 0, Media, Transport Mode, int8, assign
}

bool BMDBLEController::setCodec(uint8_t basicCodec, uint8_t codeVariant)
{
    uint8_t data[2] = {
        basicCodec,
        codeVariant
    };
    return sendCommand(0, 0, 10, 0, 1, 0, data, 2); //Camera 0, Media, Codec, int8 enum, assign
}
