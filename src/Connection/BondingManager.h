#include "BondingManager.h"

namespace BMDCamera {

// Static member initialization
const char* BondingManager::PREFERENCES_NAMESPACE = "bmd-camera";
const char* BondingManager::CAMERA_ADDRESS_KEY = "camera_addr";

BondingManager::BondingManager() : m_preferencesOpen(false) {
    // Initialize, but don't open preferences yet
}

BondingManager::~BondingManager() {
    // Ensure preferences are closed
    if (m_preferencesOpen) {
        closePreferences();
    }
}

void BondingManager::openPreferences() {
    if (!m_preferencesOpen) {
        m_preferences.begin(PREFERENCES_NAMESPACE, false);
        m_preferencesOpen = true;
    }
}

void BondingManager::closePreferences() {
    if (m_preferencesOpen) {
        m_preferences.end();
        m_preferencesOpen = false;
    }
}

bool BondingManager::saveBondingInformation(const std::string& address) {
    if (address.empty()) {
        return false;
    }
    
    openPreferences();
    bool success = m_preferences.putString(CAMERA_ADDRESS_KEY, address.c_str());
    closePreferences();
    
    return success;
}

bool BondingManager::hasBondingInformation(const std::string& address) {
    openPreferences();
    
    bool hasBonding = false;
    if (address.empty()) {
        // Check if we have any saved address
        hasBonding = m_preferences.isKey(CAMERA_ADDRESS_KEY);
    } else {
        // Check if the specific address matches our saved one
        std::string savedAddress = m_preferences.getString(CAMERA_ADDRESS_KEY, "").c_str();
        hasBonding = (savedAddress == address);
    }
    
    closePreferences();
    return hasBonding;
}

std::string BondingManager::getSavedCameraAddress() {
    openPreferences();
    std::string address = m_preferences.getString(CAMERA_ADDRESS_KEY, "").c_str();
    closePreferences();
    
    return address;
}

void BondingManager::clearBondingInformation(const std::string& address) {
    openPreferences();
    
    if (address.empty()) {
        // Clear all bonding information
        m_preferences.clear();
        
        // Get all bonded devices and clear them from BLE subsystem
        int dev_num = esp_ble_get_bond_device_num();
        if (dev_num > 0) {
            esp_ble_bond_dev_t* dev_list = new esp_ble_bond_dev_t[dev_num];
            esp_ble_get_bond_device_list(&dev_num, dev_list);
            
            for (int i = 0; i < dev_num; i++) {
                esp_ble_remove_bond_device(dev_list[i].bd_addr);
            }
            
            delete[] dev_list;
        }
    } else {
        // Check if this is the saved address
        std::string savedAddress = m_preferences.getString(CAMERA_ADDRESS_KEY, "").c_str();
        
        if (savedAddress == address) {
            // Remove saved address
            m_preferences.remove(CAMERA_ADDRESS_KEY);
        }
        
        // Remove from BLE subsystem
        BLEAddress bleAddr(address);
        esp_ble_remove_bond_device(*(uint8_t(*)[6])bleAddr.getNative());
    }
    
    closePreferences();
}

std::vector<std::string> BondingManager::getAllBondedDevices() {
    std::vector<std::string> bondedDevices;
    
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num > 0) {
        esp_ble_bond_dev_t* dev_list = new esp_ble_bond_dev_t[dev_num];
        esp_ble_get_bond_device_list(&dev_num, dev_list);
        
        for (int i = 0; i < dev_num; i++) {
            char addressStr[18];
            sprintf(addressStr, "%02X:%02X:%02X:%02X:%02X:%02X",
                dev_list[i].bd_addr[0], dev_list[i].bd_addr[1], dev_list[i].bd_addr[2],
                dev_list[i].bd_addr[3], dev_list[i].bd_addr[4], dev_list[i].bd_addr[5]);
            
            bondedDevices.push_back(std::string(addressStr));
        }
        
        delete[] dev_list;
    }
    
    return bondedDevices;
}

} // namespace BMDCamera
