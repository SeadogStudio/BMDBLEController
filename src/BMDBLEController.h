#ifndef BMD_BLE_CONTROLLER_H
#define BMD_BLE_CONTROLLER_H

#include <memory>
#include <string>
#include "Connection/BLEConnectionManager.h"
#include "Protocol/IncomingCameraControlManager.h"
#include "Interfaces/CallbackInterface.h"
#include "Interfaces/PinInputInterface.h"
#include "Controls/LensControl.h"
#include "Controls/VideoControl.h"
#include "Controls/AudioControl.h"
#include "Controls/TransportControl.h"

namespace BMDCamera {
    class BMDBLEController {
    public:
        // Constructor with optional device name
        explicit BMDBLEController(const std::string& deviceName = "BMDBLEController");
        
        // Destructor
        ~BMDBLEController();
        
        // Connection management
        bool startScan(uint32_t duration = 10);
        bool connect();
        bool connectToSavedCamera();
        void disconnect();
        bool isConnected() const;
        bool isBonded() const;
        
        // Callback registration
        void setConnectionCallback(ConnectionCallback callback);
        void setParameterUpdateCallback(ParameterUpdateCallback callback);
        void setStatusUpdateCallback(StatusUpdateCallback callback);
        void setErrorCallback(ErrorCallback callback);
        
        // PIN input method
        void setPinInputMethod(PinInputMethodPtr pinInputMethod);
        
        // Access to specific control modules
        LensControl& getLensControl();
        VideoControl& getVideoControl();
        AudioControl& getAudioControl();
        TransportControl& getTransportControl();
        
        // Access to raw parameter data
        std::optional<IncomingCameraControlManager::ParameterData> 
        getParameter(Category category, uint8_t parameter) const;
        
        // Utility methods
        void clearBondingInformation();
        std::string getStatusString() const;
        
        // Direct command sending (for advanced usage)
        bool sendCommand(
            Category category,
            uint8_t parameter,
            DataType dataType,
            OperationType operation,
            const std::vector<uint8_t>& payload
        );
        
    private:
        // Core components
        std::unique_ptr<CallbackManager> m_callbackManager;
        std::unique_ptr<BLEConnectionManager> m_connectionManager;
        std::unique_ptr<IncomingCameraControlManager> m_incomingControlManager;
        
        // Control modules
        std::unique_ptr<LensControl> m_lensControl;
        std::unique_ptr<VideoControl> m_videoControl;
        std::unique_ptr<AudioControl> m_audioControl;
        std::unique_ptr<TransportControl> m_transportControl;
        
        // Device name
        std::string m_deviceName;
        
        // Notification handler for incoming data
        void handleIncomingData(const uint8_t* data, size_t length);
        
        // Initialize all control modules
        void initializeControlModules();
    };
}

#endif // BMD_BLE_CONTROLLER_H
