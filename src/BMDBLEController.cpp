#include "BMDBLEController.h"

// --- Constants Definitions ---
const BLEUUID BMDBLEController::SERVICE_UUID("291D567A-6D75-11E6-8B77-86F30CA893D3");
const BLEUUID BMDBLEController::OUTGOING_CAMERA_CONTROL_UUID("5DD3465F-1AEE-4299-8493-D2ECA2F8E1BB");
const BLEUUID BMDBLEController::INCOMING_CAMERA_CONTROL_UUID("B864E140-76A0-416A-BF30-5876504537D9");
const BLEUUID BMDBLEController::CAMERA_STATUS_UUID("7FE8691D-95DC-4FC5-8ABD-CA74339B51B9");
const BLEUUID BMDBLEController::DEVICE_NAME_UUID("FFAC0C52-C9FB-41A0-B063-CC76282EB89C");

// --- Constructor ---
BMDBLEController::BMDBLEController(const String& deviceName) :
    m_deviceName(deviceName),
    m_securityCallbacks(*this) // Initialize security callbacks
{
    BLEDevice::init(m_deviceName.c_str());
    BLEDevice::setPower(ESP_PWR_LVL_P9); // Set to maximum power
}

// --- Destructor ---
BMDBLEController::~BMDBLEController() {
    disconnect();
    if (m_pServerAddress) {
        delete m_pServerAddress;
        m_pServerAddress = nullptr;
    }
    BLEDevice::deinit(true); // Release BLE resources
}

// --- Connection Management ---
bool BMDBLEController::connect() {
    if (m_connected) {
        return true; // Already connected.
    }
    loadBondingInformation();
    if (m_bonded && m_pServerAddress) {
        return connectToServer();
    }
    else {
        startScan();
        return true; // Scan started.  Connection attempt will happen in callback.
    }
}

bool BMDBLEController::connect(const BLEAddress& address) {
    if (m_connected) {
        return ErrorCode::ERR_ALREADY_CONNECTED;
    }
    //clear any previous data
    if (m_pServerAddress) {
        delete m_pServerAddress;
        m_pServerAddress = nullptr;
    }
    m_pServerAddress = new BLEAddress(address);
    m_deviceFound = true;
    return connectToServer();
}

void BMDBLEController::disconnect() {
    if (m_pClient && m_connected) {
        m_pClient->disconnect();
    }
    m_connected = false;
    if (m_connectionStateChangeCallback) {
        m_connectionStateChangeCallback(false);
    }
    // Don't clear bonding info here – user might want to reconnect.
}

bool BMDBLEController::isConnected() const {
    return m_connected;
}

bool BMDBLEController::isBonded() const {
    return m_bonded;
}

// --- Scanning ---
void BMDBLEController::startScan(uint32_t duration) {
    if (m_scanning) {
        return;
    }
    if (m_pBLEScan == nullptr) {
        m_pBLEScan = BLEDevice::getScan();
        m_pBLEScan->setAdvertisedDeviceCallbacks(new BLEAdvertisedDeviceCallbacks([this](const BLEAdvertisedDevice& advertisedDevice) {
            // Use Serial.print directly with c_str()
            Serial.print("BLE Device found: ");
            Serial.print(advertisedDevice.getName().c_str()); // Directly use c_str()
            Serial.print(" Address: ");
            Serial.println(advertisedDevice.getAddress().toString().c_str());


            if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(SERVICE_UUID)) {
                Serial.println("Found a Blackmagic camera!");
                stopScan(); // Stop scan immediately
                if (m_pServerAddress) {
                    delete m_pServerAddress;
                }
                m_pServerAddress = new BLEAddress(advertisedDevice.getAddress());
                m_deviceFound = true;

                if (m_deviceFoundCallback) {
                    m_deviceFoundCallback(advertisedDevice); // Notify the user
                }
                connectToServer(); // Attempt connection
            }
        }));
    }

    m_pBLEScan->setActiveScan(true);
    m_pBLEScan->setInterval(100);
    m_pBLEScan->setWindow(99);
    m_pBLEScan->start(duration, [this](BLEScanResults results) { m_scanning = false; });
    m_scanning = true;
}

void BMDBLEController::stopScan() {
    if (m_pBLEScan != nullptr) {
        m_pBLEScan->stop();
        m_scanning = false;
    }
}

