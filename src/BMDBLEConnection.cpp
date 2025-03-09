#include "BMDBLEController.h"

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
  
  // Start scan - use correct type casting
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks((BLEAdvertisedDeviceCallbacks*)_pScanCallbacks);
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
  
  // Set up security - use correct type casting
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  BLEDevice::setSecurityCallbacks((BLESecurityCallbacks*)_pSecurityCallbacks);
  
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
  
  Serial.println("Found Blackmagic camera service");
  
  // Get characteristic references
  _pOutgoingCameraControl = _pRemoteService->getCharacteristic(BLEUUID(BMD_OUTGOING_CONTROL_UUID));
  if (_pOutgoingCameraControl == nullptr) {
    Serial.println("Failed to find outgoing control characteristic");
    _pClient->disconnect();
    _connectionState = BMD_STATE_DISCONNECTED;
    
    // Trigger callback
    if (_connectionCallback) {
      _connectionCallback(_connectionState);
    }
    return false;
  }
  
  Serial.println("Found outgoing control characteristic");
  
  _pIncomingCameraControl = _pRemoteService->getCharacteristic(BLEUUID(BMD_INCOMING_CONTROL_UUID));
  if (_pIncomingCameraControl == nullptr) {
    Serial.println("Failed to find incoming control characteristic");
    _pClient->disconnect();
    _connectionState = BMD_STATE_DISCONNECTED;
    
    // Trigger callback
    if (_connectionCallback) {
      _connectionCallback(_connectionState);
    }
    return false;
  }
  
  Serial.println("Found incoming control characteristic");
  
  // Get optional characteristics
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
    _pIncomingCameraControl->registerForNotify(controlNotifyCallback);
    if (!setNotification(_pIncomingCameraControl, true, true)) {
      Serial.println("Failed to enable indications for Incoming Camera Control");
    }
  }
  
  if (_pTimecode) {
    _pTimecode->registerForNotify(timecodeNotifyCallback);
    if (!setNotification(_pTimecode, true, false)) {
      Serial.println("Failed to enable notifications for Timecode");
    }
  }
  
  if (_pCameraStatus) {
    _pCameraStatus->registerForNotify(statusNotifyCallback);
  }
  
  // Update connection state
  _connectionState = BMD_STATE_CONNECTED;
  
  // Trigger callback
  if (_connectionCallback) {
    _connectionCallback(_connectionState);
  }
  
  return true;
}

// The rest of this file remains unchanged
