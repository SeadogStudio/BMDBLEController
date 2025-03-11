#ifndef BMD_PROTOCOL_CONSTANTS_H
#define BMD_PROTOCOL_CONSTANTS_H

namespace BMDCamera {
    // Blackmagic Camera Service UUID
    constexpr const char* BMD_SERVICE_UUID = "291d567a-6d75-11e6-8b77-86f30ca893d3";
    constexpr const char* BMD_OUTGOING_CONTROL_UUID = "5DD3465F-1AEE-4299-8493-D2ECA2F8E1BB";
    constexpr const char* BMD_INCOMING_CONTROL_UUID = "B864E140-76A0-416A-BF30-5876504537D9";
    constexpr const char* BMD_TIMECODE_UUID = "6D8F2110-86F1-41BF-9AFB-451D87E976C8";
    constexpr const char* BMD_CAMERA_STATUS_UUID = "7FE8691D-95DC-4FC5-8ABD-CA74339B51B9";
    constexpr const char* BMD_DEVICE_NAME_UUID = "FFAC0C52-C9FB-41A0-B063-CC76282EB89C";

    // Protocol Categories
    enum ProtocolCategory : uint8_t {
        CAT_LENS = 0x00,
        CAT_VIDEO = 0x01,
        CAT_AUDIO = 0x02,
        CAT_OUTPUT = 0x03,
        CAT_DISPLAY = 0x04,
        CAT_TALLY = 0x05,
        CAT_REFERENCE = 0x06,
        CAT_CONFIG = 0x07,
        CAT_COLOR = 0x08,
        CAT_STATUS = 0x09,
        CAT_TRANSPORT = 0x0A,
        CAT_EXTENDED_LENS = 0x0C
    };

    // Data Types
    enum ProtocolDataType : uint8_t {
        TYPE_VOID = 0x00,
        TYPE_BYTE = 0x01,
        TYPE_INT16 = 0x02,
        TYPE_INT32 = 0x03,
        TYPE_INT64 = 0x04,
        TYPE_STRING = 0x05,
        TYPE_FIXED16 = 0x80
    };

    // Operation Types
    enum ProtocolOperation : uint8_t {
        OP_ASSIGN = 0x00,
        OP_OFFSET = 0x01,
        OP_REPORT = 0x02
    };

    // Error Codes
    enum ErrorCode {
        ERROR_NONE = 0,
        ERROR_NO_CAMERA_FOUND = 1,
        ERROR_CONNECTION_FAILED = 2,
        ERROR_NO_SAVED_CAMERA = 3,
        ERROR_AUTHENTICATION_FAILED = 4,
        ERROR_UNKNOWN = 255
    };
}

#endif // BMD_PROTOCOL_CONSTANTS_H
