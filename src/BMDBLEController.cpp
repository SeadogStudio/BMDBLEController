#include "BMDBLEController.h"

// Static instance for callbacks
BMDBLEController* BMDBLEController::_instance = nullptr;

// Constructor
BMDBLEController::BMDBLEController(String deviceName) :
  _deviceName(deviceName),
  _connectionState(BMD_STATE_DISCONNECTED),
  _deviceFound(false),
  _autoReconnect(true),
  _recordingState(false),
  _cameraStatus(0),
  _pClient(nullptr),
  _pServerAddress(nullptr),
  _pRemoteService(nullptr),
  _pOutgoingCameraControl(nullptr),
  _pIncomingCameraControl(nullptr),
  _pTimecode(nullptr),
  _pCameraStatus(nullptr),
  _pDeviceName(nullptr),
  _pScanCallbacks(nullptr),
  _pSecurityCallbacks(nullptr),
  _paramCount(0),
  _timecodeHours(0),
  _timecodeMinutes(0),
  _timecodeSeconds(0),
  _timecodeFrames(0),
  _lastReconnectAttempt(0),
  _parameterCallback(nullptr),
  _connectionCallback(nullptr),
  _timecodeCallback(nullptr),
  _pinRequestCallback(nullptr)
{
  _instance = this;
  
  // Clear parameter storage
  for (int i = 0; i < MAX_PARAMETERS; i++) {
    _parameters[i].valid = false;
  }
}

// Destructor
BMDBLEController::~BMDBLEController() {
  // Clean up
  disconnect();
  
  if (_pScanCallbacks != nullptr) {
    delete _pScanCallbacks;
    _pScanCallbacks = nullptr;
  }
  
  if (_pSecurityCallbacks != nullptr) {
    delete _pSecurityCallbacks;
    _pSecurityCallbacks = nullptr;
  }
  
  if (_pServerAddress != nullptr) {
    delete _pServerAddress;
    _pServerAddress = nullptr;
  }
}

// Initialize the controller
void BMDBLEController::begin() {
  Serial.println("Initializing BMD BLE Controller");
  
  // Initialize BLE
  BLEDevice::init(_deviceName.c_str());
  BLEDevice::setPower(ESP_PWR_LVL_P9); // Maximum power
  
  // Create callback objects
  _pScanCallbacks = new BMDScanCallbacks(this);
  _pSecurityCallbacks = new BMDSecurityCallbacks(this);
  
  // Check if we have previously connected to a camera
  _preferences.begin("bmdcamera", false);
  bool authenticated = _preferences.getBool("authenticated", false);
  String savedAddress = _preferences.getString("address", "");
  _preferences.end();
  
  if (authenticated && savedAddress.length() > 0) {
    Serial.println("Found saved camera connection");
    Serial.print("Saved address: ");
    Serial.println(savedAddress);
    
    _pServerAddress = new BLEAddress(savedAddress.c_str());
    _deviceFound = true;
  }
}

void BMDBLEController::loop() {
  // Handle reconnection
  checkConnection();
}

// Set parameter callback
void BMDBLEController::setParameterCallback(ParameterCallback callback) {
  _parameterCallback = callback;
}

// Set connection callback
void BMDBLEController::setConnectionCallback(ConnectionCallback callback) {
  _connectionCallback = callback;
}

// Set timecode callback
void BMDBLEController::setTimecodeCallback(TimecodeCallback callback) {
  _timecodeCallback = callback;
}

// Set PIN request callback
void BMDBLEController::setPinRequestCallback(PinRequestCallback callback) {
  _pinRequestCallback = callback;
}

// Get connection state
BMDConnectionState BMDBLEController::getConnectionState() {
  return _connectionState;
}

// Check if connected
bool BMDBLEController::isConnected() {
  if (_pClient && _pClient->isConnected()) {
    return true;
  }
  return false;
}

// Check if recording
bool BMDBLEController::isRecording() {
  return _recordingState;
}

// Parameter storage and retrieval

// Find parameter in storage
ParameterValue* BMDBLEController::findParameter(uint8_t category, uint8_t parameterId) {
  for (int i = 0; i < MAX_PARAMETERS; i++) {
    if (_parameters[i].valid && 
        _parameters[i].category == category && 
        _parameters[i].parameterId == parameterId) {
      return &_parameters[i];
    }
  }
  return nullptr;
}

