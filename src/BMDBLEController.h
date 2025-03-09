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

// Connection state enum
enum BMDConnectionState {
  BMD_STATE_DISCONNECTED,
  BMD_STATE_SCANNING,
  BMD_STATE_CONNECTING,
  BMD_STATE_CONNECTED
};

// Forward declaration of BLE callback classes
class BLEAdvertisedDeviceCallbacks;
class BLESecurityCallbacks;

class BMDBLEController {
public:
  // Constructor & initialization
  BMDBLEController(String deviceName = "ESP32BMDControl");
  ~BMDBLEController();
  void begin();
  
  // Connection management
  bool scan(uint32_t duration = 10);
  bool connect();
  bool disconnect();
  bool isConnected();
  void clearBondingInfo();
  void setAutoReconnect(bool enabled);
  BMDConnectionState getConnectionState();
  
  // Camera control methods
  bool setFocus(uint16_t rawValue);
  bool setFocus(float normalizedValue);
  bool setIris(float normalizedValue);
  bool setWhiteBalance(uint16_t kelvin);
  bool toggleRecording();
  bool startRecording();
  bool stopRecording();
  bool doAutoFocus();
  bool isRecording();
  
  // Parameter methods
  bool requestParameter(uint8_t category, uint8_t parameterId, uint8_t dataType);
  bool hasParameter(uint8_t category, uint8_t parameterId);
  ParameterValue* getParameter(uint8_t category, uint8_t parameterId);
  
  // Event handling
  typedef void (*ParameterCallback)(uint8_t category, uint8_t parameterId, uint8_t* data, size_t length);
  typedef void (*ConnectionCallback)(BMDConnectionState state);
  typedef void (*TimecodeCallback)(uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t frames);
  typedef void (*PinRequestCallback)(uint32_t* pinCode);
  
  void setParameterCallback(ParameterCallback callback);
  void setConnectionCallback(ConnectionCallback callback);
  void setTimecodeCallback(TimecodeCallback callback);
  void setPinRequestCallback(PinRequestCallback callback);
  
  void loop(); // Must be called in Arduino loop()
  
private:
  // BLE connection handling
  bool setNotification(BLERemoteCharacteristic* pChar, bool enable, bool isIndication);
  void checkConnection();
  bool reconnect();
  
  // Parameter storage and decoding
  ParameterValue* findParameter(uint8_t category, uint8_t parameterId);
  void storeParameter(uint8_t category, uint8_t parameterId, 
                      uint8_t dataType, uint8_t operation, uint8_t* data, size_t dataLength);
  
  // Static callbacks
  static void controlNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                           uint8_t* pData, size_t length, bool isNotify);
  
  static void timecodeNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                            uint8_t* pData, size_t length, bool isNotify);
  
  static void statusNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                          uint8_t* pData, size_t length, bool isNotify);
  
  // Packet processing
  void processIncomingPacket(uint8_t* pData, size_t length);
  void processTimecodePacket(uint8_t* pData, size_t length);
  void processStatusPacket(uint8_t* pData, size_t length);
  
  // Member variables
  String _deviceName;
  BMDConnectionState _connectionState;
  bool _deviceFound;
  bool _autoReconnect;
  bool _recordingState;
  uint8_t _cameraStatus;
  
  BLEClient* _pClient;
  BLEAddress* _pServerAddress;
  BLERemoteService* _pRemoteService;
  BLERemoteCharacteristic* _pOutgoingCameraControl;
  BLERemoteCharacteristic* _pIncomingCameraControl;
  BLERemoteCharacteristic* _pTimecode;
  BLERemoteCharacteristic* _pCameraStatus;
  BLERemoteCharacteristic* _pDeviceName;
  
  BLEAdvertisedDeviceCallbacks* _pScanCallbacks;
  BLESecurityCallbacks* _pSecurityCallbacks;
  
  // Parameter storage
  #define MAX_PARAMETERS 64
  ParameterValue _parameters[MAX_PARAMETERS];
  int _paramCount;
  
  // Timecode storage
  uint8_t _timecodeHours;
  uint8_t _timecodeMinutes;
  uint8_t _timecodeSeconds;
  uint8_t _timecodeFrames;
  
  // Timing variables
  unsigned long _lastReconnectAttempt;
  const unsigned long RECONNECT_INTERVAL = 5000; // 5 seconds
  
  // Callbacks
  ParameterCallback _parameterCallback;
  ConnectionCallback _connectionCallback;
  TimecodeCallback _timecodeCallback;
  PinRequestCallback _pinRequestCallback;
  
  static BMDBLEController* _instance; // Used for callbacks
  
  // Preferences for storing connection info
  Preferences _preferences;
};

#endif // BMD_BLE_CONTROLLER_H
