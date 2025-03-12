// BMDBLEController.cpp
#include "BMDBLEController.h"


BLEScan* BMDBLEController::pBLEScan = nullptr;   // Initialize static member
BLEClient* BMDBLEController::pClient = nullptr; // Initialize pClient
bool BMDBLEController::is_connected = false;


BMDBLEController::BMDBLEController() :
    pServerAddress(nullptr),
    pOutgoingCameraControl(nullptr),
    pIncomingCameraControl(nullptr),
    pTimecode(nullptr),
	pCameraStatus(nullptr),  // Initialize to nullptr
    rawIncomingData(""),
    rawTimecodeData("")
{
    BLEDevice::init("ESP32_BMD_Controller");  // Device name can be changed
    BLEDevice::setPower(ESP_PWR_LVL_P9); // Set max power

	pClient = BLEDevice::createClient();

	//Setup Security
	BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
	BLEDevice::setSecurityCallbacks(new MySecurityCallbacks(this));
	BLESecurity* pSecurity = new BLESecurity();
	pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
	pSecurity->setCapability(ESP_IO_CAP_IN);
	pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

	pBLEScan = BLEDevice::getScan(); //create new scan
	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(this));
	pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
	pBLEScan->setInterval(100);
	pBLEScan->setWindow(99);  //should be less or equal RSSI interval

}

BMDBLEController::~BMDBLEController() {
    if (isConnected()) {
        disconnect();
    }
    delete pServerAddress;
    // Clean up other resources if necessary
}
void BMDBLEController::MyAdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.printf("Advertised Device Found: %s\n", advertisedDevice.toString().c_str());

    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
        BLEDevice::getScan()->stop();
        bmdController->pServerAddress = new BLEAddress(advertisedDevice.getAddress());
        bmdController->doConnect = true;
		bmdController->doScan = false;
        Serial.println("Formed a connection to the Blackmagic Camera");
    }
}

uint32_t BMDBLEController::MySecurityCallbacks::onPassKeyRequest() {
	Serial.print("PassKeyRequest: ");
	Serial.println(bmdController->pinCode);
	return bmdController->pinCode;
}

void BMDBLEController::MySecurityCallbacks::onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) {
	if (auth_cmpl.success) {
		Serial.println("Authentication success!");
		// Save bonding information
		bmdController->preferences.begin("camera", false);
		bmdController->preferences.putBool("authenticated", true);
		bmdController->preferences.putString("address", bmdController->pServerAddress->toString().c_str());
		bmdController->preferences.end();
		is_connected = true;

	}
	else {
		Serial.printf("Authentication failed: %d\n", auth_cmpl.fail_reason);
		is_connected = false;
	}
}


bool BMDBLEController::connect() {
    // Check for existing bonding information
    preferences.begin("camera", false);
    bool authenticated = preferences.getBool("authenticated", false);
    String savedAddress = preferences.getString("address", "");
    preferences.end();

    if (authenticated && savedAddress.length() > 0) {
        // Reconnect using saved information
		Serial.println("Attempt Reconnection Using Saved Info");
        pServerAddress = new BLEAddress(savedAddress.c_str());
        doConnect = true; // Initiate connection attempt
    }
	else {
		//If not authenticated Start scan
		Serial.println("Start scanning for BMD Camera...");
		doScan = true;
	}

	if (doConnect) {
		if (connectToServer()) {
			Serial.println("Connected to the BLE Server.");
		}
		else {
			Serial.println("Failed to connect to the server, restarting scan");
			doScan = true; // Restart scanning if connection fails.
		}
		doConnect = false; // Reset the flag.
	}

	if (doScan) {
		// Start scanning for devices
		Serial.println("Scanning Started");
		BLEScanResults foundDevices = pBLEScan->start(5, false); // Scan for 5 seconds
		Serial.print("Devices found: ");
		Serial.println(foundDevices.getCount());
		pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
	}
    return isConnected();
}



bool BMDBLEController::connectToServer() {

	if (!pClient->connect(*pServerAddress)) {
      // Connection failed
		Serial.println("Connection Failed");
		pClient->disconnect();
      return false;
    }
    if (!discoverServices())
    {
        return false;
    }

    return true;
}

