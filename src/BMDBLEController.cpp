#include "BMDBLEController.h"

// Constructor
BMDBLEController::BMDBLEController(String deviceName) {
  _deviceName = deviceName;
  _connected = false;
  _deviceFound = false;
  _scanInProgress = false;
  _autoReconnect = true;
  _displayInitialized = false;
  _recordingState = false;
  _lastReconnectAttempt = 0;
  _currentDisplayMode = BMD_DISPLAY_BASIC;
  _paramCount = 0;
  _pClient = nullptr;
  _pServerAddress = nullptr;
  _pDisplay = nullptr;
  _responseCallback = nullptr;
}

// Initialize library
void BMDBLEController::begin() {
  Serial.println("Initializing BMDBLEController...");
  
  // Initialize BLE
  BLEDevice::init(_deviceName.c_str());
  BLEDevice::setPower(ESP_PWR_LVL_P9); // Maximum power
  
  // Create BLE callback instances
  _pAdvertisedDeviceCallbacks = new BMDAdvertisedDeviceCallbacks(this);
  _pSecurityCallbacks = new BMDSecurityCallbacks(this);
  
  // Set up security
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  BLEDevice::setSecurityCallbacks(_pSecurityCallbacks);
  
  // Check for saved camera connection
  preferences.begin("bmdcamera", false);
  bool authenticated = preferences.getBool("authenticated", false);
  String savedAddress = preferences.getString("address", "");
  preferences.end();
  
  if (authenticated && savedAddress.length() > 0) {
    Serial.println("Found saved camera connection: " + savedAddress);
    _pServerAddress = new BLEAddress(savedAddress.c_str());
    _deviceFound = true;
  }
}

// Initialize with OLED display
void BMDBLEController::begin(Adafruit_SSD1306* display) {
  begin();
  _pDisplay = display;
  _displayInitialized = true;
  
  // Initial display update
  updateDisplay();
}

// Scan for Blackmagic cameras
bool BMDBLEController::scan(uint32_t duration) {
  if (_scanInProgress) {
    return false;
  }
  
  Serial.println("Scanning for Blackmagic camera...");
  _scanInProgress = true;
  _deviceFound = false;
  
  if (_displayInitialized) {
    _pDisplay->clearDisplay();
    _pDisplay->setTextSize(1);
    _pDisplay->setCursor(0, 0);
    _pDisplay->println("BMDBLEController");
    _pDisplay->println("Scanning...");
    _pDisplay->display();
  }
  
  // Disconnect if connected
  if (_connected && _pClient) {
    _pClient->disconnect();
    _connected = false;
  }
  
  // Start scan
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(_pAdvertisedDeviceCallbacks);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(duration);
  
  return true;
}

