#ifndef BMD_BLE_CONTROLLER_H
#define BMD_BLE_CONTROLLER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <Preferences.h>
#include "BMDBLEConstants.h"

// Forward declarations
class BMDScanCallbacks;
class BMDSecurityCallbacks;

// Connection states
enum BMDConnectionState {
  BMD_STATE_DISCONNECTED,
  BMD_STATE_SCANNING,
  BMD_STATE_CONNECTING,
  BMD_STATE_CONNECTED
};

// Parameter value structure
struct ParameterValue {
  bool valid;
  uint8_t category;
  uint8_t parameterId;
  uint8_t dataType;
  uint8_t operation;
  uint8_t data[64];
  size_t dataLength;
  unsigned long timestamp;
};

// Callback type definitions
typedef void (*ParameterCallback)(uint8_t category, uint8_t parameterId, uint8_t* data, size_t length);
typedef void (*ConnectionCallback)(BMDConnectionState state);
typedef void (*TimecodeCallback)(uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t frames);
typedef void (*PinRequestCallback)(uint32_t* pinCode);

// Maximum number of parameters to store
#define MAX_PARAMETERS 32

// Reconnection interval
#define RECONNECT_INTERVAL 5000 // 5 seconds

class BMDBLEController {
  friend class BMDScanCallbacks;
  friend class BMDSecurityCallbacks;
  
private:
  // Static instance for callbacks
  static BMDBLEController* _instance;
  
  // Connection variables
  String _deviceName;
  BMDConnectionState _connectionState;
  bool _deviceFound;
  bool _autoReconnect;
  bool _recordingState;
  uint8_t _cameraStatus;
  
  // BLE objects
  BLEClient* _pClient;
  BLEAddress* _pServerAddress;
  BLERemoteService* _pRemoteService;
  BLERemoteCharacteristic* _pOutgoingCameraControl;
  BLERemoteCharacteristic* _pIncomingCameraControl;
  BLERemoteCharacteristic* _pTimecode;
  BLERemoteCharacteristic* _pCameraStatus;
  BLERemoteCharacteristic* _pDeviceName;
  
  // Callback objects
  BMDScanCallbacks* _pScanCallbacks;
  BMDSecurityCallbacks* _pSecurityCallbacks;
  
  // Parameter storage
  ParameterValue _parameters[MAX_PARAMETERS];
  int _paramCount;
  
  // Timecode data
  uint8_t _timecodeHours;
  uint8_t _timecodeMinutes;
  uint8_t _timecodeSeconds;
  uint8_t _timecodeFrames;
  
  // Reconnection tracking
  unsigned long _lastReconnectAttempt;
  
  // Preferences for storage
  Preferences _preferences;
  
  // Callback functions
  ParameterCallback _parameterCallback;
  ConnectionCallback _connectionCallback;
  TimecodeCallback _timecodeCallback;
  PinRequestCallback _pinRequestCallback;
  
  // Private methods
  ParameterValue* findParameter(uint8_t category, uint8_t parameterId);
  void storeParameter(uint8_t category, uint8_t parameterId, uint8_t dataType, uint8_t operation, uint8_t* data, size_t dataLength);
  bool setNotification(BLERemoteCharacteristic* pChar, bool enable, bool isIndication);
  
  // Packet processing methods
  void processIncomingPacket(uint8_t* pData, size_t length);
  void processTimecodePacket(uint8_t* pData, size_t length);
  void processStatusPacket(uint8_t* pData, size_t length);
  
  // Static callback functions
  static void controlNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
  static void timecodeNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
  static void statusNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
  
public:
  // Constructor and destructor
  BMDBLEController(String deviceName = "ESP32BMDController");
  ~BMDBLEController();
  
  // Initialization
  void begin();
  void loop();
  
  // Connection management
  bool scan(uint32_t duration = 10);
  bool connect();
  bool reconnect();
  bool disconnect();
  void clearBondingInfo();
  void setAutoReconnect(bool enabled);
  void checkConnection();
  
  // Camera commands
  bool setFocus(uint16_t rawValue);
  bool setFocus(float normalizedValue);
  bool setIris(float normalizedValue);
  bool setWhiteBalance(uint16_t kelvin);
  bool doAutoFocus();
  bool toggleRecording();
  bool startRecording();
  bool stopRecording();
  bool requestParameter(uint8_t category, uint8_t parameterId, uint8_t dataType);
  
  // Callback registration
  void setParameterCallback(ParameterCallback callback);
  void setConnectionCallback(ConnectionCallback callback);
  void setTimecodeCallback(TimecodeCallback callback);
  void setPinRequestCallback(PinRequestCallback callback);
  
  // State getters
  BMDConnectionState getConnectionState();
  bool isConnected();
  bool isRecording();
  
  // Parameter access
  bool hasParameter(uint8_t category, uint8_t parameterId);
  ParameterValue* getParameter(uint8_t category, uint8_t parameterId);
};

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

#endif // BMD_BLE_CONTROLLER_H
