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
  void loop();
  
  // Connection methods
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
  
  // Raw command methods
  bool sendRawCommand(uint8_t* commandData, size_t length);
  bool sendFormattedCommand(uint8_t category, uint8_t parameter, 
                           uint8_t dataType, uint8_t operation,
                           uint8_t* data, size_t dataLength);
  
  // Parameter methods
  bool requestParameter(uint8_t category, uint8_t parameterId, uint8_t dataType);
  bool hasParameter(uint8_t category, uint8_t parameterId);
  unsigned long getParameterTimestamp(uint8_t category, uint8_t parameterId);
  
  // Parameter access methods
  String getParameterAsString(uint8_t category, uint8_t parameterId);
  int32_t getParameterAsInt(uint8_t category, uint8_t parameterId);
  float getParameterAsFloat(uint8_t category, uint8_t parameterId);
  String getParameterAsHexString(uint8_t category, uint8_t parameterId);
  bool getRawParameter(uint8_t category, uint8_t parameterId, uint8_t* buffer, size_t* bufferSize);
  
  // Event handling
  void setResponseCallback(void (*callback)(uint8_t, uint8_t, String));
  void setConnectionCallback(void (*callback)(bool));
  
  // Public for testing/debugging (would normally be private)
  bool _deviceFound;

private:
  // BLE connection handling
  bool setNotification(BLERemoteCharacteristic* pChar, bool enable, bool isIndication);
  String extractTextData(uint8_t* pData, size_t length, int offset);
  
  // Parameter storage
  ParameterValue* findParameter(uint8_t category, uint8_t parameterId);
  void storeParameter(uint8_t category, uint8_t parameterId, 
                     uint8_t dataType, uint8_t operation, uint8_t* data, size_t dataLength);
  String decodeParameterToString(ParameterValue* param);
  
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
  
  // Member variables
  String _deviceName;
  bool _connected;
  bool _scanInProgress;
  bool _autoReconnect;
  bool _recordingState;
  
  BLEClient* _pClient;
  BLEAddress* _pServerAddress;
  BLERemoteService* _pRemoteService;
  BLERemoteCharacteristic* _pOutgoingCameraControl;
  BLERemoteCharacteristic* _pIncomingCameraControl;
  BLERemoteCharacteristic* _pTimecode;
  BLERemoteCharacteristic* _pCameraStatus;
  BLERemoteCharacteristic* _pDeviceName;
  
  // Parameter storage
  #define MAX_PARAMETERS 64
  ParameterValue _parameters[MAX_PARAMETERS];
  int _paramCount;
  
  // Timecode and status
  String _timecodeStr;
  uint8_t _cameraStatus;
  
  // Timing variables
  unsigned long _lastReconnectAttempt;
  const unsigned long RECONNECT_INTERVAL = 5000; // 5 seconds
  
  // Callbacks
  void (*_responseCallback)(uint8_t, uint8_t, String);
  void (*_connectionCallback)(bool);
  
  // Preferences for storing connection info
  Preferences _preferences;
  
  // Security callback class
  class BMDSecurityCallbacks : public BLESecurityCallbacks {
  public:
    BMDSecurityCallbacks(BMDBLEController* controller) : _controller(controller) {}
    
    uint32_t onPassKeyRequest();
    void onPassKeyNotify(uint32_t pass_key) {}
    bool onConfirmPIN(uint32_t pin) { return true; }
    bool onSecurityRequest() { return true; }
    void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl);
    
  private:
    BMDBLEController* _controller;
  };
  
  // BLE scan callback class
  class BMDAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  public:
    BMDAdvertisedDeviceCallbacks(BMDBLEController* controller) : _controller(controller) {}
    
    void onResult(BLEAdvertisedDevice advertisedDevice);
    
  private:
    BMDBLEController* _controller;
  };
  
  BMDSecurityCallbacks* _pSecurityCallbacks;
  BMDAdvertisedDeviceCallbacks* _pAdvertisedDeviceCallbacks;
};

#endif // BMD_BLE_CONTROLLER_H
