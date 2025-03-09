#include "BMDBLEController.h"

// Static instance for callbacks
BMDBLEController* BMDBLEController::_instance = nullptr;

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
