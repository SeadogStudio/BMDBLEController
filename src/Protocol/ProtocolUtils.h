/**
 * @file ProtocolUtils.h
 * @brief Utility functions for Blackmagic Design Camera Control Protocol via BLE
 * @author BMDBLEController Contributors
 */

#ifndef PROTOCOL_UTILS_H
#define PROTOCOL_UTILS_H

#include <Arduino.h>
#include <vector>
#include "ProtocolConstants.h"

namespace BMDBLEController {

class ProtocolUtils {
public:
    /**
     * @brief Convert a float value to fixed16 (5.11 fixed point) format
     * @param value The float value to convert
     * @return The fixed16 representation as 16-bit integer
     */
    static int16_t floatToFixed16(float value) {
        return static_cast<int16_t>(value * FIXED16_DIVISOR);
    }

    /**
     * @brief Convert a fixed16 (5.11 fixed point) value to float
     * @param fixed16Value The fixed16 value to convert
     * @return The float representation
     */
    static float fixed16ToFloat(int16_t fixed16Value) {
        return static_cast<float>(fixed16Value) / FIXED16_DIVISOR;
    }

    /**
     * @brief Create a command packet for Blackmagic camera control
     * @param category The command category (e.g., BMD_CAT_LENS)
     * @param parameter The parameter ID within the category
     * @param dataType The data type (e.g., BMD_TYPE_FIXED16)
     * @param operation The operation to perform (e.g., BMD_OP_ASSIGN)
     * @param data Pointer to the data to include
     * @param dataSize Size of the data in bytes
     * @return Vector containing the complete command packet
     */
    static std::vector<uint8_t> createCommandPacket(
            uint8_t category,
            uint8_t parameter,
            uint8_t dataType,
            uint8_t operation,
            const uint8_t* data,
            size_t dataSize) {
        
        // Calculate command length (data + 4 bytes for category, parameter, dataType, operation)
        uint8_t commandLength = dataSize + 4;
        
        // Create the command packet with header
        std::vector<uint8_t> packet;
        packet.reserve(commandLength + 4); // +4 for protocol identifier, length, command ID, reserved
        
        // Header
        packet.push_back(PROTOCOL_IDENTIFIER);   // Protocol identifier (0xFF)
        packet.push_back(commandLength);         // Command length
        packet.push_back(COMMAND_ID);            // Command ID (0x00)
        packet.push_back(RESERVED_BYTE);         // Reserved byte (0x00)
        
        // Command data
        packet.push_back(category);              // Category
        packet.push_back(parameter);             // Parameter
        packet.push_back(dataType);              // Data type
        packet.push_back(operation);             // Operation
        
        // Add the data
        if (data != nullptr && dataSize > 0) {
            packet.insert(packet.end(), data, data + dataSize);
        }
        
        // Add padding to make the total length a multiple of 4 if needed
        size_t paddingBytes = (4 - (packet.size() % 4)) % 4;
        for (size_t i = 0; i < paddingBytes; i++) {
            packet.push_back(0x00);
        }
        
        return packet;
    }

    /**
     * @brief Create a command packet for setting an 8-bit value
     * @param category The command category
     * @param parameter The parameter ID
     * @param value The 8-bit value to set
     * @param operation The operation (default: BMD_OP_ASSIGN)
     * @return Vector containing the complete command packet
     */
    static std::vector<uint8_t> createInt8CommandPacket(
            uint8_t category,
            uint8_t parameter,
            int8_t value,
            uint8_t operation = BMD_OP_ASSIGN) {
        
        return createCommandPacket(
            category,
            parameter,
            BMD_TYPE_BYTE,
            operation,
            reinterpret_cast<const uint8_t*>(&value),
            sizeof(value)
        );
    }

