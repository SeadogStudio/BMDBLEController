// BMDBLEController.h
#ifndef BMDBLECONTROLLER_H
#define BMDBLECONTROLLER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Preferences.h>  // For storing bonding info

#define SERVICE_UUID "291d567a-6d75-11e6-8b77-86f30ca893d3"
#define CHARACTERISTIC_UUID_OUTGOING_CAMERA_CONTROL "f1e4fc02-6d76-11e6-8b77-86f30ca893d3"
#define CHARACTERISTIC_UUID_INCOMING_CAMERA_CONTROL "f1e4fc03-6d76-11e6-8b77-86f30ca893d3"
#define CHARACTERISTIC_UUID_TIMECODE "24ae8716-6d75-11e6-8b77-86f30ca893d3"
#define CHARACTERISTIC_UUID_CAMERA_STATUS "f88737b8-6d75-11e6-8b77-86f30ca893d3" //Added as per guidelines.
#define CHARACTERISTIC_UUID_DEVICE_NAME  "2a00"


class BMDBLEController {
public:
    BMDBLEController();
    ~BMDBLEController();

    bool connect();
    bool disconnect();
    bool isConnected();

    // Send data to the camera
    bool sendData(const uint8_t* data, size_t length);


    // Getters for raw data (for advanced users)
    const std::string& getRawIncomingData() const { return rawIncomingData; }
    const std::string& getRawTimecodeData() const { return rawTimecodeData; }
	const std::string& getRawCameraStatusData() const { return rawCameraStatusData; }


	// Data access methods (you'll add parsing functions here)
	String getTimecode();
	String getCameraStatus();  // Add more as you parse more data

    // Set the PIN code (to be called from the main sketch)
    void setPinCode(uint32_t pin) { pinCode = pin; }


private:
    static void controlNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    static void timecodeNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
	static void cameraStatusNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);


	bool connectToServer(); //Handles conneciton to server
    bool discoverServices(); // Discover services and characteristics

	BLEAddress* pServerAddress;
    bool deviceFound = false;
    bool doConnect = false; //Flag to trigger connection
	bool doScan = false;  // Flag to trigger scanning
    BLERemoteCharacteristic* pOutgoingCameraControl;
    BLERemoteCharacteristic* pIncomingCameraControl;
    BLERemoteCharacteristic* pTimecode;
	BLERemoteCharacteristic* pCameraStatus; // Added as per guideline
	BLERemoteCharacteristic* pDeviceName;


	std::string rawIncomingData;
    std::string rawTimecodeData;
	std::string rawCameraStatusData;

    uint32_t pinCode = 0; // Store the PIN code
	static BLEScan* pBLEScan; // Declare pBLEScan as a static member
	static BLEClient* pClient; // Declare pClient
	static bool is_connected;
	Preferences preferences;

  // Inner class for advertisement callbacks
    class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
        BMDBLEController* bmdController;  // Pointer back to the main class

    public:
        MyAdvertisedDeviceCallbacks(BMDBLEController* controller) : bmdController(controller) {}

        void onResult(BLEAdvertisedDevice advertisedDevice) override;
    };

    // Inner class for security callbacks
	class MySecurityCallbacks : public BLESecurityCallbacks {
		BMDBLEController* bmdController;
	public:
		MySecurityCallbacks(BMDBLEController* controller) : bmdController(controller) {}
		uint32_t onPassKeyRequest() override;
		void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) override;
		bool onConfirmPIN(uint32_t pin) override { return true; }; // Always accept (for simplicity)
	};
};

#endif // BMDBLECONTROLLER_H
