// Advertised device callbacks class
class BMDScanCallbacks: public BLEAdvertisedDeviceCallbacks {
private:
  BMDBLEController* _controller;
  
public:
  BMDScanCallbacks(BMDBLEController* controller) : _controller(controller) {}
  
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