// Connect to camera
bool BMDBLEController::connect() {
  if (!_deviceFound || _connected) {
    return false;
  }
  
  Serial.print("Connecting to camera at address: ");
  Serial.println(_pServerAddress->toString().c_str());
  
  if (_displayInitialized) {
    _pDisplay->clearDisplay();
    _pDisplay->setTextSize(1);
    _pDisplay->setCursor(0, 0);
    _pDisplay->println("BMD Camera Found");
    _pDisplay->println("Connecting...");
    _pDisplay->display();
  }
  
  // Create BLE client
  _pClient = BLEDevice::createClient();
  
  // Set up security
  BLESecurity* pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  pSecurity->setCapability(ESP_IO_CAP_IN);
  pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  
  // Connect to device
  if (!_pClient->connect(*_pServerAddress)) {
    Serial.println("Connection failed");
    if (_displayInitialized) {
      _pDisplay->clearDisplay();
      _pDisplay->setCursor(0, 0);
      _pDisplay->println("Connection failed!");
      _pDisplay->display();
    }
    return false;
  }
  
  Serial.println("Connected to camera");
  
  // Get Blackmagic service
  _pRemoteService = _pClient->getService(BLEUUID(BMD_SERVICE_UUID));
  if (_pRemoteService == nullptr) {
    Serial.println("Failed to find Blackmagic camera service");
    _pClient->disconnect();
    return false;
  }
  
  Serial.println("Found Blackmagic camera service");
  
  // Get characteristics
  _pOutgoingCameraControl = _pRemoteService->getCharacteristic(BLEUUID(BMD_OUTGOING_CONTROL_UUID));
  if (_pOutgoingCameraControl == nullptr) {
    Serial.println("Failed to find outgoing control characteristic");
    _pClient->disconnect();
    return false;
  }
  
  _pIncomingCameraControl = _pRemoteService->getCharacteristic(BLEUUID(BMD_INCOMING_CONTROL_UUID));
  if (_pIncomingCameraControl == nullptr) {
    Serial.println("Failed to find incoming control characteristic");
    _pClient->disconnect();
    return false;
  }
  
  _pTimecode = _pRemoteService->getCharacteristic(BLEUUID(BMD_TIMECODE_UUID));
  _pCameraStatus = _pRemoteService->getCharacteristic(BLEUUID(BMD_CAMERA_STATUS_UUID));
  
  // Set device name
  _pDeviceName = _pRemoteService->getCharacteristic(BLEUUID(BMD_DEVICE_NAME_UUID));
  if (_pDeviceName) {
    Serial.println("Setting device name to \"" + _deviceName + "\"");
    _pDeviceName->writeValue(_deviceName.c_str(), _deviceName.length());
  }
  
  // Register for notifications
  _pIncomingCameraControl->registerForNotify(notifyCallback, this);
  Serial.println("Enabling indications for incoming camera control");
  if (!setNotification(_pIncomingCameraControl, true, true)) {
    Serial.println("Failed to enable indications");
  }
  
  if (_pTimecode) {
    _pTimecode->registerForNotify(timecodeNotifyCallback, this);
    setNotification(_pTimecode, true, false);
  }
  
  if (_pCameraStatus) {
    _pCameraStatus->registerForNotify(statusNotifyCallback, this);
  }
  
  _connected = true;
  
  if (_displayInitialized) {
    _pDisplay->clearDisplay();
    _pDisplay->setCursor(0, 0);
    _pDisplay->println("Connected!");
    _pDisplay->println("Receiving camera data...");
    _pDisplay->display();
  }
  
  // Initial parameter requests
  requestLensInfo();
  
  return true;
}

// Disconnect from camera
bool BMDBLEController::disconnect() {
  if (!_connected || !_pClient) {
    return false;
  }
  
  _pClient->disconnect();
  _connected = false;
  
  Serial.println("Disconnected from camera");
  
  if (_displayInitialized) {
    _pDisplay->clearDisplay();
    _pDisplay->setCursor(0, 0);
    _pDisplay->println("Disconnected");
    _pDisplay->display();
  }
  
  return true;
}

// Check if connected to camera
bool BMDBLEController::isConnected() {
  if (_pClient) {
    bool actuallyConnected = _pClient->isConnected();
    if (_connected && !actuallyConnected) {
      // Connection was lost
      _connected = false;
    }
    return actuallyConnected;
  }
  return false;
}

// Clear bonding information
void BMDBLEController::clearBondingInfo() {
  preferences.begin("bmdcamera", false);
  preferences.clear();
  preferences.end();
  
  if (_pServerAddress) {
    esp_ble_remove_bond_device(*_pServerAddress->getNative());
  }
  
  // Clear bonded devices
  int dev_num = esp_ble_get_bond_device_num();
  esp_ble_bond_dev_t* dev_list = (esp_ble_bond_dev_t*)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
  esp_ble_get_bond_device_list(&dev_num, dev_list);
  for (int i = 0; i < dev_num; i++) {
    esp_ble_remove_bond_device(dev_list[i].bd_addr);
  }
  free(dev_list);
  
  Serial.println("Cleared all saved pairing information");
  
  if (_displayInitialized) {
    _pDisplay->clearDisplay();
    _pDisplay->setCursor(0, 0);
    _pDisplay->println("Pairing info");
    _pDisplay->println("cleared!");
    _pDisplay->display();
  }
}

// Set auto reconnect option
void BMDBLEController::setAutoReconnect(bool enabled) {
  _autoReconnect = enabled;
}

