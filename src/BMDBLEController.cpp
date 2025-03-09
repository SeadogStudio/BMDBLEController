#include "BMDBLEController.h"

// Constructor
BMDBLEController::BMDBLEController(String deviceName) {
  _deviceName = deviceName;
  _connected = false;
  _deviceFound = false;
  _scanInProgress = false;
  _autoReconnect = true;
  _recordingState = false;
  _paramCount = 0;
  _lastReconnectAttempt = 0;
  _pClient = nullptr;
  _pServerAddress = nullptr;
  _pRemoteService = nullptr;
  _pOutgoingCameraControl = nullptr;
  _pIncomingCameraControl = nullptr;
  _pTimecode = nullptr;
  _pCameraStatus = nullptr;
  _pDeviceName = nullptr;
  _responseCallback = nullptr;
  _connectionCallback = nullptr;
  _pSecurityCallbacks = new BMDSecurityCallbacks(this);
  _pAdvertisedDeviceCallbacks = new BMDAdvertisedDeviceCallbacks(this);
  _timecodeStr = "--:--:--:--";
  _cameraStatus = 0;
}

// Initialize BLE
void BMDBLEController::begin() {
  // Initialize BLE
  BLEDevice::init(_deviceName.c_str());
  BLEDevice::setPower(ESP_PWR_LVL_P9); // Set to maximum power
  
  // Check if we have previously connected to a camera
  _preferences.begin("camera", false);
  bool authenticated = _preferences.getBool("authenticated", false);
  String savedAddress = _preferences.getString("address", "");
  _preferences.end();
  
  if (authenticated && savedAddress.length() > 0) {
    Serial.println("Found saved camera connection");
    _pServerAddress = new BLEAddress(savedAddress.c_str());
    _deviceFound = true;
  }
}

// Main loop function
void BMDBLEController::loop() {
  // If not connected but device found, try to reconnect
  if (_deviceFound && !_connected && _autoReconnect) {
    unsigned long currentTime = millis();
    if (currentTime - _lastReconnectAttempt >= RECONNECT_INTERVAL) {
      _lastReconnectAttempt = currentTime;
      connect();
    }
  }
  
  // Check if connected client is still active
  if (_pClient && !_pClient->isConnected() && _connected) {
    Serial.println("Connection lost!");
    _connected = false;
    
    // Call connection callback if set
    if (_connectionCallback) {
      _connectionCallback(false);
    }
  }
}

// Start scanning for Blackmagic cameras
bool BMDBLEController::scan(uint32_t duration) {
  if (_scanInProgress) {
    return false;
  }
  
  _scanInProgress = true;
  _deviceFound = false;
  
  Serial.println("Scanning for Blackmagic cameras...");
  
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(_pAdvertisedDeviceCallbacks);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(duration);
  
  return true;
}

