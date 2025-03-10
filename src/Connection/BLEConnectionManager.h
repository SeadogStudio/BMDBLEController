#ifndef BMD_BLE_CONNECTION_MANAGER_H
#define BMD_BLE_CONNECTION_MANAGER_H

#include <BLEDevice.h>
#include <BLEClient.h>
#include <Preferences.h>
#include "../Interfaces/PinInputInterface.h"
#include "../Interfaces/CallbackInterface.h"
#include "../Protocol/ProtocolConstants.h"

namespace BMDCamera {
    class BLEConnectionManager {
    public:
        // Constructor with optional callback manager
        explicit BLEConnectionManager(
            CallbackManager* callbackManager = nullptr,
            PinInputMethodPtr pinInputMethod = nullptr
        );

        // Destructor to clean up resources
        ~BLEConnectionManager();

        // Scan for Blackmagic cameras
        bool startScan(uint32_t duration = 10);

        // Connect to a discovered camera
        bool connect();

        // Connect to a specific saved/known camera address
        bool connectToSavedCamera();

        // Disconnect from the current camera
        void disconnect();

        // Check current connection status
        bool isConnected() const;

        // Set custom PIN input method
        void setPinInputMethod(PinInputMethodPtr pinInputMethod);

        // Get current camera's BLE address
        std::string getCurrentCameraAddress() const;

    private:
        // BLE Client for connection
        BLEClient* m_pClient = nullptr;

        // Saved camera address
        std::string m_savedCameraAddress;

        // Discovered camera address during scan
        std::string m_discoveredCameraAddress;

        // Preferences for storing bonding information
        Preferences m_preferences;

        // Callback manager for status updates
        CallbackManager* m_callbackManager;

        // PIN input method
        PinInputMethodPtr m_pinInputMethod;

        // Internal scan callback
        class ScanCallback : public BLEAdvertisedDeviceCallbacks {
        public:
            ScanCallback(BLEConnectionManager* manager);
            void onResult(BLEAdvertisedDevice advertisedDevice) override;

        private:
            BLEConnectionManager* m_connectionManager;
        };

        // Security callbacks for PIN entry
        class SecurityCallbacks : public BLESecurityCallbacks {
        public:
            SecurityCallbacks(BLEConnectionManager* manager);
            
            uint32_t onPassKeyRequest() override;
            void onPassKeyNotify(uint32_t pass_key) override;
            bool onConfirmPIN(uint32_t pin) override;
            bool onSecurityRequest() override;
            void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) override;

        private:
            BLEConnectionManager* m_connectionManager;
        };

        // Internal methods
        void saveBondingInformation();
        void clearBondingInformation();
        void setupBLESecurity();

        // Scan callback instance
        std::unique_ptr<ScanCallback> m_scanCallback;

        // Security callback instance
        std::unique_ptr<SecurityCallbacks> m_securityCallbacks;
    };
}

#endif // BMD_BLE_CONNECTION_MANAGER_H