// --- Internal Connection Logic ---
bool BMDBLEController::connectToServer() {
    if (!m_pServerAddress) {
        Serial.println("No camera address to connect to.");
        return false;
    }

    Serial.print("Connecting to camera at address: ");
    Serial.println(m_pServerAddress->toString().c_str());

    if (m_pClient == nullptr) {
        m_pClient = BLEDevice::createClient();

        // --- Set up security ---
        BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
        BLEDevice::setSecurityCallbacks(&m_securityCallbacks);

        BLESecurity* pSecurity = new BLESecurity(); // Consider member variable if needed longer.
        pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
        pSecurity->setCapability(ESP_IO_CAP_IN);
        pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
        delete pSecurity;  // Clean up
        pSecurity = nullptr;
    }


    // Connect to the camera
    if (!m_pClient->connect(*m_pServerAddress)) {
        Serial.println("Connection failed.");
        if (m_connectionStateChangeCallback) {
            m_connectionStateChangeCallback(false);
        }
        return false;
    }

    Serial.println("Connected to camera.");

    // --- Get the service and characteristics ---
    BLERemoteService* pRemoteService = m_pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
        Serial.println("Failed to find Blackmagic camera service.");
        m_pClient->disconnect();
        if (m_connectionStateChangeCallback) {
            m_connectionStateChangeCallback(false);
        }
        return false;
    }

    m_pDeviceName = pRemoteService->getCharacteristic(DEVICE_NAME_UUID);
    if (m_pDeviceName && m_pDeviceName->canWrite())
    {
        m_pDeviceName->writeValue(m_deviceName.c_str(), m_deviceName.length());
    }

    m_pOutgoingCameraControl = pRemoteService->getCharacteristic(OUTGOING_CAMERA_CONTROL_UUID);
    if (m_pOutgoingCameraControl == nullptr) {
        Serial.println("Failed to find outgoing control characteristic.");
        m_pClient->disconnect();
        if (m_connectionStateChangeCallback) {
            m_connectionStateChangeCallback(false);
        }
        return false;
    }

    m_pCameraStatus = pRemoteService->getCharacteristic(CAMERA_STATUS_UUID);
    if (m_pCameraStatus) {
        if (m_pCameraStatus->canNotify())
        {
            m_pCameraStatus->registerForNotify([this](BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
                this->onStatusNotify(pBLERemoteCharacteristic, pData, length, isNotify);
            });
        }
        else {
            Serial.println("Camera Status Characteristic cannot notify.");
        }
    }

    m_connected = true;
    if (m_connectionStateChangeCallback) {
        m_connectionStateChangeCallback(true);
    }
    return true;
}

// --- Preferences (Bonding) ---

void BMDBLEController::loadBondingInformation() {
    m_preferences.begin(PREFERENCES_NAMESPACE, true);
    m_bonded = m_preferences.getBool(AUTHENTICATED_KEY, false);
    String savedAddress = m_preferences.getString(ADDRESS_KEY, "");
    m_preferences.end();

    if (m_bonded && !savedAddress.isEmpty()) {
        if (m_pServerAddress) {
            delete m_pServerAddress;
            m_pServerAddress = nullptr;
        }
        m_pServerAddress = new BLEAddress(savedAddress.c_str());
        Serial.print("Loaded bonded address: "); Serial.println(savedAddress);
    }
    else {
        Serial.println("No bonding information found.");
        m_bonded = false; // Ensure m_bonded is false if no address is loaded.
    }
}

void BMDBLEController::saveBondingInformation() {
    m_preferences.begin(PREFERENCES_NAMESPACE, false);
    m_preferences.putBool(AUTHENTICATED_KEY, m_bonded);
    if (m_pServerAddress) {
        m_preferences.putString(ADDRESS_KEY, m_pServerAddress->toString().c_str());
    }
    m_preferences.end();
    Serial.println("Bonding information saved.");
}

void BMDBLEController::clearBondingInformation() {
    m_preferences.begin(PREFERENCES_NAMESPACE, false);
    m_preferences.clear();
    m_preferences.end();
    m_bonded = false;
    if (m_pServerAddress) {
        delete m_pServerAddress;
        m_pServerAddress = nullptr;
    }
    Serial.println("Bonding information cleared.");
}

// --- Security Callbacks ---

uint32_t BMDBLEController::MySecurityCallbacks::onPassKeyRequest() {
    Serial.println("Passkey Request");
    if (_parent.m_pinRequestCallback) {
        return _parent.m_pinRequestCallback();
    }
    //consider using a non-blocking method (e.g., store the pin code using webserver)
    int pinCode = 0;
    char ch;
    do {
        while (!Serial.available()) {
            vTaskDelay(1);
        }
        ch = Serial.read();
        if (ch >= '0' && ch <= '9') {
            pinCode = pinCode * 10 + (ch - '0');
            Serial.print(ch);
        }
    } while (ch != '\n');

    Serial.println();
    Serial.print("PIN code: ");
    Serial.println(pinCode);
    return pinCode;
}

void BMDBLEController::MySecurityCallbacks::onPassKeyNotify(uint32_t pass_key) {
    Serial.print("Passkey Notify: "); Serial.println(pass_key); // For debugging
}

bool BMDBLEController::MySecurityCallbacks::onConfirmPIN(uint32_t pin) {
    Serial.print("Confirm PIN: "); Serial.println(pin);
    return true; // Always accept in this case.
}

bool BMDBLEController::MySecurityCallbacks::onSecurityRequest() {
    return true; // Accept security requests.
}

void BMDBLEController::MySecurityCallbacks::onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) {
    if (auth_cmpl.success) {
        Serial.println("Authentication successful.");
        _parent.m_bonded = true;
        _parent.saveBondingInformation();
    }
    else {
        Serial.print("Authentication failed! Reason: "); Serial.println(auth_cmpl.fail_reason);
        _parent.m_bonded = false;
        _parent.clearBondingInformation();
    }
}

