// src/Protocol/ProtocolUtils.cpp
#include "ProtocolUtils.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace BMDCamera {

std::string ProtocolUtils::bytesToString(const std::vector<uint8_t>& data) {
    // Create string from bytes, assuming UTF-8 encoding
    return std::string(data.begin(), data.end());
}

int32_t ProtocolUtils::bytesToInt32(const std::vector<uint8_t>& data) {
    if (data.size() < 4) {
        return 0;
    }
    
    return littleEndianToHost32(data.data());
}

int16_t ProtocolUtils::bytesToInt16(const std::vector<uint8_t>& data) {
    if (data.size() < 2) {
        return 0;
    }
    
    return littleEndianToHost16(data.data());
}

float ProtocolUtils::bytesToFloat(const std::vector<uint8_t>& data) {
    if (data.size() < 2) {
        return 0.0f;
    }
    
    // Assume fixed16 format for float conversion
    uint16_t fixed16 = littleEndianToHost16(data.data());
    return fixed16ToFloat(fixed16);
}

bool ProtocolUtils::bytesToBoolean(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return false;
    }
    
    return data[0] != 0;
}

std::vector<uint8_t> ProtocolUtils::createCommandPacket(
    Category category,
    uint8_t parameter,
    DataType dataType,
    OperationType operation,
    const std::vector<uint8_t>& payload
) {
    // Calculate packet length (payload + 6 bytes for category, parameter, type, operation)
    uint8_t packetLength = static_cast<uint8_t>(payload.size() + 6);
    
    // Create packet with header
    std::vector<uint8_t> packet;
    packet.reserve(packetLength + 2);  // +2 for protocol ID and length byte
    
    // Protocol ID
    packet.push_back(0xFF);
    
    // Length field (doesn't include protocol ID and length byte itself)
    packet.push_back(packetLength);
    
    // Command ID and reserved byte
    packet.push_back(0x00);
    packet.push_back(0x00);
    
    // Category, parameter, data type, operation
    packet.push_back(static_cast<uint8_t>(category));
    packet.push_back(parameter);
    packet.push_back(static_cast<uint8_t>(dataType));
    packet.push_back(static_cast<uint8_t>(operation));
    
    // Payload
    packet.insert(packet.end(), payload.begin(), payload.end());
    
    // Add padding to align to 32-bit boundary if needed
    while (packet.size() % 4 != 0) {
        packet.push_back(0x00);
    }
    
    return packet;
}

bool ProtocolUtils::validatePacket(const std::vector<uint8_t>& packet) {
    // Basic validation
    if (packet.size() < 8) {
        return false;
    }
    
    // Check protocol identifier
    if (packet[0] != 0xFF) {
        return false;
    }
    
    // Check packet length
    uint8_t declaredLength = packet[1];
    if (declaredLength != packet.size() - 2) {  // -2 for protocol ID and length byte
        return false;
    }
    
    // Check command ID and reserved byte
    if (packet[2] != 0x00 || packet[3] != 0x00) {
        return false;
    }
    
    return true;
}

std::string ProtocolUtils::getCategoryName(Category category) {
    switch (category) {
        case Category::Lens: return "Lens";
        case Category::Video: return "Video";
        case Category::Audio: return "Audio";
        case Category::Output: return "Output";
        case Category::Display: return "Display";
        case Category::Tally: return "Tally";
        case Category::Reference: return "Reference";
        case Category::Configuration: return "Configuration";
        case Category::ColorCorrection: return "Color Correction";
        case Category::Status: return "Status";
        case Category::Transport: return "Transport";
        case Category::Timeline: return "Timeline";
        case Category::Media: return "Media";
        case Category::ExtendedLens: return "Extended Lens";
        default: return "Unknown";
    }
}

std::string ProtocolUtils::getDataTypeName(DataType dataType) {
    switch (dataType) {
        case DataType::Void: return "Void";
        case DataType::Boolean: return "Boolean";
        case DataType::SignedByte: return "Signed Byte";
        case DataType::SignedInt16: return "Signed Int16";
        case DataType::SignedInt32: return "Signed Int32";
        case DataType::SignedInt64: return "Signed Int64";
        case DataType::String: return "String";
        case DataType::Fixed16: return "Fixed16";
        default: return "Unknown";
    }
}

std::string ProtocolUtils::getOperationTypeName(OperationType operationType) {
    switch (operationType) {
        case OperationType::Assign: return "Assign";
        case OperationType::Offset: return "Offset";
        case OperationType::Report: return "Report";
        default: return "Unknown";
    }
}

std::string ProtocolUtils::bytesToHexString(const std::vector<uint8_t>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    for (auto byte : data) {
        ss << std::setw(2) << static_cast<int>(byte) << " ";
    }
    
    std::string result = ss.str();
    if (!result.empty()) {
        result.pop_back();  // Remove trailing space
    }
    
    return result;
}

std::vector<uint8_t> ProtocolUtils::hexStringToBytes(const std::string& hexString) {
    std::vector<uint8_t> bytes;
    
    // Remove any spaces or other separators
    std::string cleanHex;
    std::copy_if(hexString.begin(), hexString.end(), std::back_inserter(cleanHex),
                [](char c) { return std::isxdigit(c); });
    
    // Ensure even number of digits
    if (cleanHex.length() % 2 != 0) {
        cleanHex = "0" + cleanHex;
    }
    
    // Convert hex string to bytes
    for (size_t i = 0; i < cleanHex.length(); i += 2) {
        std::string byteString = cleanHex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoi(byteString, nullptr, 16));
        bytes.push_back(byte);
    }
    
    return bytes;
}

float ProtocolUtils::fixed16ToFloat(uint16_t fixed16Value) {
    // Convert from 5.11 fixed point format to float
    // 5 bits integer, 11 bits fractional
    return static_cast<float>(static_cast<int16_t>(fixed16Value)) / 2048.0f;
}

uint16_t ProtocolUtils::floatToFixed16(float value) {
    // Convert float to 5.11 fixed point format
    // 5 bits integer, 11 bits fractional
    return static_cast<uint16_t>(value * 2048.0f);
}

uint16_t ProtocolUtils::littleEndianToHost16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

uint32_t ProtocolUtils::littleEndianToHost32(const uint8_t* data) {
    return static_cast<uint32_t>(
        data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
}

}  // namespace BMDCamera
