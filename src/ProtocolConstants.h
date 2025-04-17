/**
 * @file ProtocolConstants.h
 * @brief Constant definitions for Blackmagic Design Camera Control Protocol via BLE
 * @author BMDBLEController Contributors
 */

#ifndef PROTOCOL_CONSTANTS_H
#define PROTOCOL_CONSTANTS_H

#include <Arduino.h>

namespace BMDBLEController {

// BLE Service and Characteristic UUIDs
const char* SERVICE_UUID = "291d567a-6d75-11e6-8b77-86f30ca893d3";
const char* OUTGOING_CAMERA_CONTROL_UUID = "5DD3465F-1AEE-4299-8493-D2ECA2F8E1BB";
const char* INCOMING_CAMERA_CONTROL_UUID = "B864E140-76A0-416A-BF30-5876504537D9";
const char* TIMECODE_UUID = "6D8F2110-86F1-41BF-9AFB-451D87E976C8";
const char* CAMERA_STATUS_UUID = "7FE8691D-95DC-4FC5-8ABD-CA74339B51B9";
const char* DEVICE_NAME_UUID = "FFAC0C52-C9FB-41A0-B063-CC76282EB89C";

// Protocol Categories
const uint8_t BMD_CAT_LENS = 0x00;
const uint8_t BMD_CAT_VIDEO = 0x01;
const uint8_t BMD_CAT_AUDIO = 0x02;
const uint8_t BMD_CAT_OUTPUT = 0x03;
const uint8_t BMD_CAT_DISPLAY = 0x04;
const uint8_t BMD_CAT_TALLY = 0x05;
const uint8_t BMD_CAT_REFERENCE = 0x06;
const uint8_t BMD_CAT_CONFIG = 0x07;
const uint8_t BMD_CAT_COLOR = 0x08;
const uint8_t BMD_CAT_STATUS = 0x09;
const uint8_t BMD_CAT_TRANSPORT = 0x0A;
const uint8_t BMD_CAT_EXTENDED_LENS = 0x0C;

// Parameter IDs - Lens Category (0x00)
const uint8_t BMD_PARAM_FOCUS = 0x00;
const uint8_t BMD_PARAM_APERTURE_FSTOP = 0x02;
const uint8_t BMD_PARAM_APERTURE_NORM = 0x03;
const uint8_t BMD_PARAM_AUTO_APERTURE = 0x05;
const uint8_t BMD_PARAM_ZOOM_MM = 0x07;
const uint8_t BMD_PARAM_ZOOM_NORM = 0x08;

// Parameter IDs - Video Category (0x01)
const uint8_t BMD_PARAM_WB = 0x02;
const uint8_t BMD_PARAM_GAIN = 0x0D;
const uint8_t BMD_PARAM_ISO = 0x0E;
const uint8_t BMD_PARAM_SHUTTER_ANGLE = 0x0B;
const uint8_t BMD_PARAM_SHUTTER_SPEED = 0x0C;
const uint8_t BMD_PARAM_DYNAMIC_RANGE = 0x07;

// Parameter IDs - Transport (0x0A) Category
const uint8_t BMD_PARAM_RECORDING = 0x01;

// Data Types
const uint8_t BMD_TYPE_VOID = 0x00;     // Boolean/Void
const uint8_t BMD_TYPE_BYTE = 0x01;     // Signed byte
const uint8_t BMD_TYPE_INT16 = 0x02;    // Signed 16-bit integer
const uint8_t BMD_TYPE_INT32 = 0x03;    // Signed 32-bit integer
const uint8_t BMD_TYPE_INT64 = 0x04;    // Signed 64-bit integer
const uint8_t BMD_TYPE_STRING = 0x05;   // UTF-8 string
const uint8_t BMD_TYPE_FIXED16 = 0x80;  // 5.11 fixed point

// Operation Types
const uint8_t BMD_OP_ASSIGN = 0x00;   // Assign value
const uint8_t BMD_OP_OFFSET = 0x01;   // Offset/toggle value
const uint8_t BMD_OP_REPORT = 0x02;   // Request/report value

// Error Codes
const int16_t BMD_ERROR_NONE = 0;
const int16_t BMD_ERROR_NOT_CONNECTED = -1;
const int16_t BMD_ERROR_NOT_BONDED = -2;
const int16_t BMD_ERROR_CONNECTION_FAILED = -3;
const int16_t BMD_ERROR_PIN_ENTRY_FAILED = -4;
const int16_t BMD_ERROR_COMMAND_FAILED = -5;
const int16_t BMD_ERROR_INVALID_PARAMETER = -6;

// Dynamic Range Mode Values
const uint8_t BMD_DYNAMIC_RANGE_FILM = 0x00;
const uint8_t BMD_DYNAMIC_RANGE_VIDEO = 0x01;
const uint8_t BMD_DYNAMIC_RANGE_EXTENDED = 0x02;

// Protocol Constants
const uint8_t PROTOCOL_IDENTIFIER = 0xFF;
const uint8_t COMMAND_ID = 0x00;
const uint8_t RESERVED_BYTE = 0x00;
const uint8_t BROADCAST_DESTINATION = 0xFF;

// Utilities and Conversions
const float FIXED16_DIVISOR = 2048.0f;  // 2^11 for 5.11 fixed point format

} // namespace BMDBLEController

#endif // PROTOCOL_CONSTANTS_H
