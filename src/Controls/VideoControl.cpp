#include "VideoControl.h"
#include "../BMDBLEController.h"
#include "../Protocol/ProtocolUtils.h"
#include <cmath>

namespace BMDCamera {

VideoControl::VideoControl(BMDBLEController* controller)
    : m_controller(controller) {
    // Constructor implementation
}

bool VideoControl::setVideoMode(const VideoMode& mode) {
    // Pack mode data into payload
    std::vector<uint8_t> payload = {
        mode.frameRate,
        static_cast<uint8_t>(mode.isMRate ? 1 : 0),
        mode.dimensions,
        static_cast<uint8_t>(mode.isInterlaced ? 1 : 0),
        mode.colorSpace
    };
    
    return sendCommand(0x00, DataType::SignedByte, OperationType::Assign, payload);
}

std::optional<VideoControl::VideoMode> VideoControl::getVideoMode() const {
    auto param = m_controller->getParameter(Category::Video, 0x00);
    
    if (!param || param->data.size() < 5) {
        return std::nullopt;
    }
    
    VideoMode mode;
    mode.frameRate = param->data[0];
    mode.isMRate = param->data[1] != 0;
    mode.dimensions = param->data[2];
    mode.isInterlaced = param->data[3] != 0;
    mode.colorSpace = param->data[4];
    
    return mode;
}

bool VideoControl::setWhiteBalance(uint16_t kelvin, int16_t tint) {
    // Create payload (4 bytes: kelvin and tint as 16-bit integers)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(kelvin & 0xFF),
        static_cast<uint8_t>((kelvin >> 8) & 0xFF),
        static_cast<uint8_t>(tint & 0xFF),
        static_cast<uint8_t>((tint >> 8) & 0xFF)
    };
    
    return sendCommand(0x02, DataType::SignedInt16, OperationType::Assign, payload);
}

bool VideoControl::getWhiteBalance(uint16_t& kelvin, int16_t& tint) const {
    auto param = m_controller->getParameter(Category::Video, 0x02);
    
    if (!param || param->data.size() < 4) {
        return false;
    }
    
    kelvin = static_cast<uint16_t>(param->data[0] | (param->data[1] << 8));
    tint = static_cast<int16_t>(param->data[2] | (param->data[3] << 8));
    
    return true;
}

bool VideoControl::triggerAutoWhiteBalance() {
    // Auto white balance is triggered with an empty command
    return sendCommand(0x03, DataType::Void, OperationType::Assign, {});
}

bool VideoControl::restoreAutoWhiteBalance() {
    // Restore auto white balance is parameter 0x04
    return sendCommand(0x04, DataType::Void, OperationType::Assign, {});
}

bool VideoControl::setExposure(uint32_t microseconds) {
    // Create payload (32-bit integer, little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(microseconds & 0xFF),
        static_cast<uint8_t>((microseconds >> 8) & 0xFF),
        static_cast<uint8_t>((microseconds >> 16) & 0xFF),
        static_cast<uint8_t>((microseconds >> 24) & 0xFF)
    };
    
    return sendCommand(0x05, DataType::SignedInt32, OperationType::Assign, payload);
}

bool VideoControl::getExposure(uint32_t& microseconds) const {
    auto param = m_controller->getParameter(Category::Video, 0x05);
    
    if (!param) {
        return false;
    }
    
    microseconds = static_cast<uint32_t>(param->toInteger());
    return true;
}

bool VideoControl::setExposureOrdinal(uint16_t ordinalValue) {
    // Create payload (16-bit integer, little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(ordinalValue & 0xFF),
        static_cast<uint8_t>((ordinalValue >> 8) & 0xFF)
    };
    
    return sendCommand(0x06, DataType::SignedInt16, OperationType::Assign, payload);
}

bool VideoControl::getExposureOrdinal(uint16_t& ordinalValue) const {
    auto param = m_controller->getParameter(Category::Video, 0x06);
    
    if (!param) {
        return false;
    }
    
    int64_t value = param->toInteger();
    if (value < 0 || value > UINT16_MAX) {
        return false;
    }
    
    ordinalValue = static_cast<uint16_t>(value);
    return true;
}

bool VideoControl::setDynamicRangeMode(DynamicRangeMode mode) {
    // Create payload (single byte)
    std::vector<uint8_t> payload = { static_cast<uint8_t>(mode) };
    
    return sendCommand(0x07, DataType::SignedByte, OperationType::Assign, payload);
}

std::optional<VideoControl::DynamicRangeMode> VideoControl::getDynamicRangeMode() const {
    auto param = m_controller->getParameter(Category::Video, 0x07);
    
    if (!param) {
        return std::nullopt;
    }
    
    int64_t value = param->toInteger();
    if (value < 0 || value > 2) {
        return std::nullopt;
    }
    
    return static_cast<DynamicRangeMode>(value);
}