// Connect to the camera
bool BMDBLEController::connect() {
  if (!_deviceFound || _connected) {
    return false;
  }
  
  Serial.print("Connecting to camera at address: ");
  Serial.println(_pServerAddress->toString().c_str());
  
  // Create a BLE client
  _pClient = BLEDevice::createClient();
  
  // Set up security
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  BLEDevice::setSecurityCallbacks(_pSecurityCallbacks);
  
  BLESecurity* pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  pSecurity->setCapability(ESP_IO_CAP_IN);
  pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  
  // Connect to the remote device
  if (!_pClient->connect(*_pServerAddress)) {
    Serial.println("Connection failed");
    delete pSecurity;
    return false;
  }
  
  Serial.println("Connected to camera");
  
  // Obtain a reference to the service
  _pRemoteService = _pClient->getService(BLEUUID(BMD_SERVICE_UUID));
  if (_pRemoteService == nullptr) {
    Serial.println("Failed to find Blackmagic camera service");
    _pClient->disconnect();
    delete pSecurity;
    return false;
  }
  
  // Get characteristic references
  _pOutgoingCameraControl = _pRemoteService->getCharacteristic(BLEUUID(BMD_OUTGOING_CONTROL_UUID));
  if (_pOutgoingCameraControl == nullptr) {
    Serial.println("Failed to find outgoing control characteristic");
    _pClient->disconnect();
    delete pSecurity;
    return false;
  }
  
  _pIncomingCameraControl = _pRemoteService->getCharacteristic(BLEUUID(BMD_INCOMING_CONTROL_UUID));
  if (_pIncomingCameraControl == nullptr) {
    Serial.println("Failed to find incoming control characteristic");
    _pClient->disconnect();
    delete pSecurity;
    return false;
  }
  
  // Optional characteristics
  _pTimecode = _pRemoteService->getCharacteristic(BLEUUID(BMD_TIMECODE_UUID));
  _pCameraStatus = _pRemoteService->getCharacteristic(BLEUUID(BMD_CAMERA_STATUS_UUID));
  _pDeviceName = _pRemoteService->getCharacteristic(BLEUUID(BMD_DEVICE_NAME_UUID));
  
  // Set device name
  if (_pDeviceName) {
    _pDeviceName->writeValue(_deviceName.c_str(), _deviceName.length());
    Serial.println("Device name set");
  }
  
  // Register for notifications
  if (_pIncomingCameraControl) {
    _pIncomingCameraControl->registerForNotify(notifyCallback, this);
    if (!setNotification(_pIncomingCameraControl, true, true)) {
      Serial.println("Failed to enable indications for Incoming Camera Control");
    }
  }
  
  if (_pTimecode) {
    _pTimecode->registerForNotify(timecodeNotifyCallback, this);
    if (!setNotification(_pTimecode, true, false)) {
      Serial.println("Failed to enable notifications for Timecode");
    }
  }
  
  if (_pCameraStatus) {
    _pCameraStatus->registerForNotify(statusNotifyCallback, this);
  }
  
  _connected = true;
  
  // Call connection callback if set
  if (_connectionCallback) {
    _connectionCallback(true);
  }
  
  delete pSecurity;
  return true;
}

// Disconnect from the camera
bool BMDBLEController::disconnect() {
  if (!_connected || !_pClient) {
    return false;
  }
  
  _pClient->disconnect();
  _connected = false;
  
  // Call connection callback if set
  if (_connectionCallback) {
    _connectionCallback(false);
  }
  
  return true;
}

// Check if connected to a camera
bool BMDBLEController::isConnected() {
  return _connected && _pClient && _pClient->isConnected();
}

// Clear bonding information
void BMDBLEController::clearBondingInfo() {
  _preferences.begin("camera", false);
  _preferences.clear();
  _preferences.end();
  
  if (_pServerAddress) {
    esp_ble_remove_bond_device(*_pServerAddress->getNative());
  }
  
  int dev_num = esp_ble_get_bond_device_num();
  esp_ble_bond_dev_t* dev_list = (esp_ble_bond_dev_t*)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
  esp_ble_get_bond_device_list(&dev_num, dev_list);
  for (int i = 0; i < dev_num; i++) {
    esp_ble_remove_bond_device(dev_list[i].bd_addr);
  }
  free(dev_list);
  
  Serial.println("Cleared all saved pairing information");
}

// Enable or disable auto reconnection
void BMDBLEController::setAutoReconnect(bool enabled) {
  _autoReconnect = enabled;
}

// Set focus with a raw value (0-2048)
bool BMDBLEController::setFocus(uint16_t rawValue) {
  if (!_connected || !_pOutgoingCameraControl) {
    return false;
  }
  
  // Create focus command
  uint8_t focusCommand[12] = {
    0xFF,                    // Destination (all cameras)
    0x08,                    // Command length
    0x00, 0x00,              // Command ID, Reserved
    BMD_CAT_LENS,            // Category (Lens)
    BMD_PARAM_FOCUS,         // Parameter (Focus)
    BMD_TYPE_FIXED16,        // Data type (fixed16)
    BMD_OP_ASSIGN,           // Operation (assign)
    (uint8_t)(rawValue & 0xFF),         // Low byte
    (uint8_t)((rawValue >> 8) & 0xFF),  // High byte
    0x00, 0x00               // Padding
  };
  
  return sendRawCommand(focusCommand, sizeof(focusCommand));
}