// Set focus using raw value (0-2048)
bool BMDBLEController::setFocus(uint16_t rawValue) {
  if (!_connected || !_pOutgoingCameraControl) {
    Serial.println("Not connected to camera");
    return false;
  }
  
  // Calculate normalized value for display
  float normalizedValue = (float)rawValue / 2048.0f;
  
  // Create focus command
  uint8_t focusCommand[12] = {
    0xFF,                    // Protocol identifier
    0x08,                    // Command length
    0x00, 0x00,              // Command ID, Reserved
    BMD_CAT_LENS,            // Category (Lens)
    BMD_PARAM_FOCUS,         // Parameter (Focus)
    BMD_TYPE_FIXED16,        // Data type (fixed16)
    BMD_OP_ASSIGN,           // Operation (assign)
    (uint8_t)(rawValue & 0xFF),          // Low byte
    (uint8_t)((rawValue >> 8) & 0xFF),   // High byte
    0x00, 0x00               // Padding
  };
  
  Serial.print("Setting focus to raw value: ");
  Serial.print(rawValue);
  Serial.print(" (");
  Serial.print(normalizedValue, 3);
  Serial.println(" normalized)");
  
  // Send command
  _pOutgoingCameraControl->writeValue(focusCommand, sizeof(focusCommand), true);
  
  return true;
}

// Set focus using normalized value (0.0-1.0)
bool BMDBLEController::setFocus(float normalizedValue) {
  // Convert normalized value to raw (0-2048)
  if (normalizedValue < 0.0) normalizedValue = 0.0;
  if (normalizedValue > 1.0) normalizedValue = 1.0;
  
  uint16_t rawValue = (uint16_t)(normalizedValue * 2048.0);
  return setFocus(rawValue);
}

// Set iris/aperture (normalized 0.0-1.0)
bool BMDBLEController::setIris(float normalizedValue) {
  if (!_connected || !_pOutgoingCameraControl) {
    Serial.println("Not connected to camera");
    return false;
  }
  
  // Clamp value
  if (normalizedValue < 0.0) normalizedValue = 0.0;
  if (normalizedValue > 1.0) normalizedValue = 1.0;
  
  // Create iris command
  uint8_t irisCommand[12] = {
    0xFF,                    // Protocol identifier
    0x08,                    // Command length
    0x00, 0x00,              // Command ID, Reserved
    BMD_CAT_LENS,            // Category (Lens)
    BMD_PARAM_IRIS_NORM,     // Parameter (Aperture normalized)
    BMD_TYPE_FIXED16,        // Data type (fixed16)
    BMD_OP_ASSIGN,           // Operation (assign)
    0x00, 0x00,              // Data bytes (will be filled)
    0x00, 0x00               // Padding
  };
  
  // Convert normalized value to fixed16 format
  int16_t fixed16Value = (int16_t)(normalizedValue * 2048.0);
  
  // Set data bytes (little endian)
  irisCommand[8] = (fixed16Value & 0xFF);         // Low byte
  irisCommand[9] = ((fixed16Value >> 8) & 0xFF);  // High byte
  
  Serial.print("Setting iris to normalized value: ");
  Serial.println(normalizedValue, 3);
  
  // Send command
  _pOutgoingCameraControl->writeValue(irisCommand, sizeof(irisCommand), true);
  
  return true;
}

// Set white balance
bool BMDBLEController::setWhiteBalance(uint16_t kelvin) {
  if (!_connected || !_pOutgoingCameraControl) {
    Serial.println("Not connected to camera");
    return false;
  }
  
  // Limit to reasonable range
  if (kelvin < 2500) kelvin = 2500;
  if (kelvin > 10000) kelvin = 10000;
  
  // Create white balance command
  uint8_t wbCommand[14] = {
    0xFF,  // Destination
    0x08,  // Length
    0x00, 0x00,  // Command ID, Reserved
    BMD_CAT_VIDEO,  // Category (Video)
    BMD_PARAM_WB,   // Parameter (White Balance)
    BMD_TYPE_INT16, // Data Type (signed 16-bit integer)
    BMD_OP_ASSIGN,  // Operation (assign)
    (uint8_t)(kelvin & 0xFF),         // WB low byte
    (uint8_t)((kelvin >> 8) & 0xFF),  // WB high byte
    0,    // Tint low byte (0 = no tint)
    0,    // Tint high byte
    0,    // Padding
    0     // Padding
  };
  
  Serial.print("Setting white balance to: ");
  Serial.print(kelvin);
  Serial.println("K");
  
  // Send command
  _pOutgoingCameraControl->writeValue(wbCommand, sizeof(wbCommand), true);
  
  return true;
}

