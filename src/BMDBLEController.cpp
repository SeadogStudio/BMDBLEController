#include "BMDBLEController.h"

// Static instance for callbacks
BMDBLEController* BMDBLEController::_instance = nullptr;

// Advertised device callbacks class
class BMDSecurityCallbacks: public BLESecurityCallbacks {
private:
  BMDBLEController* _controller;
  
public:
  BMDAdvertisedDeviceCallbacks(BMDBLEController* controller) : _controller(controller) {}
  
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
    
    // Check for Blackmagic camera service UUID
    if (advertisedDevice.haveServiceUUID() && 
        advertisedDevice.isAdvertisingService(BLEUUID(BMD_SERVICE_UUID))) {
      
      Serial.println("Found a Blackmagic Design camera!");
      advertisedDevice.getScan()->stop();
      
      // Save the device address
      if (_controller->_pServerAddress != nullptr) {
        delete _controller->_pServerAddress;
      }
      _controller->_pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      _controller->_deviceFound = true;
      
      // Trigger connection state callback
      if (_controller->_connectionCallback) {
        _controller->_connectionCallback(BMD_STATE_SCANNING);
      }
    }
  }
};

// Security callbacks class
class BMDSecurityCallbacks: public BLESecurityCallbacks {
private:
  BMDBLEController* _controller;
  
public:
  BMDSecurityCallbacks(BMDBLEController* controller) : _controller(controller) {}
  
  uint32_t onPassKeyRequest() {
    Serial.println("PIN code requested for pairing");
    
    uint32_t pinCode = 123456; // Default PIN code
    
    // Call user callback if set
    if (_controller->_pinRequestCallback) {
      _controller->_pinRequestCallback(&pinCode);
    } else {
      Serial.println("---> PLEASE ENTER 6 DIGIT PIN (default: 123456): ");
      
      char input[7] = {0};
      int inputPos = 0;
      
      // Wait for PIN input
      while (inputPos < 6) {
        if (Serial.available()) {
          char c = Serial.read();
          if (c >= '0' && c <= '9') {
            input[inputPos++] = c;
            Serial.print(c);
          }
        }
        delay(10);
      }
      
      Serial.println();
      pinCode = atoi(input);
    }
    
    Serial.print("Using PIN code: ");
    Serial.println(pinCode);
    
    return pinCode;
  }

  void onPassKeyNotify(uint32_t pass_key) {
    // Not used
  }

  bool onConfirmPIN(uint32_t pin) {
    return true;
  }

  bool onSecurityRequest() {
    return true;
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) {
    if (auth_cmpl.success) {
      Serial.println("Authentication successful");
      
      // Save authentication state to preferences
      _controller->_preferences.begin("bmdcamera", false);
      _controller->_preferences.putBool("authenticated", true);
      if (_controller->_pServerAddress) {
        _controller->_preferences.putString("address", _controller->_pServerAddress->toString().c_str());
      }
      _controller->_preferences.end();
    } else {
      Serial.println("Authentication failed");
    }
  }
};

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
  _pScanCallbacks = new BMDAdvertisedDeviceCallbacks(this);
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

// Check connection and handle reconnect
void BMDBLEController::checkConnection() {
  // Check if client exists and connection state
  if (_pClient && !_pClient->isConnected() && _connectionState == BMD_STATE_CONNECTED) {
    Serial.println("Connection lost!");
    _connectionState = BMD_STATE_DISCONNECTED;
    
    // Trigger callback
    if (_connectionCallback) {
      _connectionCallback(_connectionState);
    }
  }
  
  // Handle reconnection if auto-reconnect is enabled
  if (_autoReconnect && _deviceFound && _connectionState == BMD_STATE_DISCONNECTED) {
    unsigned long currentTime = millis();
    if (currentTime - _lastReconnectAttempt >= RECONNECT_INTERVAL) {
      _lastReconnectAttempt = currentTime;
      reconnect();
    }
  }
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
  memcpy(param->data, data, param->dataLength);
  
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
      _recordingState = (pData[8] == 2);
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