// Set focus with a normalized value (0.0-1.0)
bool BMDBLEController::setFocus(float normalizedValue) {
  // Clamp value between 0.0 and 1.0
  if (normalizedValue < 0.0f) normalizedValue = 0.0f;
  if (normalizedValue > 1.0f) normalizedValue = 1.0f;
  
  // Convert to raw value
  uint16_t rawValue = (uint16_t)(normalizedValue * 2048.0f);
  
  return setFocus(rawValue);
}

// Set iris with a normalized value (0.0-1.0)
bool BMDBLEController::setIris(float normalizedValue) {
  if (!_connected || !_pOutgoingCameraControl) {
    return false;
  }
  
  // Clamp value between 0.0 and 1.0
  if (normalizedValue < 0.0f) normalizedValue = 0.0f;
  if (normalizedValue > 1.0f) normalizedValue = 1.0f;
  
  // Convert to fixed16 format
  int16_t fixed16Value = (int16_t)(normalizedValue * 2048.0f);
  
  // Create iris command
  uint8_t irisCommand[12] = {
    0xFF,                    // Destination (all cameras)
    0x08,                    // Command length
    0x00, 0x00,              // Command ID, Reserved
    BMD_CAT_LENS,            // Category (Lens)
    BMD_PARAM_IRIS_NORM,     // Parameter (Iris Normalized)
    BMD_TYPE_FIXED16,        // Data type (fixed16)
    BMD_OP_ASSIGN,           // Operation (assign)
    (uint8_t)(fixed16Value & 0xFF),         // Low byte
    (uint8_t)((fixed16Value >> 8) & 0xFF),  // High byte
    0x00, 0x00               // Padding
  };
  
  return sendRawCommand(irisCommand, sizeof(irisCommand));
}

// Set white balance in Kelvin
bool BMDBLEController::setWhiteBalance(uint16_t kelvin) {
  if (!_connected || !_pOutgoingCameraControl) {
    return false;
  }
  
  // Create white balance command
  uint8_t wbCommand[14] = {
    0xFF,                    // Destination (all cameras)
    0x08,                    // Command length
    0x00, 0x00,              // Command ID, Reserved
    BMD_CAT_VIDEO,           // Category (Video)
    BMD_PARAM_WB,            // Parameter (White Balance)
    BMD_TYPE_INT16,          // Data type (int16)
    BMD_OP_ASSIGN,           // Operation (assign)
    (uint8_t)(kelvin & 0xFF),         // Low byte
    (uint8_t)((kelvin >> 8) & 0xFF),  // High byte
    0x00, 0x00,              // Tint (0)
    0x00, 0x00               // Padding
  };
  
  return sendRawCommand(wbCommand, sizeof(wbCommand));
}

// Toggle recording state
bool BMDBLEController::toggleRecording() {
  if (!_connected || !_pOutgoingCameraControl) {
    return false;
  }
  
  // Create recording command
  uint8_t recordingCommand[12] = {
    0xFF,                    // Destination (all cameras)
    0x05,                    // Command length
    0x00, 0x00,              // Command ID, Reserved
    BMD_CAT_TRANSPORT,       // Category (Transport)
    BMD_PARAM_TRANSPORT_MODE,// Parameter (Transport Mode)
    BMD_TYPE_BYTE,           // Data type (int8)
    BMD_OP_ASSIGN,           // Operation (assign)
    _recordingState ? 0x00 : 0x02,  // Toggle between record (2) and preview (0)
    0x00, 0x00, 0x00         // Padding
  };
  
  bool result = sendRawCommand(recordingCommand, sizeof(recordingCommand));
  
  if (result) {
    // Update local recording state (will be confirmed by incoming control packet)
    _recordingState = !_recordingState;
  }
  
  return result;
}

// Trigger auto focus
bool BMDBLEController::doAutoFocus() {
  if (!_connected || !_pOutgoingCameraControl) {
    return false;
  }
  
  // Create auto focus command
  uint8_t afCommand[12] = {
    0xFF,                    // Destination (all cameras)
    0x08,                    // Command length
    0x00, 0x00,              // Command ID, Reserved
    BMD_CAT_LENS,            // Category (Lens)
    BMD_PARAM_AUTO_FOCUS,    // Parameter (Auto Focus)
    BMD_TYPE_VOID,           // Data type (void)
    BMD_OP_ASSIGN,           // Operation (assign)
    0x01,                    // Execute auto focus
    0x00, 0x00, 0x00         // Padding
  };
  
  return sendRawCommand(afCommand, sizeof(afCommand));
}