// --- Command Sending ---

bool BMDBLEController::sendCommand(const std::vector<uint8_t>& command) {
    if (!m_connected || !m_pOutgoingCameraControl) {
        return false;
    }
    return m_pOutgoingCameraControl->writeValue(const_cast<uint8_t*>(command.data()), command.size(), true);
}

// --- Example Camera Control Commands ---

bool BMDBLEController::setWhiteBalance(uint16_t kelvin, int16_t tint) {
    if (kelvin < 2000 || kelvin > 10000)
    {
        Serial.println("White balance value out of range (2000-10000 K).");
        return false;
    }
    std::vector<uint8_t> command = {
      255,  // Destination
      8,    // Length
      0,    // Command ID
      0,    // Reserved
      1,    // Category (Video)
      2,    // Parameter (Manual White Balance)
      2,    // Data Type (signed 16-bit integer)
      0,    // Operation (assign)
      (uint8_t)(kelvin & 0xFF),         // Color temp low byte
      (uint8_t)((kelvin >> 8) & 0xFF),  // Color temp high byte
      (uint8_t)(tint & 0xFF),           // Tint low byte
      (uint8_t)((tint >> 8) & 0xFF)     // Tint high byte
    };
    return sendCommand(command);
}

bool BMDBLEController::setISO(uint16_t iso) {
    //TODO: Implement based on Blackmagic Camera Control Protocol
    std::vector<uint8_t> command = { /* ... command bytes ... */ };
    return sendCommand(command);
}

bool BMDBLEController::setShutterAngle(uint16_t angle) {
    //TODO: Implement based on Blackmagic Camera Control Protocol
    std::vector<uint8_t> command = { /* ... command bytes ... */ };
    return sendCommand(command);
}
bool BMDBLEController::setNDFilter(uint8_t nd) {
    //TODO: Implement based on Blackmagic Camera Control Protocol
    std::vector<uint8_t> command = { /* ... command bytes ... */ };
    return sendCommand(command);
}

bool BMDBLEController::startRecording()
{
    //TODO: Implement based on Blackmagic Camera Control Protocol
    std::vector<uint8_t> command = { /* ... command bytes ... */ };
    return sendCommand(command);
}

bool BMDBLEController::stopRecording()
{
    //TODO: Implement based on Blackmagic Camera Control Protocol
    std::vector<uint8_t> command = { /* ... command bytes ... */ };
    return sendCommand(command);
}

bool BMDBLEController::getCameraStatus()
{
    //TODO: Implement based on Blackmagic Camera Control Protocol.  This may not be needed if status notifications are working.
    std::vector<uint8_t> command = { /* ... command bytes ... */ };
    return sendCommand(command);
}

// --- Getters for Characteristics ---

BLERemoteCharacteristic* BMDBLEController::getOutgoingCameraControlCharacteristic() const {
    return m_pOutgoingCameraControl;
}

BLERemoteCharacteristic* BMDBLEController::getCameraStatusCharacteristic() const {
    return m_pCameraStatus;
}

// --- Callback Management ---
void BMDBLEController::setStatusChangeCallback(void (*callback)(uint8_t status)) {
    m_statusChangeCallback = callback;
}

void BMDBLEController::setDeviceFoundCallback(void (*callback)(const BLEAdvertisedDevice& device)) {
    m_deviceFoundCallback = callback;
}

void BMDBLEController::setConnectionStateChangeCallback(void (*callback)(bool connected)) {
    m_connectionStateChangeCallback = callback;
}
void BMDBLEController::setPinRequestCallback(uint32_t (*callback)())
{
    m_pinRequestCallback = callback;
}
// --- Status Notification Handler ---
void BMDBLEController::onStatusNotify(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (length > 0 && m_statusChangeCallback) {
        m_statusChangeCallback(pData[0]); // Pass the status byte to the callback.
    }
}

// --- Utility Functions ---

String BMDBLEController::errorCodeToString(int16_t errorCode) {
    switch (errorCode) {
    case ERR_NONE: return "No error";
    case ERR_NOT_CONNECTED: return "Not connected to a camera";
    case ERR_CHARACTERISTIC_NOT_FOUND: return "Required characteristic not found";
    case ERR_COMMAND_FAILED: return "Command failed to send";
    case ERR_INVALID_PARAMETER: return "Invalid parameter value";
    case ERR_SCAN_FAILED: return "BLE scan failed to start";
    case ERR_CONNECTION_FAILED: return "Failed to connect to camera";
    case ERR_SERVICE_NOT_FOUND: return "Blackmagic camera service not found";
    case ERR_ALREADY_CONNECTED: return "Already connected to a camera";
    case ERR_ALREADY_SCANNING: return "Already scanning for devices";
    case ERR_NO_BONDED_DEVICE: return "No bonded device found";
    default: return "Unknown error";
    }
}