// Toggle recording state
bool BMDBLEController::toggleRecording() {
  if (!_connected || !_pOutgoingCameraControl) {
    Serial.println("Not connected to camera");
    return false;
  }
  
  // Create recording command
  uint8_t recordingCommand[12] = {
    0xFF,  // Protocol identifier
    0x05,  // Length (5 bytes after this)
    0x00, 0x00,  // Command ID, Reserved
    BMD_CAT_TRANSPORT,  // Category (Transport/Media)
    0x01,  // Parameter (Transport mode)
    BMD_TYPE_BYTE,  // Data type (signed byte)
    BMD_OP_ASSIGN,  // Operation (assign)
    _recordingState ? 0x00 : 0x02,  // Mode (0=Preview, 2=Record)
    0x00, 0x00, 0x00  // Padding
  };
  
  Serial.print("Sending recording command: ");
  Serial.println(_recordingState ? "STOP" : "START");
  
  // Send command
  _pOutgoingCameraControl->writeValue(recordingCommand, sizeof(recordingCommand), true);
  
  // We'll update _recordingState when we get confirmation from the camera
  
  return true;
}

// Get recording state
bool BMDBLEController::isRecording() {
  return _recordingState;
}

// Execute auto focus
bool BMDBLEController::doAutoFocus() {
  if (!_connected || !_pOutgoingCameraControl) {
    Serial.println("Not connected to camera");
    return false;
  }
  
  // Create auto focus command
  uint8_t afCommand[8] = {
    0xFF,  // Protocol identifier
    0x04,  // Length
    0x00, 0x00,  // Command ID, Reserved
    BMD_CAT_LENS,  // Category (Lens)
    0x01,  // Parameter (Instantaneous autofocus)
    BMD_TYPE_VOID,  // Data type (void)
    BMD_OP_ASSIGN   // Operation (assign)
  };
  
  Serial.println("Executing auto focus");
  
  // Send command
  _pOutgoingCameraControl->writeValue(afCommand, sizeof(afCommand), true);
  
  return true;
}

// Request a specific parameter
bool BMDBLEController::requestParameter(uint8_t category, uint8_t parameterId, uint8_t dataType) {
  if (!_connected || !_pOutgoingCameraControl) {
    Serial.println("Not connected to camera");
    return false;
  }
  
  // Create request command
  uint8_t requestCommand[12] = {
    0xFF,         // Protocol identifier
    0x08,         // Command length
    0x00, 0x00,   // Command ID, Reserved
    category,     // Category
    parameterId,  // Parameter
    dataType,     // Data type
    BMD_OP_REPORT,// Operation (report/request)
    0x00, 0x00,   // Empty data
    0x00, 0x00    // Padding
  };
  
  Serial.print("Requesting Parameter - Cat: 0x");
  Serial.print(category, HEX);
  Serial.print(" Param: 0x");
  Serial.println(parameterId, HEX);
  
  // Send command
  _pOutgoingCameraControl->writeValue(requestCommand, sizeof(requestCommand), true);
  
  return true;
}

// Get parameter value as string
String BMDBLEController::getParameterValue(uint8_t category, uint8_t parameterId) {
  ParameterValue* param = findParameter(category, parameterId);
  if (param && param->valid) {
    return decodeParameterToString(param);
  }
  return "Not available";
}

// Check if parameter exists
bool BMDBLEController::hasParameter(uint8_t category, uint8_t parameterId) {
  ParameterValue* param = findParameter(category, parameterId);
  return (param != nullptr && param->valid);
}