// Get current recording state
bool BMDBLEController::isRecording() {
  return _recordingState;
}

// Send a raw command to the camera
bool BMDBLEController::sendRawCommand(uint8_t* commandData, size_t length) {
  if (!_connected || !_pOutgoingCameraControl) {
    return false;
  }
  
  try {
    _pOutgoingCameraControl->writeValue(commandData, length, true);
    return true;
  } catch (std::exception &e) {
    Serial.print("Exception sending command: ");
    Serial.println(e.what());
    return false;
  }
}

// Send a formatted command to the camera
bool BMDBLEController::sendFormattedCommand(uint8_t category, uint8_t parameter, 
                                          uint8_t dataType, uint8_t operation,
                                          uint8_t* data, size_t dataLength) {
  if (!_connected || !_pOutgoingCameraControl) {
    return false;
  }
  
  // Calculate total command length
  size_t totalLength = 8 + dataLength; // 8 bytes for header
  
  // Pad to 4-byte boundary
  size_t paddedLength = (totalLength + 3) & ~3;
  
  // Create command buffer
  uint8_t* commandData = new uint8_t[paddedLength];
  memset(commandData, 0, paddedLength); // Clear buffer and set padding to 0
  
  // Set header
  commandData[0] = 0xFF;              // Destination (all cameras)
  commandData[1] = (uint8_t)dataLength;   // Command length
  commandData[2] = 0x00;              // Command ID
  commandData[3] = 0x00;              // Reserved
  commandData[4] = category;          // Category
  commandData[5] = parameter;         // Parameter
  commandData[6] = dataType;          // Data type
  commandData[7] = operation;         // Operation
  
  // Copy data if any
  if (dataLength > 0 && data != nullptr) {
    memcpy(&commandData[8], data, dataLength);
  }
  
  // Send command
  bool result = sendRawCommand(commandData, paddedLength);
  
  // Clean up
  delete[] commandData;
  
  return result;
}

// Request a parameter from the camera
bool BMDBLEController::requestParameter(uint8_t category, uint8_t parameterId, uint8_t dataType) {
  return sendFormattedCommand(category, parameterId, dataType, BMD_OP_REPORT, nullptr, 0);
}

// Check if a parameter is available
bool BMDBLEController::hasParameter(uint8_t category, uint8_t parameterId) {
  ParameterValue* param = findParameter(category, parameterId);
  return (param != nullptr && param->valid);
}

// Get parameter timestamp
unsigned long BMDBLEController::getParameterTimestamp(uint8_t category, uint8_t parameterId) {
  ParameterValue* param = findParameter(category, parameterId);
  if (param != nullptr && param->valid) {
    return param->timestamp;
  }
  return 0;
}

// Get parameter as string
String BMDBLEController::getParameterAsString(uint8_t category, uint8_t parameterId) {
  ParameterValue* param = findParameter(category, parameterId);
  if (param != nullptr && param->valid) {
    return decodeParameterToString(param);
  }
  return "";
}

// Get parameter as integer
int32_t BMDBLEController::getParameterAsInt(uint8_t category, uint8_t parameterId) {
  ParameterValue* param = findParameter(category, parameterId);
  if (param != nullptr && param->valid) {
    // Process based on data type
    switch (param->dataType) {
      case BMD_TYPE_BYTE: {
        return (int32_t)(int8_t)param->data[0];
      }
      case BMD_TYPE_INT16: {
        return (int32_t)((int16_t)(param->data[0] | (param->data[1] << 8)));
      }
      case BMD_TYPE_INT32: {
        return (int32_t)(param->data[0] | 
                        (param->data[1] << 8) | 
                        (param->data[2] << 16) | 
                        (param->data[3] << 24));
      }
      case BMD_TYPE_FIXED16: {
        int16_t rawValue = (int16_t)(param->data[0] | (param->data[1] << 8));
        return (int32_t)rawValue;
      }
    }
  }
  return 0;
}