// Store parameter
void BMDBLEController::storeParameter(uint8_t category, uint8_t parameterId, 
                                     uint8_t dataType, uint8_t operation, 
                                     uint8_t* data, size_t dataLength) {
  // Check if parameter already exists
  ParameterValue* param = findParameter(category, parameterId);
  
  // If not found, find an empty slot
  if (param == nullptr) {
    for (int i = 0; i < MAX_PARAMETERS; i++) {
      if (!_parameters[i].valid) {
        param = &_parameters[i];
        _paramCount++;
        break;
      }
    }
  }
  
  // If still not found, storage is full
  if (param == nullptr) {
    Serial.println("Parameter storage full!");
    return;
  }
  
  // Store parameter data
  param->category = category;
  param->parameterId = parameterId;
  param->dataType = dataType;
  param->operation = operation;
  param->dataLength = (dataLength > 64) ? 64 : dataLength;
  param->timestamp = millis();
  param->valid = true;
  
  // Copy data
  if (data != nullptr) {
    memcpy(param->data, data, param->dataLength);
  } else {
    param->dataLength = 0;
  }
  
  // Call callback if registered
  if (_parameterCallback) {
    _parameterCallback(category, parameterId, data, dataLength);
  }
}

// Check if parameter exists
bool BMDBLEController::hasParameter(uint8_t category, uint8_t parameterId) {
  return (findParameter(category, parameterId) != nullptr);
}

// Get parameter
ParameterValue* BMDBLEController::getParameter(uint8_t category, uint8_t parameterId) {
  return findParameter(category, parameterId);
}

// Process incoming control packet
void BMDBLEController::processIncomingPacket(uint8_t* pData, size_t length) {
  // Verify packet is long enough
  if (length < 8) {
    Serial.println("Invalid packet: too short");
    return;
  }
  
  // Parse packet structure
  uint8_t protocolId = pData[0];
  uint8_t packetLength = pData[1];
  uint8_t commandId = pData[2];
  uint8_t reserved = pData[3];
  uint8_t category = pData[4];
  uint8_t parameterId = pData[5];
  uint8_t dataType = pData[6];
  uint8_t operation = pData[7];
  
  // Special handling for recording state
  if (category == BMD_CAT_TRANSPORT && parameterId == BMD_PARAM_TRANSPORT_MODE) {
    if (length > 8) {
      bool newRecordingState = (pData[8] == 2);
      if (newRecordingState != _recordingState) {
        _recordingState = newRecordingState;
        Serial.print("Recording state changed: ");
        Serial.println(_recordingState ? "RECORDING" : "STOPPED");
      }
    }
  }
  
  // Store parameter data
  if (length > 8) {
    storeParameter(category, parameterId, dataType, operation, &pData[8], length - 8);
  } else {
    storeParameter(category, parameterId, dataType, operation, nullptr, 0);
  }
}

// Process timecode packet
void BMDBLEController::processTimecodePacket(uint8_t* pData, size_t length) {
  if (length >= 4) {
    // Blackmagic timecode format is BCD
    _timecodeFrames = (pData[0] >> 4) * 10 + (pData[0] & 0x0F);
    _timecodeSeconds = (pData[1] >> 4) * 10 + (pData[1] & 0x0F);
    _timecodeMinutes = (pData[2] >> 4) * 10 + (pData[2] & 0x0F);
    _timecodeHours = (pData[3] >> 4) * 10 + (pData[3] & 0x0F);
    
    // Call callback if registered
    if (_timecodeCallback) {
      _timecodeCallback(_timecodeHours, _timecodeMinutes, _timecodeSeconds, _timecodeFrames);
    }
  }
}

// Process status packet
void BMDBLEController::processStatusPacket(uint8_t* pData, size_t length) {
  if (length > 0) {
    _cameraStatus = pData[0];
    Serial.print("Camera status updated: 0x");
    Serial.println(_cameraStatus, HEX);
  }
}

// Static callback functions
void BMDBLEController::controlNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                          uint8_t* pData, size_t length, bool isNotify) {
  if (_instance) {
    _instance->processIncomingPacket(pData, length);
  }
}

void BMDBLEController::timecodeNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                           uint8_t* pData, size_t length, bool isNotify) {
  if (_instance) {
    _instance->processTimecodePacket(pData, length);
  }
}

void BMDBLEController::statusNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                         uint8_t* pData, size_t length, bool isNotify) {
  if (_instance) {
    _instance->processStatusPacket(pData, length);
  }
}