// Update OLED display
void BMDBLEController::updateDisplay() {
  if (!_displayInitialized || !_pDisplay) {
    return;
  }
  
  _pDisplay->clearDisplay();
  _pDisplay->setTextSize(1);
  _pDisplay->setTextColor(SSD1306_WHITE);
  _pDisplay->setCursor(0, 0);
  
  if (!_connected) {
    if (_deviceFound && !_connected) {
      _pDisplay->println("BMD Camera Found");
      _pDisplay->println("Press button to connect");
    } else if (_scanInProgress) {
      _pDisplay->println("Scanning for");
      _pDisplay->println("Blackmagic camera...");
    } else {
      _pDisplay->println("BMDBLEController");
      _pDisplay->println("Not connected");
      _pDisplay->println("Press button to scan");
    }
  } else {
    // Connected state - display depends on mode
    switch (_currentDisplayMode) {
      case BMD_DISPLAY_FOCUS:
        displayFocusMode();
        break;
      
      case BMD_DISPLAY_RECORDING:
        displayRecordingMode();
        break;
      
      case BMD_DISPLAY_DEBUG:
        displayDebugMode();
        break;
      
      case BMD_DISPLAY_BASIC:
      default:
        displayBasicMode();
        break;
    }
  }
  
  _pDisplay->display();
}

// Set display mode
void BMDBLEController::setDisplayMode(uint8_t mode) {
  _currentDisplayMode = mode;
  updateDisplay();
}

// Set callback for parameter responses
void BMDBLEController::setResponseCallback(void (*callback)(uint8_t, uint8_t, String)) {
  _responseCallback = callback;
}

// Main loop function - must be called regularly
void BMDBLEController::loop() {
  // Check connection status
  if (_pClient && _connected && !_pClient->isConnected()) {
    Serial.println("Connection lost!");
    _connected = false;
    
    if (_displayInitialized) {
      _pDisplay->clearDisplay();
      _pDisplay->setCursor(0, 0);
      _pDisplay->println("Connection lost!");
      _pDisplay->display();
    }
  }
  
  // Try to reconnect if device found but not connected
  if (_deviceFound && !_connected && _autoReconnect) {
    unsigned long currentTime = millis();
    if (currentTime - _lastReconnectAttempt >= RECONNECT_INTERVAL || _lastReconnectAttempt == 0) {
      _lastReconnectAttempt = currentTime;
      connect();
    }
  }
  
  // Update display
  if (_displayInitialized && millis() - _lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    _lastDisplayUpdate = millis();
  }
}

// Find parameter by category and ID
ParameterValue* BMDBLEController::findParameter(uint8_t category, uint8_t parameterId) {
  for (int i = 0; i < _paramCount; i++) {
    if (_parameters[i].category == category && 
        _parameters[i].parameterId == parameterId &&
        _parameters[i].valid) {
      return &_parameters[i];
    }
  }
  
  return nullptr;
}

// Store a parameter
void BMDBLEController::storeParameter(uint8_t category, uint8_t parameterId, uint8_t dataType, 
                                 uint8_t operation, uint8_t* data, size_t dataLength) {
  // Look for existing parameter
  ParameterValue* param = findParameter(category, parameterId);
  
  // Create new parameter if not found
  if (param == nullptr) {
    if (_paramCount >= MAX_PARAMETERS) {
      Serial.println("Parameter storage full");
      return;
    }
    
    param = &_parameters[_paramCount++];
    param->category = category;
    param->parameterId = parameterId;
  }
  
  // Update parameter data
  param->dataType = dataType;
  param->operation = operation;
  param->dataLength = min(dataLength, (size_t)64);
  memcpy(param->data, data, param->dataLength);
  param->timestamp = millis();
  param->valid = true;
  
  // Print parameter info (debug)
  String decodedValue = decodeParameterToString(param);
  Serial.print("Received parameter - Cat: 0x");
  Serial.print(category, HEX);
  Serial.print(" Param: 0x");
  Serial.print(parameterId, HEX);
  Serial.print(" Data: ");
  Serial.println(decodedValue);
  
  // Call callback if set
  if (_responseCallback) {
    _responseCallback(category, parameterId, decodedValue);
  }
  
  // Special handling for specific parameters
  handleSpecialParameters(category, parameterId, decodedValue);
}