// Get parameter as float
float BMDBLEController::getParameterAsFloat(uint8_t category, uint8_t parameterId) {
  ParameterValue* param = findParameter(category, parameterId);
  if (param != nullptr && param->valid) {
    // Process based on data type
    switch (param->dataType) {
      case BMD_TYPE_BYTE: {
        return (float)(int8_t)param->data[0];
      }
      case BMD_TYPE_INT16: {
        return (float)((int16_t)(param->data[0] | (param->data[1] << 8)));
      }
      case BMD_TYPE_INT32: {
        int32_t value = (int32_t)(param->data[0] | 
                                 (param->data[1] << 8) | 
                                 (param->data[2] << 16) | 
                                 (param->data[3] << 24));
        
        // Special case for shutter angle (category 1, parameter 11)
        if (category == BMD_CAT_VIDEO && parameterId == BMD_PARAM_SHUTTER_ANGLE) {
          return (float)value / 100.0f;
        }
        
        return (float)value;
      }
      case BMD_TYPE_FIXED16: {
        int16_t rawValue = (int16_t)(param->data[0] | (param->data[1] << 8));
        return (float)rawValue / 2048.0f;
      }
    }
  }
  return 0.0f;
}

// Get parameter as hex string
String BMDBLEController::getParameterAsHexString(uint8_t category, uint8_t parameterId) {
  ParameterValue* param = findParameter(category, parameterId);
  if (param != nullptr && param->valid) {
    String result = "";
    for (size_t i = 0; i < param->dataLength; i++) {
      if (param->data[i] < 0x10) {
        result += "0";
      }
      result += String(param->data[i], HEX);
      result += " ";
    }
    return result;
  }
  return "";
}

// Get raw parameter data
bool BMDBLEController::getRawParameter(uint8_t category, uint8_t parameterId, uint8_t* buffer, size_t* bufferSize) {
  ParameterValue* param = findParameter(category, parameterId);
  if (param != nullptr && param->valid && buffer != nullptr && bufferSize != nullptr) {
    size_t copySize = min(*bufferSize, param->dataLength);
    memcpy(buffer, param->data, copySize);
    *bufferSize = copySize;
    return true;
  }
  return false;
}

// Set response callback
void BMDBLEController::setResponseCallback(void (*callback)(uint8_t, uint8_t, String)) {
  _responseCallback = callback;
}

// Set connection callback
void BMDBLEController::setConnectionCallback(void (*callback)(bool)) {
  _connectionCallback = callback;
  
  // If already connected, call the callback immediately
  if (_connected && callback) {
    callback(true);
  }
}

// Find parameter in storage
ParameterValue* BMDBLEController::findParameter(uint8_t category, uint8_t parameterId) {
  for (int i = 0; i < _paramCount; i++) {
    if (_parameters[i].category == category && _parameters[i].parameterId == parameterId) {
      return &_parameters[i];
    }
  }
  return nullptr;
}

// Store parameter
void BMDBLEController::storeParameter(uint8_t category, uint8_t parameterId, 
                                    uint8_t dataType, uint8_t operation, 
                                    uint8_t* data, size_t dataLength) {
  // Look for existing parameter
  ParameterValue* param = findParameter(category, parameterId);
  
  // If not found, add a new one
  if (param == nullptr) {
    if (_paramCount < MAX_PARAMETERS) {
      param = &_parameters[_paramCount++];
      param->category = category;
      param->parameterId = parameterId;
    } else {
      Serial.println("Parameter storage full!");
      return;
    }
  }
  
  // Update parameter data
  param->dataType = dataType;
  param->operation = operation;
  param->dataLength = min(dataLength, (size_t)64);
  param->timestamp = millis();
  param->valid = true;
  
  // Copy data
  if (data != nullptr && dataLength > 0) {
    memcpy(param->data, data, param->dataLength);
  }
  
  // Call response callback if set
  if (_responseCallback) {
    _responseCallback(category, parameterId, decodeParameterToString(param));
  }
}

