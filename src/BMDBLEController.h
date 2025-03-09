#ifndef BMD_BLE_CONTROLLER_H
#define BMD_BLE_CONTROLLER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "BMDBLEConstants.h"

// Parameter value storage structure
struct ParameterValue {
  uint8_t category;          // Parameter category
  uint8_t parameterId;       // Parameter ID
  uint8_t dataType;          // Data type
  uint8_t operation;         // Operation type
  uint8_t data[64];          // Raw data (up to 64 bytes per parameter)
  size_t dataLength;         // Actual data length
  unsigned long timestamp;   // When the value was last updated
  bool valid;                // If parameter contains valid data
};

class BMDBLEController {
public:
  // Constructor & initialization
  BMDBLEController(String deviceName = "ESP32BMDControl");
  void begin();
  void begin(Adafruit_SSD1306* display);
  
  // Connection management
  bool scan(uint32_t duration = 10);
  bool connect();
  bool disconnect();
  bool isConnected();
  void clearBondingInfo();
  void setAutoReconnect(bool enabled);
  
  // Camera control methods
  bool setFocus(uint16_t rawValue);
  bool setFocus(float normalizedValue);
  bool setIris(float normalizedValue);
  bool setWhiteBalance(uint16_t kelvin);
  bool toggleRecording();
  bool doAutoFocus();
  bool isRecording();
  
  // Parameter methods
  bool requestParameter(uint8_t category, uint8_t parameterId, uint8_t dataType);
  String getParameterValue(uint8_t category, uint8_t parameterId);
  bool hasParameter(uint8_t category, uint8_t parameterId);
  
  // Display methods
  void updateDisplay();
  void setDisplayMode(uint8_t mode);
  
  // Event handling
  void setResponseCallback(void (*callback)(uint8_t, uint8_t, String));
  void loop(); // Must be called in Arduino loop()
  
private:
  // BLE connection handling
  bool setNotification(BLERemoteCharacteristic* pChar, bool enable, bool isIndication);
  void handleSpecialParameters(uint8_t category, uint8_t parameterId, String value);
  void requestLensInfo();
  
  // Parameter storage
  ParameterValue* findParameter(uint8_t category, uint8_t parameterId);
  void storeParameter(uint8_t category, uint8_t parameterId, 
                     uint8_t dataType, uint8_t operation, uint8_t* data, size_t dataLength);
  String decodeParameterToString(ParameterValue* param);
  
  // Display helper methods
  void displayBasicMode();
  void displayFocusMode();
  void displayRecordingMode();
  void displayDebugMode();
  
  // Static callbacks
  static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                            uint8_t* pData, size_t length, bool isNotify, void* pThis) {
    ((BMDBLEController*)pThis)->processIncomingPacket(pData, length);
  }
  
  static void timecodeNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                    uint8_t* pData, size_t length, bool isNotify, void* pThis) {
    ((BMDBLEController*)pThis)->processTimecodePacket(pData, length);
  }
  
  static void statusNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                  uint8_t* pData, size_t length, bool isNotify, void* pThis) {
    ((BMDBLEController*)pThis)->processStatusPacket(pData, length);
  }
  
  // Processing methods
  void processIncomingPacket(uint8_t* pData, size_t length);
  void processTimecodePacket(uint8_t* pData, size_t length);
  void processStatusPacket(uint8_t* pData, size_t length);
  
  // BLE callback classes
  class BMDAdvertisedDeviceCallbacks;
  class BMDSecurityCallbacks;
  
  // Member variables
  String _deviceName;
  bool _connected;
  bool _deviceFound;
  bool _scanInProgress;
  bool _autoReconnect;
  bool _displayInitialized;
  bool _recordingState;
  uint8_t _currentDisplayMode;
  
  BLEClient* _pClient;
  BLEAddress* _pServerAddress;
  BLERemoteService* _pRemoteService;
  BLERemoteCharacteristic* _pOutgoingCameraControl;
  BLERemoteCharacteristic* _pIncomingCameraControl;
  BLERemoteCharacteristic* _pTimecode;
  BLERemoteCharacteristic* _pCameraStatus;
  BLERemoteCharacteristic* _pDeviceName;
  
  BMDAdvertisedDeviceCallbacks* _pAdvertisedDeviceCallbacks;
  BMDSecurityCallbacks* _pSecurityCallbacks;
  
  Adafruit_SSD1306* _pDisplay;
  
  // Parameter storage
  #define MAX_PARAMETERS 64
  ParameterValue _parameters[MAX_PARAMETERS];
  int _paramCount;
  
  // Timecode and status
  String _timecodeStr;
  uint8_t _cameraStatus;
  
  // Timing variables
  unsigned long _lastReconnectAttempt;
  unsigned long _lastDisplayUpdate;
  const unsigned long RECONNECT_INTERVAL = 5000; // 5 seconds
  const unsigned long DISPLAY_UPDATE_INTERVAL = 200; // 200ms
  
  // Callback for parameter responses
  void (*_responseCallback)(uint8_t, uint8_t, String);
  
  // Preferences for storing connection info
  Preferences preferences;
};

#endif // BMD_BLE_CONTROLLER_H