// Handle special parameter updates
void BMDBLEController::handleSpecialParameters(uint8_t category, uint8_t parameterId, String value) {
  // Update recording state
  if (category == BMD_CAT_TRANSPORT && parameterId == 0x01) {
    if (value.indexOf("2") != -1) {
      _recordingState = true;
      Serial.println("Recording state: RECORDING");
    } else {
      _recordingState = false;
      Serial.println("Recording state: STOPPED");
    }
  }
  
  // Update display after receiving important parameters
  if (_displayInitialized) {
    if (category == BMD_CAT_EXTENDED_LENS || 
        category == BMD_CAT_LENS ||
        (category == BMD_CAT_TRANSPORT && parameterId == 0x01) ||
        (category == BMD_CAT_VIDEO && parameterId == BMD_PARAM_WB)) {
      updateDisplay();
    }
  }
}

// Request lens information
void BMDBLEController::requestLensInfo() {
  // Request lens model
  requestParameter(BMD_CAT_EXTENDED_LENS, 0x09, BMD_TYPE_STRING);
  
  // Request focal length
  requestParameter(BMD_CAT_EXTENDED_LENS, 0x0B, BMD_TYPE_STRING);
  
  // Request focus distance
  requestParameter(BMD_CAT_EXTENDED_LENS, 0x0C, BMD_TYPE_STRING);
  
  // Request basic parameters
  requestParameter(BMD_CAT_VIDEO, BMD_PARAM_WB, BMD_TYPE_INT16);
  requestParameter(BMD_CAT_LENS, BMD_PARAM_FOCUS, BMD_TYPE_FIXED16);
  requestParameter(BMD_CAT_LENS, BMD_PARAM_IRIS_NORM, BMD_TYPE_FIXED16);
  requestParameter(BMD_CAT_TRANSPORT, 0x01, BMD_TYPE_BYTE);
}

// Process incoming packet
void BMDBLEController::processIncomingPacket(uint8_t* pData, size_t length) {
  // Verify packet is long enough
  if (length < 8) {
    Serial.println("Invalid packet: too short");
    return;
  }
  
  // Parse basic packet format
  uint8_t protocolId = pData[0];
  uint8_t packetLength = pData[1];
  uint8_t commandId = pData[2];
  uint8_t reserved = pData[3];
  uint8_t category = pData[4];
  uint8_t parameterId = pData[5];
  uint8_t dataType = pData[6];
  uint8_t operation = pData[7];
  
  // Filter out battery voltage packets (Category 0x09, Parameter 0x00) to reduce spam
  if (category == 0x09 && parameterId == 0x00) {
    // Don't return - we still want to store these parameters
  }
  
  // Store parameter value
  storeParameter(category, parameterId, dataType, operation, &pData[8], length - 8);
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
    
    Serial.print("Timecode: ");
    Serial.println(_timecodeStr);
  }
}

// Process status packet
void BMDBLEController::processStatusPacket(uint8_t* pData, size_t length) {
  if (length > 0) {
    _cameraStatus = pData[0];
    Serial.print("Camera status: 0x");
    Serial.println(_cameraStatus, HEX);
  }
}

