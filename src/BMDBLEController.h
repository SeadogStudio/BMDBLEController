#ifndef BMDBLECONTROLLER_H
#define BMDBLECONTROLLER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <Preferences.h>
#include <map>
#include <tuple>

class BMDBLEController {
public:
    // Constructor and Destructor
    BMDBLEController(const String& deviceName = "ESP32-BMDControl");
    ~BMDBLEController();

    // Connection Management
    bool connect();
    bool connect(const BLEAddress& address);
    void disconnect();
    bool isConnected() const;
    bool isBonded() const;

    // Scanning
    void startScan(uint32_t duration = 10);
    void stopScan();

    // Camera Control Commands (Setters only - NO Getters for active fetching)
    bool setWhiteBalance(uint16_t kelvin, int16_t tint = 0);
    bool setISO(uint16_t iso);
    bool setShutterAngle(uint16_t angle); // Or shutter speed
    bool setNDFilter(uint8_t nd);
    bool startRecording();
    bool stopRecording();

    // Getters (Accessors for last known values)
    int16_t getCurrentWhiteBalance() const;
    int16_t getCurrentTint() const; // Separate tint
    int16_t getCurrentISO() const;
    int16_t getCurrentShutterAngle() const;
    // uint8_t getCurrentNDFilter() const; // If ND is a single byte

    // Callbacks (as before, but emphasize onStatusNotify)
    void setStatusChangeCallback(void (*callback)(uint8_t status));
    void setDeviceFoundCallback(void (*callback)(const BLEAdvertisedDevice& device));
    void setConnectionStateChangeCallback(void (*callback)(bool connected));
    void setPinRequestCallback(uint32_t (*callback)());

    // Utility Functions
    void clearBondingInformation();
    static String errorCodeToString(int16_t errorCode);

    // --- Error Codes ---
    enum ErrorCode : int16_t {
        ERR_NONE = 0,
        ERR_NOT_CONNECTED = -1,
        ERR_CHARACTERISTIC_NOT_FOUND = -2,
        ERR_COMMAND_FAILED = -3,
        ERR_INVALID_PARAMETER = -4,
        ERR_SCAN_FAILED = -5,
        ERR_CONNECTION_FAILED = -6,
        ERR_SERVICE_NOT_FOUND = -7,
        ERR_ALREADY_CONNECTED = -8,
        ERR_ALREADY_SCANNING = -9,
        ERR_NO_BONDED_DEVICE = -10,
        // ... add more as needed
    };

    // --- New: Data Types for Parameter Storage ---
    struct VideoParams {
        int16_t whiteBalance = -1; // Initialize to an "unknown" value
        int16_t tint = -1;
        int16_t iso = -1;
        int16_t shutterAngle = -1;
        // ... other video parameters ...
    };

    //TODO lens parameters
    struct LensParams
    {
        //TODO: add lens parameters, example:
        // float focus;
        // float iris;
    };

    //TODO audio parameters
    struct AudioParams
    {
        //TODO: add audio parameters, example:
        // float gainCh1;
        // float gainCh2;
    };


private:
    // --- Nested Security Callbacks Class ---
    class MySecurityCallbacks : public BLESecurityCallbacks {
    public:
        MySecurityCallbacks(BMDBLEController& parent) : _parent(parent) {}

        uint32_t onPassKeyRequest() override;
        void onPassKeyNotify(uint32_t pass_key) override;
        bool onConfirmPIN(uint32_t pin) override;
        bool onSecurityRequest() override;
        void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) override;

    private:
        BMDBLEController& _parent;
    };

    // --- Internal Methods ---
    bool connectToServer();
    void loadBondingInformation();
    void saveBondingInformation();

    void onStatusNotify(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    bool sendCommand(const std::vector<uint8_t>& command);
    // Add this new internal method:
    void updateParameter(uint8_t category, uint8_t parameter, const uint8_t* data, size_t length);

    // --- Member Variables ---
    BLEClient* m_pClient = nullptr;
    BLEScan* m_pBLEScan = nullptr;
    BLEAddress* m_pServerAddress = nullptr;
    BLERemoteCharacteristic* m_pOutgoingCameraControl = nullptr;
    BLERemoteCharacteristic* m_pCameraStatus = nullptr;
    BLERemoteCharacteristic* m_pDeviceName = nullptr;
    bool m_deviceFound = false;
    bool m_connected = false;
    bool m_bonded = false;
    bool m_scanning = false;

    Preferences m_preferences;
    MySecurityCallbacks m_securityCallbacks;

    // Callbacks
    void (*m_statusChangeCallback)(uint8_t status) = nullptr;
    void (*m_deviceFoundCallback)(const BLEAdvertisedDevice& device) = nullptr;
    void (*m_connectionStateChangeCallback)(bool connected) = nullptr;
    uint32_t (*m_pinRequestCallback)() = nullptr;

    // --- Constants ---
    static const BLEUUID SERVICE_UUID;
    static const BLEUUID OUTGOING_CAMERA_CONTROL_UUID;
    static const BLEUUID INCOMING_CAMERA_CONTROL_UUID;
    static const BLEUUID CAMERA_STATUS_UUID;
    static const BLEUUID DEVICE_NAME_UUID;
    static constexpr const char* PREFERENCES_NAMESPACE = "bmd-camera";
    static constexpr const char* AUTHENTICATED_KEY = "authenticated";
    static constexpr const char* ADDRESS_KEY = "address";
    const String m_deviceName;

    // --- New: Data Storage ---
    std::map<uint8_t, VideoParams> m_videoParameters; // Map category to parameters
    std::map<uint8_t, LensParams> m_lensParameters;    //TODO: Map for Lens parameters
    std::map<uint8_t, AudioParams> m_audioParameters;   //TODO: Map for Audio parameters
};

#endif // BMDBLECONTROLLER_H