bool VideoControl::setSharpeningLevel(SharpeningLevel level) {
    // Create payload (single byte)
    std::vector<uint8_t> payload = { static_cast<uint8_t>(level) };
    
    return sendCommand(0x08, DataType::SignedByte, OperationType::Assign, payload);
}

std::optional<VideoControl::SharpeningLevel> VideoControl::getSharpeningLevel() const {
    auto param = m_controller->getParameter(Category::Video, 0x08);
    
    if (!param) {
        return std::nullopt;
    }
    
    int64_t value = param->toInteger();
    if (value < 0 || value > 3) {
        return std::nullopt;
    }
    
    return static_cast<SharpeningLevel>(value);
}

bool VideoControl::setRecordingFormat(const RecordingFormat& format) {
    // Pack format data into payload (9 values, but using bit flags for booleans)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(format.fileFrameRate & 0xFF),
        static_cast<uint8_t>((format.fileFrameRate >> 8) & 0xFF),
        
        static_cast<uint8_t>(format.sensorFrameRate & 0xFF),
        static_cast<uint8_t>((format.sensorFrameRate >> 8) & 0xFF),
        
        static_cast<uint8_t>(format.frameWidth & 0xFF),
        static_cast<uint8_t>((format.frameWidth >> 8) & 0xFF),
        
        static_cast<uint8_t>(format.frameHeight & 0xFF),
        static_cast<uint8_t>((format.frameHeight >> 8) & 0xFF),
        
        // Pack boolean flags into a single byte
        static_cast<uint8_t>(
            (format.isFileMRate ? 0x01 : 0) |
            (format.isSensorMRate ? 0x02 : 0) |
            (format.isSensorOffSpeed ? 0x04 : 0) |
            (format.isInterlaced ? 0x08 : 0) |
            (format.isWindowed ? 0x10 : 0)
        )
    };
    
    return sendCommand(0x09, DataType::SignedInt16, OperationType::Assign, payload);
}

std::optional<VideoControl::RecordingFormat> VideoControl::getRecordingFormat() const {
    auto param = m_controller->getParameter(Category::Video, 0x09);
    
    if (!param || param->data.size() < 9) {
        return std::nullopt;
    }
    
    RecordingFormat format;
    format.fileFrameRate = static_cast<uint16_t>(param->data[0] | (param->data[1] << 8));
    format.sensorFrameRate = static_cast<uint16_t>(param->data[2] | (param->data[3] << 8));
    format.frameWidth = static_cast<uint16_t>(param->data[4] | (param->data[5] << 8));
    format.frameHeight = static_cast<uint16_t>(param->data[6] | (param->data[7] << 8));
    
    // Unpack boolean flags from byte
    uint8_t flags = param->data[8];
    format.isFileMRate = (flags & 0x01) != 0;
    format.isSensorMRate = (flags & 0x02) != 0;
    format.isSensorOffSpeed = (flags & 0x04) != 0;
    format.isInterlaced = (flags & 0x08) != 0;
    format.isWindowed = (flags & 0x10) != 0;
    
    return format;
}

bool VideoControl::setAutoExposureMode(AutoExposureMode mode) {
    // Create payload (single byte)
    std::vector<uint8_t> payload = { static_cast<uint8_t>(mode) };
    
    return sendCommand(0x0A, DataType::SignedByte, OperationType::Assign, payload);
}

std::optional<VideoControl::AutoExposureMode> VideoControl::getAutoExposureMode() const {
    auto param = m_controller->getParameter(Category::Video, 0x0A);
    
    if (!param) {
        return std::nullopt;
    }
    
    int64_t value = param->toInteger();
    if (value < 0 || value > 4) {
        return std::nullopt;
    }
    
    return static_cast<AutoExposureMode>(value);
}

bool VideoControl::setShutterAngle(uint32_t angleHundredths) {
    // Create payload (32-bit integer, little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(angleHundredths & 0xFF),
        static_cast<uint8_t>((angleHundredths >> 8) & 0xFF),
        static_cast<uint8_t>((angleHundredths >> 16) & 0xFF),
        static_cast<uint8_t>((angleHundredths >> 24) & 0xFF)
    };
    
    return sendCommand(0x0B, DataType::SignedInt32, OperationType::Assign, payload);
}

bool VideoControl::getShutterAngle(uint32_t& angleHundredths) const {
    auto param = m_controller->getParameter(Category::Video, 0x0B);
    
    if (!param) {
        return false;
    }
    
    angleHundredths = static_cast<uint32_t>(param->toInteger());
    return true;
}

