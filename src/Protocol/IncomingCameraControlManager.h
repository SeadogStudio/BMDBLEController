#ifndef BMD_INCOMING_CAMERA_CONTROL_MANAGER_H
#define BMD_INCOMING_CAMERA_CONTROL_MANAGER_H

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <chrono>
#include <optional>
#include "../Interfaces/CallbackInterface.h"
#include "../Protocol/ProtocolConstants.h"

namespace BMDCamera {
    class IncomingCameraControlManager {
    public:
        // Structure to hold parameter data
        struct ParameterData {
            std::vector<uint8_t> rawData;
            DataType dataType;
            uint64_t timestamp;

            // Conversion methods
            std::string toString() const;
            float toFloat() const;
            int64_t toInteger() const;
            bool toBoolean() const;
        };

        // Constructor with optional callback manager
        explicit IncomingCameraControlManager(CallbackManager* callbackManager = nullptr);

        // Process an incoming packet
        void processIncomingPacket(const uint8_t* data, size_t length);

        // Check if a specific Category and Parameter combination exists
        bool hasParameter(Category category, uint8_t parameter) const;

        // Retrieve latest data for a specific Category and Parameter
        std::optional<ParameterData> getParameter(Category category, uint8_t parameter) const;

        // Clear all cached parameters
        void clearCache();

        // Get list of all cached categories
        std::vector<Category> getCachedCategories() const;

        // Get list of parameters for a specific category
        std::vector<uint8_t> getParametersForCategory(Category category) const;

    private:
        // Nested map to store latest data for each unique Category and Parameter
        std::unordered_map<uint8_t, 
            std::unordered_map<uint8_t, ParameterData>> m_parameterCache;

        // Pointer to callback manager for notifying parameter updates
        CallbackManager* m_callbackManager;

        // Helper to get current timestamp
        uint64_t getCurrentTimestamp() const;

        // Validate incoming packet
        bool validatePacket(const uint8_t* data, size_t length) const;
    };
}

#endif // BMD_INCOMING_CAMERA_CONTROL_MANAGER_H
