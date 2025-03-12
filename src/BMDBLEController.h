#ifndef BMDBLECONTROLLER_H
#define BMDBLECONTROLLER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>

// BMD Camera Control Service and Characteristic UUIDs (Keep these as defines)
#define BMD_SERVICE_UUID "291d567a-6d75-11e6-8b77-86f30ca893d3"
#define BMD_CHARACTERISTIC_UUID "5dd3465f-1aee-4299-8493-d2eca2f5e1bb"

class BMDBLEController {
public:
    BMDBLEController();
    bool connect(const char* cameraAddress);
    void disconnect();
    bool isConnected() const; // Use const for getter methods
    bool sendCommand(uint16_t command, uint16_t parameter);
    bool startScan(uint32_t scanDuration = 5);  // Make scan duration configurable

    // Higher-level command functions (examples - add more as needed)
    bool startRecording();
    bool stopRecording();
    bool setISO(uint16_t isoValue);
    bool setShutterAngle(float angle);
    bool setAperture(float apertureValue);
    bool setWhiteBalance(uint16_t whiteBalanceValue);
    bool setNDFilter(uint8_t ndFilterValue); //ND filters are integer.
    bool getBatteryLevel();

    // Callbacks (optional, but highly recommended for asynchronous handling)
    void setOnConnectCallback(void (*callback)(void)) { onConnectCallback = callback; }
    void setOnDisconnectCallback(void (*callback)(void)) { onDisconnectCallback = callback; }
    void setOnDataReceivedCallback(void (*callback)(uint8_t*, size_t)) { onDataReceivedCallback = callback;}

private:
    static void notifyCallback(
        BLERemoteCharacteristic* pBLERemoteCharacteristic,
        uint8_t* pData,
        size_t length,
        bool isNotify);

    bool connected;
    BLEAddress *pServerAddress;
    BLERemoteCharacteristic* pRemoteCharacteristic;
    BLEClient* pClient;
    bool doConnect = false;

    // Callbacks
    void (*onConnectCallback)(void);
    void (*onDisconnectCallback)(void);
    void (*onDataReceivedCallback)(uint8_t*, size_t);

    bool connectToServer();
};

#endif