// Display basic mode
void BMDBLEController::displayBasicMode() {
  // Header with lens model if available
  ParameterValue* lensModel = findParameter(BMD_CAT_EXTENDED_LENS, 0x09);
  if (lensModel && lensModel->valid) {
    String model = decodeParameterToString(lensModel);
    _pDisplay->println(model.substring(0, 21)); // Limit length to fit display
  } else {
    _pDisplay->println("BMD Camera Connected");
  }
  
  // Recording state and timecode
  if (_recordingState) {
    _pDisplay->print("REC  ");
    // Blinking REC symbol
    if (millis() % 1000 < 500) {
      _pDisplay->fillCircle(25, 4, 3, SSD1306_WHITE);
    }
  } else {
    _pDisplay->print("STBY ");
  }
  
  _pDisplay->println(_timecodeStr);
  
  // White balance if available
  ParameterValue* wb = findParameter(BMD_CAT_VIDEO, BMD_PARAM_WB);
  if (wb && wb->valid) {
    _pDisplay->print("WB: ");
    _pDisplay->println(decodeParameterToString(wb));
  }
  
  // Focal length and focus distance if available
  ParameterValue* focalLength = findParameter(BMD_CAT_EXTENDED_LENS, 0x0B);
  ParameterValue* focusDistance = findParameter(BMD_CAT_EXTENDED_LENS, 0x0C);
  
  if (focalLength && focalLength->valid) {
    _pDisplay->print(decodeParameterToString(focalLength));
    _pDisplay->print("  ");
  }
  
  if (focusDistance && focusDistance->valid) {
    _pDisplay->println(decodeParameterToString(focusDistance));
  } else {
    _pDisplay->println("");
  }
  
  // Battery voltage if available
  ParameterValue* batteryVoltage = findParameter(0x09, 0x00);
  if (batteryVoltage && batteryVoltage->valid && batteryVoltage->dataLength >= 2) {
    int16_t rawVoltage = (int16_t)(batteryVoltage->data[0] | (batteryVoltage->data[1] << 8));
    float voltage = (float)rawVoltage / 1000.0f;
    _pDisplay->print("Batt: ");
    _pDisplay->print(voltage, 1);
    _pDisplay->println("V");
  }
  
  // Focus value if available
  ParameterValue* focus = findParameter(BMD_CAT_LENS, BMD_PARAM_FOCUS);
  if (focus && focus->valid) {
    int16_t rawValue = (int16_t)(focus->data[0] | (focus->data[1] << 8));
    float normalizedValue = (float)rawValue / 2048.0f;
    
    _pDisplay->print("Focus: ");
    _pDisplay->println(normalizedValue, 3);
    
    // Draw focus position bar
    _pDisplay->drawRect(0, 56, 128, 8, SSD1306_WHITE);
    int barWidth = (int)(normalizedValue * 126);
    _pDisplay->fillRect(1, 57, barWidth, 6, SSD1306_WHITE);
  }
}

// Display recording mode
void BMDBLEController::displayRecordingMode() {
  // Header with recording state
  if (_recordingState) {
    _pDisplay->print("REC  ");
    // Blinking REC symbol
    if (millis() % 1000 < 500) {
      _pDisplay->fillCircle(25, 4, 3, SSD1306_WHITE);
    }
  } else {
    _pDisplay->print("STBY ");
  }
  
  // Show timecode
  _pDisplay->println(_timecodeStr);
  
  // Lens info if available
  ParameterValue* lensModel = findParameter(BMD_CAT_EXTENDED_LENS, 0x09);
  if (lensModel && lensModel->valid) {
    String model = decodeParameterToString(lensModel);
    _pDisplay->println(model.substring(0, 21)); // Limit length to fit display
  }
  
  // Show white balance
  ParameterValue* wb = findParameter(BMD_CAT_VIDEO, BMD_PARAM_WB);
  if (wb && wb->valid) {
    _pDisplay->print("WB: ");
    _pDisplay->print(decodeParameterToString(wb));
    _pDisplay->print("  ");
  }
  
  // Show iris/aperture if available
  ParameterValue* iris = findParameter(BMD_CAT_LENS, BMD_PARAM_IRIS_NORM);
  if (iris && iris->valid) {
    int16_t rawValue = (int16_t)(iris->data[0] | (iris->data[1] << 8));
    float normalizedValue = (float)rawValue / 2048.0f;
    _pDisplay->print("f/");
    _pDisplay->println(2.8 + normalizedValue * 19.2, 1); // Approximate f-stop conversion
  } else {
    _pDisplay->println("");
  }
  
  // Show remaining card space if available (future)
  
  // Battery voltage if available
  ParameterValue* batteryVoltage = findParameter(0x09, 0x00);
  if (batteryVoltage && batteryVoltage->valid && batteryVoltage->dataLength >= 2) {
    int16_t rawVoltage = (int16_t)(batteryVoltage->data[0] | (batteryVoltage->data[1] << 8));
    float voltage = (float)rawVoltage / 1000.0f;
    _pDisplay->print("Batt: ");
    _pDisplay->print(voltage, 1);
    _pDisplay->println("V");
  }
  
  // Show clip filename if recording
  ParameterValue* clipFilename = findParameter(BMD_CAT_EXTENDED_LENS, 0x0F); // Clip filename
  if (_recordingState && clipFilename && clipFilename->valid) {
    String filename = decodeParameterToString(clipFilename);
    if (filename.length() > 2) { // Valid filename
      _pDisplay->println(filename.substring(0, 21)); // Limit length to fit display
    }
  }
}

