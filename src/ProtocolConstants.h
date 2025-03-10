#ifndef BMD_PROTOCOL_CONSTANTS_H
#define BMD_PROTOCOL_CONSTANTS_H

#include <cstdint>

namespace BMDCamera {
    // Service and Characteristic UUIDs
    constexpr const char* SERVICE_UUID = "291d567a-6d75-11e6-8b77-86f30ca893d3";
    constexpr const char* OUTGOING_CONTROL_UUID = "5DD3465F-1AEE-4299-8493-D2ECA2F8E1BB";
    constexpr const char* INCOMING_CONTROL_UUID = "B864E140-76A0-416A-BF30-5876504537D9";
    constexpr const char* TIMECODE_UUID = "6D8F2110-86F1-41BF-9AFB-451D87E976C8";
    constexpr const char* CAMERA_STATUS_UUID = "7FE8691D-95DC-4FC5-8ABD-CA74339B51B9";
    constexpr const char* DEVICE_NAME_UUID = "FFAC0C52-C9FB-41A0-B063-CC76282EB89C";

    // Protocol Categories
    enum class Category : uint8_t {
        Lens = 0x00,
        Video = 0x01,
        Audio = 0x02,
        Output = 0x03,
        Display = 0x04,
        Tally = 0x05,
        Reference = 0x06,
        Configuration = 0x07,
        ColorCorrection = 0x08,
        Status = 0x09,
        Transport = 0x0A,
        ExtendedLens = 0x0C
    };

    // Data Types
    enum class DataType : uint8_t {
        Void = 0x00,
        SignedByte = 0x01,
        SignedInt16 = 0x02,
        SignedInt32 = 0x03,
        SignedInt64 = 0x04,
        UTF8String = 0x05,
        Fixed16 = 0x80
    };

    // Operation Types
    enum class OperationType : uint8_t {
        Assign = 0x00,
        Offset = 0x01,
        Report = 0x02
    };

    // Known Parameters by Category (extensible)
    struct KnownParameter {
        Category category;
        uint8_t parameterID;
        DataType dataType;
        const char* name;
    };

    // Predefined known parameters (can be expanded)
    constexpr KnownParameter KnownParameters[] = {
        // Lens Category
        {Category::Lens, 0x00, DataType::Fixed16, "Focus"},
        {Category::Lens, 0x02, DataType::Fixed16, "Aperture (f-stop)"},
        
        // Video Category
        {Category::Video, 0x02, DataType::SignedInt16, "White Balance"},
        {Category::Video, 0x0B, DataType::SignedInt32, "Shutter Angle"},
        
        // More parameters can be added here
    };
}

#endif // BMD_PROTOCOL_CONSTANTS_H
