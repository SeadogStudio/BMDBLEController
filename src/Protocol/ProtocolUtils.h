#ifndef BMD_PROTOCOL_UTILS_H
#define BMD_PROTOCOL_UTILS_H

#include <vector>
#include <cstdint>
#include <string>
#include "../Protocol/ProtocolConstants.h"

namespace BMDCamera {
    class ProtocolUtils {
    public:
        // Convert raw bytes to specific data types
        static std::string bytesToString(const std::vector<uint8_t>& data);
        static int32_t bytesToInt32(const std::vector<uint8_t>& data);
        static int16_t bytesToInt16(const std::vector<uint8_t>& data);
        static float bytesToFloat(const std::vector<uint8_t>& data);
        static bool bytesToBoolean(const std::vector<uint8_t>& data);

        // Create command packets
        static std::vector<uint8_t> createCommandPacket(
            Category category,
            uint8_t parameter,
            DataType dataType,
            OperationType operation,
            const std::vector<uint8_t>& payload
        );

        // Validate packet structure
        static bool validatePacket(const std::vector<uint8_t>& packet);

        // Get human-readable name for category, parameter, etc.
        static std::string getCategoryName(Category category);
        static std::string getDataTypeName(DataType dataType);
        static std::string getOperationTypeName(OperationType operationType);

        // Hex representation utilities
        static std::string bytesToHexString(const std::vector<uint8_t>& data);
        static std::vector<uint8_t> hexStringToBytes(const std::string& hexString);

        // Fixed16 (5.11) conversion utilities
        static float fixed16ToFloat(uint16_t fixed16Value);
        static uint16_t floatToFixed16(float value);

    private:
        // Endian conversion helpers
        static uint16_t littleEndianToHost16(const uint8_t* data);
        static uint32_t littleEndianToHost32(const uint8_t* data);
    };
}

#endif // BMD_PROTOCOL_UTILS_H
