#ifndef BMD_CALLBACK_INTERFACE_H
#define BMD_CAMERA_CALLBACK_INTERFACE_H

#include <functional>
#include <string>
#include "../Protocol/ProtocolConstants.h"

namespace BMDCamera {
    // Callback for connection state changes
    using ConnectionCallback = std::function<void(bool isConnected)>;

    // Callback for parameter updates
    using ParameterUpdateCallback = std::function<void(
        Category category, 
        uint8_t parameterID, 
        const std::vector<uint8_t>& data
    )>;

    // Callback for general status updates
    using StatusUpdateCallback = std::function<void(const std::string& message)>;

    // Callback for error handling
    using ErrorCallback = std::function<void(const std::string& errorMessage)>;

    class CallbackManager {
    public:
        // Register callbacks
        void setConnectionCallback(ConnectionCallback cb) {
            m_connectionCallback = std::move(cb);
        }

        void setParameterUpdateCallback(ParameterUpdateCallback cb) {
            m_parameterUpdateCallback = std::move(cb);
        }

        void setStatusUpdateCallback(StatusUpdateCallback cb) {
            m_statusUpdateCallback = std::move(cb);
        }

        void setErrorCallback(ErrorCallback cb) {
            m_errorCallback = std::move(cb);
        }

        // Trigger callbacks if set
        void notifyConnectionState(bool isConnected) {
            if (m_connectionCallback) {
                m_connectionCallback(isConnected);
            }
        }

        void notifyParameterUpdate(
            Category category, 
            uint8_t parameterID, 
            const std::vector<uint8_t>& data
        ) {
            if (m_parameterUpdateCallback) {
                m_parameterUpdateCallback(category, parameterID, data);
            }
        }

        void notifyStatusUpdate(const std::string& message) {
            if (m_statusUpdateCallback) {
                m_statusUpdateCallback(message);
            }
        }

        void notifyError(const std::string& errorMessage) {
            if (m_errorCallback) {
                m_errorCallback(errorMessage);
            }
        }

    private:
        ConnectionCallback m_connectionCallback;
        ParameterUpdateCallback m_parameterUpdateCallback;
        StatusUpdateCallback m_statusUpdateCallback;
        ErrorCallback m_errorCallback;
    };
}

#endif // BMD_CALLBACK_INTERFACE_H
