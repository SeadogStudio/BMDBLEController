#include "BMDBLEController.h"

// Static member initialization (required for the callbacks)
BMDBLEController* BMDBLEController::_instance = nullptr;

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
    _instance = this; // Set the static instance pointer
}

bool BMDBLEController::begin() {
  BLEDevice::init(""); // Initialize the BLE device
  return true; // Indicate successful initialization
}

bool BMDBLEController::connectToCamera(BLEAdvertisedDevice* device) {
    if (_connected) {
        return false; // Already connected, prevent multiple connections
    }

    _pClient = BLEDevice::createClient();
    if (!_pClient) {
        return false; // Failed to create client
    }

    if (!_pClient->connect(device)) {
        return false; // Failed to connect
    }

    BLERemoteService* pRemoteService = _pClient->getService(BLACKMAGIC_CAMERA_SERVICE_UUID);
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
        _pIncomingCharacteristic->registerForNotify(_incomingDataNotifyCallback);
    }
    if (_statusCallback) {
        _pStatusCharacteristic->registerForNotify(_statusNotifyCallback);
    }
    _callbacksSet = true; // Set the flag to indicate callbacks are registered


    // Initiate bonding.  The simplest way to do this is to write to the
    // status characteristic.  A value of 0x01 powers on the camera and
    // starts the bonding process.
    uint8_t powerOn = 0x01;
    _pStatusCharacteristic->writeValue(&powerOn, 1, false); // Write without response

    _connected = true; // Set connection status
    return true; // Connection successful (so far)
}

void BMDBLEController::disconnect() {
  if (_connected) {
    // Deregister for notifications *before* disconnecting.  This is important
    // to prevent the callbacks from being called after the object is invalid.
    if (_callbacksSet) {
        _pIncomingCharacteristic->deregisterForNotify();
        _pStatusCharacteristic->deregisterForNotify();
        _callbacksSet = false; // Clear the flag
    }
    _pClient->disconnect();
    _connected = false;
    _bonded = false; // Reset bonded status on disconnect
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
    return _pOutgoingCharacteristic->writeValue(command, length, false);
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
    commandPacket[1] = dataLength;      // Command Length (data length only)
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
    bool result = _pOutgoingCharacteristic->writeValue(commandPacket, paddedLength, false);

    // Clean up
    delete[] commandPacket;

    return result;
}



void BMDBLEController::setIncomingDataCallback(IncomingDataCallback callback) {
    _incomingDataCallback = callback;
    // If already connected and callbacks are not yet set, set them now.
    if(_connected && !_callbacksSet && _pIncomingCharacteristic){
        _pIncomingCharacteristic->registerForNotify(_incomingDataNotifyCallback);
    }
}

void BMDBLEController::setStatusCallback(StatusCallback callback) {
    _statusCallback = callback;
    if(_connected && !_callbacksSet && _pStatusCharacteristic){
        _pStatusCharacteristic->registerForNotify(_statusNotifyCallback);
    }
}

// Static callback function for incoming data notifications
void BMDBLEController::_incomingDataNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    // Call the user-provided callback (if set) using the instance pointer
    if (_instance && _instance->_incomingDataCallback) {
        _instance->_incomingDataCallback(pData, length);
    }
}

// Static callback function for status notifications
void BMDBLEController::_statusNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    // Call the user-provided callback (if set) using the instance pointer
    if (_instance && _instance->_statusCallback) {
        if(length > 0){
            uint8_t status = pData[0];
            if (status & 0x04) { // Check for the "Paired" bit (bit 2)
                _instance->_bonded = true; // Update the bonded status
            }
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
    return sendCommand(0, 0, 2, 0, 128, 0, data, 2); // Camera 0, Lens, Aperture (f-stop), fixed16, assign
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