// Display debug mode - show raw parameter values
void BMDBLEController::displayDebugMode() {
  _pDisplay->println("Debug Mode");
  _pDisplay->print("Params: ");
  _pDisplay->println(_paramCount);
  
  // Show last 4 parameters received
  int startIdx = max(0, _paramCount - 4);
  for (int i = startIdx; i < _paramCount; i++) {
    if (_parameters[i].valid) {
      _pDisplay->print("0x");
      _pDisplay->print(_parameters[i].category, HEX);
      _pDisplay->print(",0x");
      _pDisplay->print(_parameters[i].parameterId, HEX);
      _pDisplay->print(": ");
      
      // Show first few bytes
      for (int j = 0; j < min(4, (int)_parameters[i].dataLength); j++) {
        if (_parameters[i].data[j] < 0x10) _pDisplay->print("0");
        _pDisplay->print(_parameters[i].data[j], HEX);
        _pDisplay->print(" ");
      }
      _pDisplay->println("");
    }
  }
  
  // Show connection info
  _pDisplay->print("Conn: ");
  _pDisplay->println(_connected ? "Yes" : "No");
  _pDisplay->print("BLE: ");
  if (_pClient) {
    _pDisplay->println(_pClient->isConnected() ? "Connected" : "Disconnected");
  } else {
    _pDisplay->println("No client");
  }
}

// Display focus mode
void BMDBLEController::displayFocusMode() {
  // Title with lens model if available
  ParameterValue* lensModel = findParameter(BMD_CAT_EXTENDED_LENS, 0x09);
  if (lensModel && lensModel->valid) {
    String model = decodeParameterToString(lensModel);
    _pDisplay->println(model.substring(0, 21)); // Limit length to fit display
  } else {
    _pDisplay->println("BMD Camera");
  }
  
  // Show focus value
  ParameterValue* focus = findParameter(BMD_CAT_LENS, BMD_PARAM_FOCUS);
  if (focus && focus->valid) {
    String focusStr = decodeParameterToString(focus);
    _pDisplay->println("Focus: " + focusStr);
    
    // Extract normalized value and draw focus bar
    int16_t rawValue = (int16_t)(focus->data[0] | (focus->data[1] << 8));
    float normalizedValue = (float)rawValue / 2048.0f;
    
    // Draw focus position bar
    _pDisplay->drawRect(0, 25, 128, 8, SSD1306_WHITE);
    int barWidth = (int)(normalizedValue * 126);
    _pDisplay->fillRect(1, 26, barWidth, 6, SSD1306_WHITE);
  } else {
    _pDisplay->println("Focus: Unknown");
  }
  
  // Show focal length if available
  ParameterValue* focalLength = findParameter(BMD_CAT_EXTENDED_LENS, 0x0B);
  if (focalLength && focalLength->valid) {
    String flStr = decodeParameterToString(focalLength);
    _pDisplay->setCursor(0, 35);
    _pDisplay->print(flStr);
  }
  
  // Show recording state
  _pDisplay->setCursor(50, 35);
  if (_recordingState) {
    _pDisplay->print("REC");
    
    // Blinking REC dot
    if (millis() % 1000 < 500) {
      _pDisplay->fillCircle(45, 38, 3, SSD1306_WHITE);
    }
  } else {
    _pDisplay->print("STBY");
  }
  
  // Show timecode if available
  _pDisplay->setCursor(0, 45);
  _pDisplay->print("TC: ");
  _pDisplay->print(_timecodeStr);
  
  // Show aperture if available
  ParameterValue* iris = findParameter(BMD_CAT_LENS, BMD_PARAM_IRIS_NORM);
  if (iris && iris->valid) {
    _pDisplay->setCursor(0, 55);
    _pDisplay->print("Iris: ");
    
    int16_t rawValue = (int16_t)(iris->data[0] | (iris->data[1] << 8));
    float normalizedValue = (float)rawValue / 2048.0f;
    _pDisplay->print(normalizedValue, 2);
  }
}