// Decode parameter to string
String BMDBLEController::decodeParameterToString(ParameterValue* param) {
  if (param == nullptr || !param->valid) {
    return "";
  }
  
  String result = "";
  
  switch (param->dataType) {
    case BMD_TYPE_VOID:
      result = "void";
      break;
      
    case BMD_TYPE_BYTE: {
      int8_t value = (int8_t)param->data[0];
      
      // Special case handling based on category/parameter
      if (param->category == BMD_CAT_VIDEO && param->parameterId == BMD_PARAM_DYNAMIC_RANGE) {
        switch (value) {
          case 0: result = "Film Mode"; break;
          case 1: result = "Video Mode"; break;
          case 2: result = "Extended Video Mode"; break;
          default: result = String(value);
        }
      } else if (param->category == BMD_CAT_VIDEO && param->parameterId == BMD_PARAM_DISPLAY_LUT) {
        switch (value) {
          case 0: result = "None"; break;
          case 1: result = "Custom LUT"; break;
          case 2: result = "Film to Video"; break;
          case 3: result = "Extended Video"; break;
          default: result = String(value);
        }
      } else {
        result = String(value);
      }
      break;
    }
      
    case BMD_TYPE_INT16: {
      int16_t value = (int16_t)(param->data[0] | (param->data[1] << 8));
      
      // Special case for white balance (category 1, parameter 2)
      if (param->category == BMD_CAT_VIDEO && param->parameterId == BMD_PARAM_WB) {
        result = String(value) + "K";
      } else {
        result = String(value);
      }
      break;
    }
      
    case BMD_TYPE_INT32: {
      int32_t value = (int32_t)(param->data[0] | 
                               (param->data[1] << 8) | 
                               (param->data[2] << 16) | 
                               (param->data[3] << 24));
      
      // Special case for shutter angle (category 1, parameter 11)
      if (param->category == BMD_CAT_VIDEO && param->parameterId == BMD_PARAM_SHUTTER_ANGLE) {
        float angle = (float)value / 100.0;
        result = String(angle, 2) + "°";
      }
      // Special case for shutter speed (category 1, parameter 12)
      else if (param->category == BMD_CAT_VIDEO && param->parameterId == BMD_PARAM_SHUTTER_SPEED) {
        result = "1/" + String(value) + " sec";
      } else {
        result = String(value);
      }
      break;
    }
      
    case BMD_TYPE_STRING:
      result = extractTextData(param->data, param->dataLength, 0);
      break;
      
    case BMD_TYPE_FIXED16: {
      int16_t raw = (int16_t)(param->data[0] | (param->data[1] << 8));
      float value = (float)raw / 2048.0f;
      
      // For focus (category 0, parameter 0), show both normalized and raw
      if (param->category == BMD_CAT_LENS && param->parameterId == BMD_PARAM_FOCUS) {
        result = String(value, 3) + " (raw: " + String(raw) + ")";
      } else {
        result = String(value, 3);
      }
      break;
    }
      
    default:
      result = "Unknown data type";
      break;
  }
  
  return result;
}

// Extract text data from a packet
String BMDBLEController::extractTextData(uint8_t* pData, size_t length, int offset) {
  String result = "";
  for (size_t i = offset; i < length; i++) {
    if (pData[i] == 0) break; // Stop at null terminator
    if (isprint(pData[i])) {
      result += (char)pData[i];
    }
  }
  return result;
}

// Set notification or indication
bool BMDBLEController::setNotification(BLERemoteCharacteristic* pChar, bool enable, bool isIndication) {
  if (!pChar) return false;
  
  // Find the Client Characteristic Configuration descriptor
  BLERemoteDescriptor* pDescriptor = pChar->getDescriptor(BLEUUID((uint16_t)0x2902));
  if (!pDescriptor) {
    Serial.println("Failed to get CCCD descriptor");
    return false;
  }
  
  // Set the value based on whether we want notifications or indications
  uint8_t val[2];
  if (enable) {
    val[0] = isIndication ? 0x02 : 0x01; // 0x02 for indications, 0x01 for notifications
  } else {
    val[0] = 0x00; // Disable
  }
  val[1] = 0x00;
  
  // Write to the descriptor
  try {
    pDescriptor->writeValue(val, 2, true);
    Serial.print(isIndication ? "Indications" : "Notifications");
    Serial.println(enable ? " enabled" : " disabled");
    return true;
  } catch (std::exception &e) {
    Serial.print("Exception setting notification/indication: ");
    Serial.println(e.what());
    return false;
  }
}