    /**
     * @brief Create a command packet for setting a 16-bit value
     * @param category The command category
     * @param parameter The parameter ID
     * @param value The 16-bit value to set
     * @param operation The operation (default: BMD_OP_ASSIGN)
     * @return Vector containing the complete command packet
     */
    static std::vector<uint8_t> createInt16CommandPacket(
            uint8_t category,
            uint8_t parameter,
            int16_t value,
            uint8_t operation = BMD_OP_ASSIGN) {
        
        return createCommandPacket(
            category,
            parameter,
            BMD_TYPE_INT16,
            operation,
            reinterpret_cast<const uint8_t*>(&value),
            sizeof(value)
        );
    }

    /**
     * @brief Create a command packet for setting a 32-bit value
     * @param category The command category
     * @param parameter The parameter ID
     * @param value The 32-bit value to set
     * @param operation The operation (default: BMD_OP_ASSIGN)
     * @return Vector containing the complete command packet
     */
    static std::vector<uint8_t> createInt32CommandPacket(
            uint8_t category,
            uint8_t parameter,
            int32_t value,
            uint8_t operation = BMD_OP_ASSIGN) {
        
        return createCommandPacket(
            category,
            parameter,
            BMD_TYPE_INT32,
            operation,
            reinterpret_cast<const uint8_t*>(&value),
            sizeof(value)
        );
    }

    /**
     * @brief Create a command packet for setting a fixed16 (5.11) value
     * @param category The command category
     * @param parameter The parameter ID
     * @param value The float value to convert to fixed16
     * @param operation The operation (default: BMD_OP_ASSIGN)
     * @return Vector containing the complete command packet
     */
    static std::vector<uint8_t> createFixed16CommandPacket(
            uint8_t category,
            uint8_t parameter,
            float value,
            uint8_t operation = BMD_OP_ASSIGN) {
        
        int16_t fixed16Value = floatToFixed16(value);
        
        return createCommandPacket(
            category,
            parameter,
            BMD_TYPE_FIXED16,
            operation,
            reinterpret_cast<const uint8_t*>(&fixed16Value),
            sizeof(fixed16Value)
        );
    }

    /**
     * @brief Create a command packet for setting a string value
     * @param category The command category
     * @param parameter The parameter ID
     * @param value The string value to set
     * @param operation The operation (default: BMD_OP_ASSIGN)
     * @return Vector containing the complete command packet
     */
    static std::vector<uint8_t> createStringCommandPacket(
            uint8_t category,
            uint8_t parameter,
            const String& value,
            uint8_t operation = BMD_OP_ASSIGN) {
        
        return createCommandPacket(
            category,
            parameter,
            BMD_TYPE_STRING,
            operation,
            reinterpret_cast<const uint8_t*>(value.c_str()),
            value.length()
        );
    }

    /**
     * @brief Create a command packet for requesting a parameter value
     * @param category The command category
     * @param parameter The parameter ID
     * @param dataType The expected data type
     * @return Vector containing the complete command packet
     */
    static std::vector<uint8_t> createRequestPacket(
            uint8_t category,
            uint8_t parameter,
            uint8_t dataType) {
        
        return createCommandPacket(
            category,
            parameter,
            dataType,
            BMD_OP_REPORT,
            nullptr,
            0
        );
    }

    /**
     * @brief Extract an 8-bit value from a response packet
     * @param packet The response packet
     * @param offset The offset in the packet (default: 8, after the header)
     * @return The extracted 8-bit value
     */
    static int8_t extractInt8FromPacket(const std::vector<uint8_t>& packet, size_t offset = 8) {
        if (packet.size() <= offset) {
            return 0;
        }
        return static_cast<int8_t>(packet[offset]);
    }

    /**
     * @brief Extract a 16-bit value from a response packet
     * @param packet The response packet
     * @param offset The offset in the packet (default: 8, after the header)
     * @return The extracted 16-bit value
     */
    static int16_t extractInt16FromPacket(const std::vector<uint8_t>& packet, size_t offset = 8) {
        if (packet.size() <= offset + 1) {
            return 0;
        }
        
        int16_t value = 0;
        value |= packet[offset];
        value |= (static_cast<int16_t>(packet[offset + 1]) << 8);
        
        return value;
    }