bool BMDBLEController::discoverServices() {
     BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
        Serial.print("Failed to find our service UUID: ");
        Serial.println(SERVICE_UUID);
        pClient->disconnect();
        return false;
    }
    Serial.println("Found our service");

    // Obtain references to the characteristics
    pOutgoingCameraControl = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_OUTGOING_CAMERA_CONTROL);
    pIncomingCameraControl = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_INCOMING_CAMERA_CONTROL);
    pTimecode = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_TIMECODE);
	pCameraStatus = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_CAMERA_STATUS);
	pDeviceName = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_DEVICE_NAME);  // Get device name characteristic
    if (pOutgoingCameraControl == nullptr || pIncomingCameraControl == nullptr || pTimecode == nullptr || pCameraStatus == nullptr) {
        Serial.println("Failed to find one or more characteristics");
        pClient->disconnect();
        return false;
    }
    Serial.println("Found our characteristics");
	// Trigger bonding by writing to device name (if not already bonded)
	if (!isConnected()) {
		Serial.println("Writing to device name to initiate bonding...");
		std::string deviceName = "ESP32";
		pDeviceName->writeValue(deviceName.c_str(), deviceName.length());
	}
    // Register for notifications
    pIncomingCameraControl->registerForNotify(controlNotifyCallback);
    pTimecode->registerForNotify(timecodeNotifyCallback);
	pCameraStatus->registerForNotify(cameraStatusNotifyCallback);

    return true;

}

bool BMDBLEController::disconnect() {
    if (isConnected()) {
        pClient->disconnect();
        is_connected = false; // Update connection status
		return true;
    }
	return false;
}

bool BMDBLEController::isConnected() {
	//return pClient->isConnected(); //Using is_connected is better
	return is_connected;
}

bool BMDBLEController::sendData(const uint8_t* data, size_t length) {
    if (isConnected() && pOutgoingCameraControl != nullptr) {
        pOutgoingCameraControl->writeValue((uint8_t*)data, length); // Cast away const
        return true;
    }
    return false;
}

// --- Notification Callbacks ---

void BMDBLEController::controlNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    // Store raw data
   ((BMDBLEController*)pBLERemoteCharacteristic->getRemoteService()->getClient()->getData())->rawIncomingData = std::string((char*)pData, length);

    // Add your parsing logic here.  Example:
    // if (length >= 2 && pData[0] == 0x01) { // Example:  Check for a specific command
    //     // Parse the data...
    // }
	Serial.print("Incoming Camera Control Notify callback, Data: ");
	Serial.println(((BMDBLEController*)pBLERemoteCharacteristic->getRemoteService()->getClient()->getData())->rawIncomingData.c_str());
}

void BMDBLEController::timecodeNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
	// Store raw data
	((BMDBLEController*)pBLERemoteCharacteristic->getRemoteService()->getClient()->getData())->rawTimecodeData = std::string((char*)pData, length);
	Serial.print("Timecode Notify callback, Data: ");
	Serial.println(((BMDBLEController*)pBLERemoteCharacteristic->getRemoteService()->getClient()->getData())->rawTimecodeData.c_str());

    // Parse Timecode Here (Example, adapt to actual format)
	
}
void BMDBLEController::cameraStatusNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
	// Store raw data
	((BMDBLEController*)pBLERemoteCharacteristic->getRemoteService()->getClient()->getData())->rawCameraStatusData = std::string((char*)pData, length);
	Serial.print("Camera Status Notify callback, Data: ");
	Serial.println(((BMDBLEController*)pBLERemoteCharacteristic->getRemoteService()->getClient()->getData())->rawCameraStatusData.c_str());

}

String BMDBLEController::getTimecode()
{
	return String(rawTimecodeData.c_str()); //Simple return you MUST PARSE IT 
}

String BMDBLEController::getCameraStatus()
{
	return String(rawCameraStatusData.c_str()); //Simple return you MUST PARSE IT
}