// Process incoming camera control packet
void BMDBLEController::processIncomingPacket(uint8_t* pData, size_t length) {
  // Verify packet is long enough
  if (length < 8) {
    Serial.println("Invalid packet: too short");
    return;
  }
  
  // Parse basic packet format
  uint8_t category = pData[4];
  uint8_t parameter = pData[5];
  uint8_t dataType = pData[6];
  uint8_t operation = pData[7];
  
  // Filter out battery voltage packets (Category 0x09, Parameter 0x00)
  if (category == BMD_CAT_STATUS && parameter == 0x00) {
    return; // Ignore battery voltage updates
  }
  
  // Check for recording state update
  if (category == BMD_CAT_TRANSPORT && parameter == BMD_PARAM_TRANSPORT_MODE) {
    if (length > 8 && dataType == BMD_TYPE_BYTE) {
      // Extract the transport mode
      uint8_t transportMode = pData[8];
      bool newRecordingState = (transportMode == 2); // 2 = record
      
      // Update recording state if changed
      if (_recordingState != newRecordingState) {
        _recordingState = newRecordingState;
        Serial.print("Recording state changed to: ");
        Serial.println(_recordingState ? "recording" : "stopped");
      }
    }
  }
  
  // Store the parameter
  storeParameter(category, parameter, dataType, operation, &pData[8], length - 8);
}

// Process timecode packet
void BMDBLEController::processTimecodePacket(uint8_t* pData, size_t length) {
  if (length >= 4) {
    // Blackmagic timecode format is BCD (Binary-Coded Decimal)
    uint8_t frames = (pData[0] >> 4) * 10 + (pData[0] & 0x0F);
    uint8_t seconds = (pData[1] >> 4) * 10 + (pData[1] & 0x0F);
    uint8_t minutes = (pData[2] >> 4) * 10 + (pData[2] & 0x0F);
    uint8_t hours = (pData[3] >> 4) * 10 + (pData[3] & 0x0F);
    
    char tc[12];
    sprintf(tc, "%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
    _timecodeStr = String(tc);
  }
}

// Process camera status packet
void BMDBLEController::processStatusPacket(uint8_t* pData, size_t length) {
  if (length > 0) {
    _cameraStatus = pData[0];
  }
}

// Security callback implementation
uint32_t BMDBLEController::BMDSecurityCallbacks::onPassKeyRequest() {
  Serial.println("---> PLEASE ENTER 6 DIGIT PIN (end with ENTER): ");
  
  int pinCode = 0;
  char ch;
  do {
    while (!Serial.available()) {
      vTaskDelay(1);
    }
    ch = Serial.read();
    if (ch >= '0' && ch <= '9') {
      pinCode = pinCode * 10 + (ch - '0');
      Serial.print(ch);
    }
  } while (ch != '\n');
  Serial.println();
  Serial.print("PIN code: ");
  Serial.println(pinCode);
  return pinCode;
}

void BMDBLEController::BMDSecurityCallbacks::onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) {
  if (auth_cmpl.success) {
    Serial.println("Authentication successful");
    // Save authentication state to preferences
    _controller->_preferences.begin("camera", false);
    _controller->_preferences.putBool("authenticated", true);
    if (_controller->_pServerAddress) {
      _controller->_preferences.putString("address", _controller->_pServerAddress->toString().c_str());
    }
    _controller->_preferences.end();
  } else {
    Serial.println("Authentication failed");
  }
}

// Scan callback implementation
void BMDBLEController::BMDAdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
  Serial.print("Found device: ");
  Serial.println(advertisedDevice.toString().c_str());
  
  // Check for Blackmagic camera service UUID
  if (advertisedDevice.haveServiceUUID() && 
      advertisedDevice.isAdvertisingService(BLEUUID(BMD_SERVICE_UUID))) {
    
    Serial.println("Found a Blackmagic Design camera!");
    advertisedDevice.getScan()->stop();
    
    // Save the device address and set flag
    if (_controller->_pServerAddress) {
      delete _controller->_pServerAddress;
    }
    _controller->_pServerAddress = new BLEAddress(advertisedDevice.getAddress());
    _controller->_deviceFound = true;
    _controller->_scanInProgress = false;
  }
}