    /**
     * @brief Extract a 32-bit value from a response packet
     * @param packet The response packet
     * @param offset The offset in the packet (default: 8, after the header)
     * @return The extracted 32-bit value
     */
    static int32_t extractInt32FromPacket(const std::vector<uint8_t>& packet, size_t offset = 8) {
        if (packet.size() <= offset + 3) {
            return 0;
        }
        
        int32_t value = 0;
        value |= packet[offset];
        value |= (static_cast<int32_t>(packet[offset + 1]) << 8);
        value |= (static_cast<int32_t>(packet[offset + 2]) << 16);
        value |= (static_cast<int32_t>(packet[offset + 3]) << 24);
        
        return value;
    }

    /**
     * @brief Extract a fixed16 (5.11) value from a response packet and convert to float
     * @param packet The response packet
     * @param offset The offset in the packet (default: 8, after the header)
     * @return The extracted float value
     */
    static float extractFixed16FromPacket(const std::vector<uint8_t>& packet, size_t offset = 8) {
        int16_t fixed16Value = extractInt16FromPacket(packet, offset);
        return fixed16ToFloat(fixed16Value);
    }

    /**
     * @brief Extract a string value from a response packet
     * @param packet The response packet
     * @param offset The offset in the packet (default: 8, after the header)
     * @return The extracted string value
     */
    static String extractStringFromPacket(const std::vector<uint8_t>& packet, size_t offset = 8) {
        if (packet.size() <= offset) {
            return "";
        }
        
        // Calculate string length (to the end of the packet)
        size_t stringLength = packet.size() - offset;
        
        // Create a buffer and copy the data
        char* buffer = new char[stringLength + 1];
        for (size_t i = 0; i < stringLength; i++) {
            buffer[i] = static_cast<char>(packet[offset + i]);
        }
        buffer[stringLength] = '\0';
        
        // Create a String from the buffer
        String result(buffer);
        
        // Clean up
        delete[] buffer;
        
        return result;
    }

    /**
     * @brief Extract the category from a response packet
     * @param packet The response packet
     * @return The category value
     */
    static uint8_t extractCategory(const std::vector<uint8_t>& packet) {
        if (packet.size() <= 4) {
            return 0;
        }
        return packet[4];
    }

    /**
     * @brief Extract the parameter ID from a response packet
     * @param packet The response packet
     * @return The parameter ID
     */
    static uint8_t extractParameter(const std::vector<uint8_t>& packet) {
        if (packet.size() <= 5) {
            return 0;
        }
        return packet[5];
    }

    /**
     * @brief Extract the data type from a response packet
     * @param packet The response packet
     * @return The data type
     */
    static uint8_t extractDataType(const std::vector<uint8_t>& packet) {
        if (packet.size() <= 6) {
            return 0;
        }
        return packet[6];
    }

    /**
     * @brief Extract the operation from a response packet
     * @param packet The response packet
     * @return The operation
     */
    static uint8_t extractOperation(const std::vector<uint8_t>& packet) {
        if (packet.size() <= 7) {
            return 0;
        }
        return packet[7];
    }

    /**
     * @brief Convert a byte array to a hexadecimal string for debugging
     * @param data The byte array
     * @param length The length of the array
     * @return A string representing the hexadecimal values
     */
    static String byteArrayToHexString(const uint8_t* data, size_t length) {
        String result;
        result.reserve(length * 3); // Each byte becomes 2 hex chars + a space
        
        for (size_t i = 0; i < length; i++) {
            if (data[i] < 0x10) {
                result += "0"; // Add leading zero for values < 0x10
            }
            result += String(data[i], HEX);
            if (i < length - 1) {
                result += " ";
            }
        }
        
        return result;
    }

    /**
     * @brief Convert a vector of bytes to a hexadecimal string for debugging
     * @param data The vector of bytes
     * @return A string representing the hexadecimal values
     */
    static String vectorToHexString(const std::vector<uint8_t>& data) {
        return byteArrayToHexString(data.data(), data.size());
    }
};

} // namespace BMDBLEController

#endif // PROTOCOL_UTILS_H
