// src/Protocol/IncomingCameraControlManager.cpp
#include "IncomingCameraControlManager.h"
#include "ProtocolUtils.h"
#include <algorithm>

namespace BMDCamera {

IncomingCameraControlManager::IncomingCameraControlManager(CallbackManager* callbackManager)
    : m_callbackManager(callbackManager) {
}

void IncomingCameraControlManager::processIncomingPacket(const uint8_t* data, size_t length) {
    // Validate the packet first
    if (!validatePacket(data, length)) {
        return;
    }
    
    // Extract packet information
    Category category = static_cast<Category>(data[4]);
    uint8_t parameter = data[5];
    DataType dataType = static_cast<DataType>(data[6]);
    OperationType operation = static_cast<OperationType>(data[7]);
    
    // Only process response/report packets (operation 0x02)
    if (operation != OperationType::Report) {
        return;
    }
    
    // Extract the data payload (starting from byte 8)
    std::vector<uint8_t> payload;
    if (length > 8) {
        payload.assign(data + 8, data + length);
    }
    
    // Create parameter data
    ParameterData paramData;
    paramData.rawData = std::move(payload);
    paramData.dataType = dataType;
    paramData.timestamp = getCurrentTimestamp();
    
    // Store in cache
    m_parameterCache[static_cast<uint8_t>(category)][parameter] = paramData;
    
    // Notify callback if available
    if (m_callbackManager) {
        m_callbackManager->onParameterUpdated(category, parameter, paramData);
    }
}

bool IncomingCameraControlManager::hasParameter(Category category, uint8_t parameter) const {
    auto catIt = m_parameterCache.find(static_cast<uint8_t>(category));
    if (catIt == m_parameterCache.end()) {
        return false;
    }
    
    return catIt->second.find(parameter) != catIt->second.end();
}

std::optional<IncomingCameraControlManager::ParameterData> 
IncomingCameraControlManager::getParameter(Category category, uint8_t parameter) const {
    auto catIt = m_parameterCache.find(static_cast<uint8_t>(category));
    if (catIt == m_parameterCache.end()) {
        return std::nullopt;
    }
    
    auto paramIt = catIt->second.find(parameter);
    if (paramIt == catIt->second.end()) {
        return std::nullopt;
    }
    
    return paramIt->second;
}

void IncomingCameraControlManager::clearCache() {
    m_parameterCache.clear();
}

std::vector<Category> IncomingCameraControlManager::getCachedCategories() const {
    std::vector<Category> categories;
    categories.reserve(m_parameterCache.size());
    
    for (const auto& catEntry : m_parameterCache) {
        categories.push_back(static_cast<Category>(catEntry.first));
    }
    
    return categories;
}

std::vector<uint8_t> IncomingCameraControlManager::getParametersForCategory(Category category) const {
    std::vector<uint8_t> parameters;
    
    auto catIt = m_parameterCache.find(static_cast<uint8_t>(category));
    if (catIt == m_parameterCache.end()) {
        return parameters;
    }
    
    parameters.reserve(catIt->second.size());
    for (const auto& paramEntry : catIt->second) {
        parameters.push_back(paramEntry.first);
    }
    
    return parameters;
}

uint64_t IncomingCameraControlManager::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

bool IncomingCameraControlManager::validatePacket(const uint8_t* data, size_t length) const {
    // Basic packet validation
    if (length < 8) {
        // Minimum packet size is 8 bytes (header + category + parameter + type + operation)
        return false;
    }
    
    // Check protocol identifier
    if (data[0] != 0xFF) {
        return false;
    }
    
    // Check packet length byte matches actual length
    uint8_t declaredLength = data[1];
    if (declaredLength != length - 2) {  // -2 because length byte doesn't include protocol ID and itself
        return false;
    }
    
    // Check command ID and reserved byte are as expected
    if (data[2] != 0x00 || data[3] != 0x00) {
        return false;
    }
    
    return true;
}

// Implementation of ParameterData conversion methods

std::string IncomingCameraControlManager::ParameterData::toString() const {
    switch (dataType) {
        case DataType::String:
            return std::string(rawData.begin(), rawData.end());
            
        case DataType::SignedByte:
        case DataType::SignedInt16:
        case DataType::SignedInt32:
        case DataType::SignedInt64:
            return std::to_string(toInteger());
            
        case DataType::Fixed16:
            return std::to_string(toFloat());
            
        case DataType::Boolean:
        case DataType::Void:
            return toBoolean() ? "true" : "false";
            
        default:
            return "";
    }
}

float IncomingCameraControlManager::ParameterData::toFloat() const {
    switch (dataType) {
        case DataType::Fixed16:
            if (rawData.size() >= 2) {
                uint16_t fixed16 = (rawData[0] | (rawData[1] << 8));
                return ProtocolUtils::fixed16ToFloat(fixed16);
            }
            return 0.0f;
            
        case DataType::SignedByte:
        case DataType::SignedInt16:
        case DataType::SignedInt32:
        case DataType::SignedInt64:
            return static_cast<float>(toInteger());
            
        case DataType::Boolean:
        case DataType::Void:
            return toBoolean() ? 1.0f : 0.0f;
            
        case DataType::String:
            // Try to convert string to float
            try {
                return std::stof(toString());
            } catch (...) {
                return 0.0f;
            }
            
        default:
            return 0.0f;
    }
}

int64_t IncomingCameraControlManager::ParameterData::toInteger() const {
    if (rawData.empty()) {
        return 0;
    }
    
    switch (dataType) {
        case DataType::SignedByte:
            return static_cast<int64_t>(static_cast<int8_t>(rawData[0]));
            
        case DataType::SignedInt16:
            if (rawData.size() >= 2) {
                int16_t value = static_cast<int16_t>(
                    (rawData[0]) | (rawData[1] << 8));
                return static_cast<int64_t>(value);
            }
            break;
            
        case DataType::SignedInt32:
            if (rawData.size() >= 4) {
                int32_t value = static_cast<int32_t>(
                    (rawData[0]) | (rawData[1] << 8) | 
                    (rawData[2] << 16) | (rawData[3] << 24));
                return static_cast<int64_t>(value);
            }
            break;
            
        case DataType::SignedInt64:
            if (rawData.size() >= 8) {
                int64_t value = 0;
                for (int i = 0; i < 8; ++i) {
                    value |= (static_cast<int64_t>(rawData[i]) << (i * 8));
                }
                return value;
            }
            break;
            
        case DataType::Fixed16:
            if (rawData.size() >= 2) {
                int16_t fixed16 = static_cast<int16_t>(
                    (rawData[0]) | (rawData[1] << 8));
                return static_cast<int64_t>(fixed16);
            }
            break;
            
        case DataType::Boolean:
        case DataType::Void:
            return toBoolean() ? 1 : 0;
            
        case DataType::String:
            // Try to convert string to integer
            try {
                return std::stoll(toString());
            } catch (...) {
                return 0;
            }
    }
    
    return 0;
}

bool IncomingCameraControlManager::ParameterData::toBoolean() const {
    if (rawData.empty()) {
        return false;
    }
    
    // For boolean/void types, any non-zero value is true
    return rawData[0] != 0;
}

}  // namespace BMDCamera
