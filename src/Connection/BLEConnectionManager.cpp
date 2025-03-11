#include "BLEConnectionManager.h"
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

namespace BMDCamera {

// Constructor
BLEConnectionManager::BLEConnectionManager(
    CallbackManager* callbackManager,
    PinInputMethodPtr pinInputMethod
) : 
    m_callbackManager(callbackManager),
    m_pinInputMethod(pinInputMethod)
{
    // Initialize scan and security callback instances
    m_scanCallback = std::unique_ptr<ScanCallback>(new ScanCallback(this));
    m_securityCallbacks = std::unique_ptr<SecurityCallbacks>(new SecurityCallbacks(this));
    
    // Load any saved bonding information
    m_preferences.begin("bmd-camera", false);
    if (m_preferences.isKey("camera_addr")) {
        m_savedCameraAddress = m_preferences.getString("camera_addr", "").c_str();
    }
    m_preferences.end();
}

// Destructor
BLEConnectionManager::~BLEConnectionManager() {
    disconnect();
}

// Start scanning for Blackmagic cameras
bool BLEConnectionManager::startScan(uint32_t duration) {
    // Ensure BLE is initialized
    if (!BLEDevice::getInitialized()) {
        BLEDevice::init("BMDCameraControlESP32");
    }
    
    // Set to maximum power for better range
    BLEDevice::setPower(ESP_PWR_LVL_P9);
    
    // Get the scanner
    BLEScan* pBLEScan = BLEDevice::getScan();
    
    // Set the callback and start scanning
    pBLEScan->setAdvertisedDeviceCallbacks(m_scanCallback.get());
    pBLEScan->setActiveScan(true);
    pBLEScan->start(duration, false);
    
    // Return true to indicate scan started
    return true;
}

// Connect to a discovered camera
bool BLEConnectionManager::connect() {
    // Check if a camera was discovered
    if (m_discoveredCameraAddress.empty()) {
        if (m_callbackManager) {
            m_callbackManager->onError(ERROR_NO_CAMERA_FOUND);
        }
        return false;
    }
    
    // Create a client if needed
    if (!m_pClient) {
        m_pClient = BLEDevice::createClient();
    }
    
    // Set up security
    setupBLESecurity();
    
    // Connect to the device
    BLEAddress bleAddr(m_discoveredCameraAddress);
    if (!m_pClient->connect(bleAddr)) {
        if (m_callbackManager) {
            m_callbackManager->onError(ERROR_CONNECTION_FAILED);
        }
        return false;
    }
    
    // Successfully connected
    if (m_callbackManager) {
        m_callbackManager->onConnectionChanged(true);
    }
    
    return true;
}

// Connect to a specific saved camera address
bool BLEConnectionManager::connectToSavedCamera() {
    // Check if we have a saved address
    if (m_savedCameraAddress.empty()) {
        if (m_callbackManager) {
            m_callbackManager->onError(ERROR_NO_SAVED_CAMERA);
        }
        return false;
    }
    
    // Set discovered address to saved address
    m_discoveredCameraAddress = m_savedCameraAddress;
    
    // Use the regular connect method
    return connect();
}

// Disconnect from the current camera
void BLEConnectionManager::disconnect() {
    if (m_pClient && m_pClient->isConnected()) {
        m_pClient->disconnect();
        
        if (m_callbackManager) {
            m_callbackManager->onConnectionChanged(false);
        }
    }
    
    // Clean up client
    if (m_pClient) {
        delete m_pClient;
        m_pClient = nullptr;
    }
}

// Check current connection status
bool BLEConnectionManager::isConnected() const {
    return (m_pClient != nullptr && m_pClient->isConnected());
}

// Set custom PIN input method
void BLEConnectionManager::setPinInputMethod(PinInputMethodPtr pinInputMethod) {
    m_pinInputMethod = pinInputMethod;
}

// Get current camera's BLE address
std::string BLEConnectionManager::getCurrentCameraAddress() const {
    return m_discoveredCameraAddress;
}

// Internal method to save bonding information
void BLEConnectionManager::saveBondingInformation() {
    if (!m_discoveredCameraAddress.empty()) {
        m_preferences.begin("bmd-camera", false);
        m_preferences.putString("camera_addr", m_discoveredCameraAddress.c_str());
        m_preferences.end();
        
        // Update saved address
        m_savedCameraAddress = m_discoveredCameraAddress;
    }
}

// Internal method to clear bonding information
void BLEConnectionManager::clearBondingInformation() {
    m_preferences.begin("bmd-camera", false);
    m_preferences.clear();
    m_preferences.end();
    
    m_savedCameraAddress.clear();
    
    // If we have a BLE address, clean up its bonding
    if (!m_discoveredCameraAddress.empty()) {
        BLEAddress bleAddr(m_discoveredCameraAddress);
        esp_ble_remove_bond_device(*(uint8_t(*)[6])bleAddr.getNative());
    }
}

// Setup BLE security
void BLEConnectionManager::setupBLESecurity() {
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
    BLEDevice::setSecurityCallbacks(m_securityCallbacks.get());
    
    BLESecurity* pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
    pSecurity->setCapability(ESP_IO_CAP_IN);
    pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
}

// ScanCallback implementation
BLEConnectionManager::ScanCallback::ScanCallback(BLEConnectionManager* manager)
    : m_connectionManager(manager) {}

void BLEConnectionManager::ScanCallback::onResult(BLEAdvertisedDevice advertisedDevice) {
    // Check if the device advertises the BMD service UUID
    if (advertisedDevice.haveServiceUUID() && 
        advertisedDevice.isAdvertisingService(BLEUUID(BMD_SERVICE_UUID))) {
        
        // Store the address
        m_connectionManager->m_discoveredCameraAddress = advertisedDevice.getAddress().toString();
        
        // Stop scanning - we found what we're looking for
        advertisedDevice.getScan()->stop();
        
        // Notify through callback if available
        if (m_connectionManager->m_callbackManager) {
            m_connectionManager->m_callbackManager->onDeviceFound(advertisedDevice);
        }
    }
}

// SecurityCallbacks implementation
BLEConnectionManager::SecurityCallbacks::SecurityCallbacks(BLEConnectionManager* manager)
    : m_connectionManager(manager) {}

uint32_t BLEConnectionManager::SecurityCallbacks::onPassKeyRequest() {
    // Use the PIN input method if available
    if (m_connectionManager->m_pinInputMethod) {
        return m_connectionManager->m_pinInputMethod->requestPin();
    }
    
    // Default to 0 if no input method
    return 0;
}

void BLEConnectionManager::SecurityCallbacks::onPassKeyNotify(uint32_t pass_key) {
    // Not used in this implementation
}

bool BLEConnectionManager::SecurityCallbacks::onConfirmPIN(uint32_t pin) {
    return true;
}

bool BLEConnectionManager::SecurityCallbacks::onSecurityRequest() {
    return true;
}

void BLEConnectionManager::SecurityCallbacks::onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) {
    if (auth_cmpl.success) {
        // Save bonding information on successful authentication
        m_connectionManager->saveBondingInformation();
        
        // Notify through callback if available
        if (m_connectionManager->m_callbackManager) {
            m_connectionManager->m_callbackManager->onAuthenticationComplete(true);
        }
    } else {
        if (m_connectionManager->m_callbackManager) {
            m_connectionManager->m_callbackManager->onAuthenticationComplete(false);
            m_connectionManager->m_callbackManager->onError(ERROR_AUTHENTICATION_FAILED);
        }
    }
}

} // namespace BMDCamera
