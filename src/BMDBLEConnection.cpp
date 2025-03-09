// Start scanning for Blackmagic cameras
bool BMDBLEController::scan(uint32_t duration) {
  // Update connection state
  _connectionState = BMD_STATE_SCANNING;
  _deviceFound = false;
  
  // Trigger callback
  if (_connectionCallback) {
    _connectionCallback(_connectionState);
  }
  
  Serial.print("Scanning for Blackmagic camera (");
  Serial.print(duration);
  Serial.println(" seconds)...");
  
  // Start scan
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(_pScanCallbacks);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(duration);
  
  return true;
}

// Connect to camera
bool BMDBLEController::connect() {
  if (!_deviceFound || _pServerAddress == nullptr) {
    Serial.println("No camera found to connect to");
    return false;
  }
  
  // Update connection state
  _connectionState = BMD_STATE_CONNECTING;
  
  // Trigger callback
  if (_connectionCallback) {
    _connectionCallback(_connectionState);
  }
  
  Serial.print("Connecting to camera at address: ");
  Serial.println(_pServerAddress->toString().c_str());
  
  // Create a BLE client
  if (_pClient != nullptr) {
    delete _pClient;
  }
  _pClient = BLEDevice::createClient();
  
  // Set up security
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  BLEDevice::setSecurityCallbacks(_pSecurityCallbacks);
  
  BLESecurity* pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  pSecurity->setCapability(ESP_IO_CAP_IN);
  pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  
  // Connect to the camera
  if (!_pClient->connect(*_pServerAddress)) {
    Serial.println("Connection failed");
    _connectionState = BMD_STATE_DISCONNECTED;
    
    // Trigger callback
    if (_connectionCallback) {
      _connectionCallback(_connectionState);
    }
    return false;
  }
  
  Serial.println("Connected to camera");
  
  // Get the Blackmagic camera service
  _pRemoteService = _pClient->getService(BLEUUID(BMD_SERVICE_UUID));
  if (_pRemoteService == nullptr) {
    Serial.println("Failed to find Blackmagic camera service");
    _pClient->disconnect();
    _connectionState = BMD_STATE_DISCONNECTED;
    
    // Trigger callback
    if (_connectionCallback) {
      _connectionCallback(_connectionState);
    }
    return false;
  }
  
  // ...rest of the method remains the same...
}