bool VideoControl::setShutterSpeed(uint32_t speed) {
    // Create payload (32-bit integer, little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(speed & 0xFF),
        static_cast<uint8_t>((speed >> 8) & 0xFF),
        static_cast<uint8_t>((speed >> 16) & 0xFF),
        static_cast<uint8_t>((speed >> 24) & 0xFF)
    };
    
    return sendCommand(0x0C, DataType::SignedInt32, OperationType::Assign, payload);
}

bool VideoControl::getShutterSpeed(uint32_t& speed) const {
    auto param = m_controller->getParameter(Category::Video, 0x0C);
    
    if (!param) {
        return false;
    }
    
    speed = static_cast<uint32_t>(param->toInteger());
    return true;
}

bool VideoControl::setISO(uint32_t iso) {
    // Create payload (32-bit integer, little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(iso & 0xFF),
        static_cast<uint8_t>((iso >> 8) & 0xFF),
        static_cast<uint8_t>((iso >> 16) & 0xFF),
        static_cast<uint8_t>((iso >> 24) & 0xFF)
    };
    
    return sendCommand(0x0E, DataType::SignedInt32, OperationType::Assign, payload);
}

bool VideoControl::getISO(uint32_t& iso) const {
    auto param = m_controller->getParameter(Category::Video, 0x0E);
    
    if (!param) {
        return false;
    }
    
    iso = static_cast<uint32_t>(param->toInteger());
    return true;
}

bool VideoControl::setGain(int8_t gainDB) {
    // Create payload (single byte)
    std::vector<uint8_t> payload = { static_cast<uint8_t>(gainDB) };
    
    return sendCommand(0x0D, DataType::SignedByte, OperationType::Assign, payload);
}

bool VideoControl::getGain(int8_t& gainDB) const {
    auto param = m_controller->getParameter(Category::Video, 0x0D);
    
    if (!param) {
        return false;
    }
    
    int64_t value = param->toInteger();
    // Ensure it fits in int8_t range
    if (value < -128 || value > 127) {
        return false;
    }
    
    gainDB = static_cast<int8_t>(value);
    return true;
}

bool VideoControl::setNDFilter(float stop) {
    // Convert float to fixed16 format
    uint16_t fixed16Value = static_cast<uint16_t>(stop * 2048.0f);
    
    // Create payload (16-bit little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(fixed16Value & 0xFF),
        static_cast<uint8_t>((fixed16Value >> 8) & 0xFF)
    };
    
    return sendCommand(0x16, DataType::Fixed16, OperationType::Assign, payload);
}

bool VideoControl::getNDFilter(float& stop) const {
    auto param = m_controller->getParameter(Category::Video, 0x16);
    
    if (!param) {
        return false;
    }
    
    stop = param->toFloat();
    return true;
}

bool VideoControl::setNDFilterDisplayMode(NDFilterDisplayMode mode) {
    // Create payload (single byte)
    std::vector<uint8_t> payload = { static_cast<uint8_t>(mode) };
    
    return sendCommand(0x1E, DataType::SignedByte, OperationType::Assign, payload);
}

std::optional<VideoControl::NDFilterDisplayMode> VideoControl::getNDFilterDisplayMode() const {
    auto param = m_controller->getParameter(Category::Video, 0x1E);
    
    if (!param) {
        return std::nullopt;
    }
    
    int64_t value = param->toInteger();
    if (value < 0 || value > 2) {
        return std::nullopt;
    }
    
    return static_cast<NDFilterDisplayMode>(value);
}

bool VideoControl::setDisplayLUT(const LUTSettings& settings) {
    // Create payload (2 bytes)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(settings.selectedLUT),
        static_cast<uint8_t>(settings.enabled ? 1 : 0)
    };
    
    return sendCommand(0x0F, DataType::SignedByte, OperationType::Assign, payload);
}

std::optional<VideoControl::LUTSettings> VideoControl::getDisplayLUT() const {
    auto param = m_controller->getParameter(Category::Video, 0x0F);
    
    if (!param || param->data.size() < 2) {
        return std::nullopt;
    }
    
    LUTSettings settings;
    
    uint8_t lutValue = param->data[0];
    if (lutValue > 3) {
        return std::nullopt;
    }
    
    settings.selectedLUT = static_cast<DisplayLUT>(lutValue);
    settings.enabled = param->data[1] != 0;
    
    return settings;
}

bool VideoControl::sendCommand(
    uint8_t parameter,
    DataType dataType,
    OperationType operation,
    const std::vector<uint8_t>& payload
) {
    // Use the controller to send the command
    return m_controller->sendCommand(
        Category::Video,
        parameter,
        dataType,
        operation,
        payload
    );
}

} // namespace BMDCamera
