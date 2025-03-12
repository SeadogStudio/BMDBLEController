// AudioControl.cpp
#include "AudioControl.h"
#include "../BMDBLEController.h"
#include "../Protocol/ProtocolUtils.h"
#include <cmath>

namespace BMDCamera {

AudioControl::AudioControl(BMDBLEController* controller)
    : m_controller(controller) {
    // Constructor implementation
}

bool AudioControl::setChannelInput(uint8_t channelIndex, InputType inputType) {
    // Create payload (string value)
    std::string inputName;
    
    switch (inputType) {
        case InputType::None:
            inputName = "None";
            break;
        case InputType::CameraLeft:
            inputName = "Camera - Left";
            break;
        case InputType::CameraRight:
            inputName = "Camera - Right";
            break;
        case InputType::CameraMono:
            inputName = "Camera - Mono";
            break;
        case InputType::XLR1Mic:
            inputName = "XLR1 - Mic";
            break;
        case InputType::XLR1Line:
            inputName = "XLR1 - Line";
            break;
        case InputType::XLR2Mic:
            inputName = "XLR2 - Mic";
            break;
        case InputType::XLR2Line:
            inputName = "XLR2 - Line";
            break;
        case InputType::Line3_5mmLeft:
            inputName = "3.5mm Left - Line";
            break;
        case InputType::Mic3_5mmLeft:
            inputName = "3.5mm Left - Mic";
            break;
        case InputType::Line3_5mmRight:
            inputName = "3.5mm Right - Line";
            break;
        case InputType::Mic3_5mmRight:
            inputName = "3.5mm Right - Mic";
            break;
        case InputType::Line3_5mmMono:
            inputName = "3.5mm Mono - Line";
            break;
        case InputType::Mic3_5mmMono:
            inputName = "3.5mm Mono - Mic";
            break;
        default:
            return false;
    }
    
    // Convert string to vector of bytes
    std::vector<uint8_t> payload(inputName.begin(), inputName.end());
    
    // Send command to specific channel
    return m_controller->sendCommand(
        Category::Audio,
        channelIndex,
        DataType::UTF8String,
        OperationType::Assign,
        payload
    );
}

std::optional<AudioControl::InputType> AudioControl::getChannelInput(uint8_t channelIndex) const {
    auto param = m_controller->getParameter(Category::Audio, channelIndex);
    
    if (!param) {
        return std::nullopt;
    }
    
    std::string inputName = param->toString();
    
    // Map string back to enum
    if (inputName == "None")
        return InputType::None;
    else if (inputName == "Camera - Left")
        return InputType::CameraLeft;
    else if (inputName == "Camera - Right")
        return InputType::CameraRight;
    else if (inputName == "Camera - Mono")
        return InputType::CameraMono;
    else if (inputName == "XLR1 - Mic")
        return InputType::XLR1Mic;
    else if (inputName == "XLR1 - Line")
        return InputType::XLR1Line;
    else if (inputName == "XLR2 - Mic")
        return InputType::XLR2Mic;
    else if (inputName == "XLR2 - Line")
        return InputType::XLR2Line;
    else if (inputName == "3.5mm Left - Line")
        return InputType::Line3_5mmLeft;
    else if (inputName == "3.5mm Left - Mic")
        return InputType::Mic3_5mmLeft;
    else if (inputName == "3.5mm Right - Line")
        return InputType::Line3_5mmRight;
    else if (inputName == "3.5mm Right - Mic")
        return InputType::Mic3_5mmRight;
    else if (inputName == "3.5mm Mono - Line")
        return InputType::Line3_5mmMono;
    else if (inputName == "3.5mm Mono - Mic")
        return InputType::Mic3_5mmMono;
    
    return std::nullopt;
}

bool AudioControl::getInputDescription(uint8_t channelIndex, InputDescription& description) const {
    // Parameter code is channel_index + 1 to get description
    auto param = m_controller->getParameter(Category::Audio, channelIndex + 1);
    
    if (!param || param->data.size() < 12) {
        return false;
    }
    
    // Parse binary data for description
    // First 4 bytes: gain range min/max (2 float16 values)
    uint16_t minRaw = param->data[0] | (param->data[1] << 8);
    uint16_t maxRaw = param->data[2] | (param->data[3] << 8);
    
    description.gainRange.min = static_cast<float>(minRaw) / 256.0f; // Convert to dB
    description.gainRange.max = static_cast<float>(maxRaw) / 256.0f;
    
    // Capabilities flags
    uint8_t capFlags = param->data[4];
    description.capabilities.supportsPhantomPower = (capFlags & 0x01) != 0;
    description.capabilities.supportsLowCutFilter = (capFlags & 0x02) != 0;
    
    // Padding info
    uint8_t padFlags = param->data[5];
    description.capabilities.padding.available = (padFlags & 0x01) != 0;
    description.capabilities.padding.forced = (padFlags & 0x02) != 0;
    
    // Padding value (if available)
    if (description.capabilities.padding.available) {
        uint16_t padValueRaw = param->data[6] | (param->data[7] << 8);
        description.capabilities.padding.value = static_cast<float>(padValueRaw) / 256.0f;
    } else {
        description.capabilities.padding.value = 0.0f;
    }
    
    return true;
}

bool AudioControl::getSupportedInputs(uint8_t channelIndex, std::vector<InputAvailability>& inputs) const {
    // Parameter code is channel_index + 2 to get supported inputs
    auto param = m_controller->getParameter(Category::Audio, channelIndex + 2);
    
    if (!param || param->data.size() < 2) {
        return false;
    }
    
    // Clear output vector
    inputs.clear();
    
    // Parse binary data for supported inputs
    // Each input is represented by a pair of values: input type and availability
    for (size_t i = 0; i < param->data.size(); i += 2) {
        if (i + 1 >= param->data.size()) break;
        
        InputAvailability input;
        uint8_t inputTypeValue = param->data[i];
        
        // Convert raw value to enum
        if (inputTypeValue < 14) {
            input.inputType = static_cast<InputType>(inputTypeValue);
            input.available = param->data[i + 1] != 0;
            inputs.push_back(input);
        }
    }
    
    return !inputs.empty();
}

bool AudioControl::setChannelLevel(uint8_t channelIndex, float gain, float normalizedValue) {
    // Parameter code for level is channel_index + 3
    uint8_t parameter = channelIndex + 3;
    
    // Convert gain to fixed16 format
    uint16_t gainFixed16 = static_cast<uint16_t>(gain * 256.0f);
    
    // If normalized value not specified, use gain as a base for normalization (0-1)
    if (normalizedValue < 0.0f) {
        normalizedValue = std::min(1.0f, std::max(0.0f, gain / 70.0f)); // Assuming 0-70dB range
    }
    
    uint16_t normalizedFixed16 = static_cast<uint16_t>(normalizedValue * 2048.0f);
    
    // Create payload (4 bytes: gain and normalized values as 16-bit integers)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(gainFixed16 & 0xFF),
        static_cast<uint8_t>((gainFixed16 >> 8) & 0xFF),
        static_cast<uint8_t>(normalizedFixed16 & 0xFF),
        static_cast<uint8_t>((normalizedFixed16 >> 8) & 0xFF)
    };
    
    return m_controller->sendCommand(
        Category::Audio,
        parameter,
        DataType::Fixed16,
        OperationType::Assign,
        payload
    );
}

bool AudioControl::getChannelLevel(uint8_t channelIndex, float& gain, float& normalizedValue) const {
    // Parameter code for level is channel_index + 3
    uint8_t parameter = channelIndex + 3;
    
    auto param = m_controller->getParameter(Category::Audio, parameter);
    
    if (!param || param->data.size() < 4) {
        return false;
    }
    
    // First 2 bytes: gain in fixed16 format
    uint16_t gainRaw = param->data[0] | (param->data[1] << 8);
    
    // Next 2 bytes: normalized value in fixed16 format
    uint16_t normalizedRaw = param->data[2] | (param->data[3] << 8);
    
    // Convert to float
    gain = static_cast<float>(gainRaw) / 256.0f;
    normalizedValue = static_cast<float>(normalizedRaw) / 2048.0f;
    
    return true;
}

bool AudioControl::setPhantomPower(uint8_t channelIndex, bool enabled) {
    // Parameter code for phantom power is channel_index + 4
    uint8_t parameter = channelIndex + 4;
    
    // Create boolean payload
    std::vector<uint8_t> payload = { enabled ? 0x01 : 0x00 };
    
    return m_controller->sendCommand(
        Category::Audio,
        parameter,
        DataType::Void,
        OperationType::Assign,
        payload
    );
}

bool AudioControl::getPhantomPower(uint8_t channelIndex, bool& enabled) const {
    // Parameter code for phantom power is channel_index + 4
    uint8_t parameter = channelIndex + 4;
    
    auto param = m_controller->getParameter(Category::Audio, parameter);
    
    if (!param) {
        return false;
    }
    
    enabled = param->toBoolean();
    return true;
}

bool AudioControl::setPadding(uint8_t channelIndex, bool enabled) {
    // Parameter code for padding is channel_index + 5
    uint8_t parameter = channelIndex + 5;
    
    // Create boolean payload
    std::vector<uint8_t> payload = { enabled ? 0x01 : 0x00 };
    
    return m_controller->sendCommand(
        Category::Audio,
        parameter,
        DataType::Void,
        OperationType::Assign,
        payload
    );
}

bool AudioControl::getPadding(uint8_t channelIndex, bool& enabled) const {
    // Parameter code for padding is channel_index + 5
    uint8_t parameter = channelIndex + 5;
    
    auto param = m_controller->getParameter(Category::Audio, parameter);
    
    if (!param) {
        return false;
    }
    
    enabled = param->toBoolean();
    return true;
}

bool AudioControl::setLowCutFilter(uint8_t channelIndex, bool enabled) {
    // Parameter code for low cut filter is channel_index + 6
    uint8_t parameter = channelIndex + 6;
    
    // Create boolean payload
    std::vector<uint8_t> payload = { enabled ? 0x01 : 0x00 };
    
    return m_controller->sendCommand(
        Category::Audio,
        parameter,
        DataType::Void,
        OperationType::Assign,
        payload
    );
}

bool AudioControl::getLowCutFilter(uint8_t channelIndex, bool& enabled) const {
    // Parameter code for low cut filter is channel_index + 6
    uint8_t parameter = channelIndex + 6;
    
    auto param = m_controller->getParameter(Category::Audio, parameter);
    
    if (!param) {
        return false;
    }
    
    enabled = param->toBoolean();
    return true;
}

bool AudioControl::getChannelAvailable(uint8_t channelIndex, bool& available) const {
    // Parameter code for availability is channel_index + 7
    uint8_t parameter = channelIndex + 7;
    
    auto param = m_controller->getParameter(Category::Audio, parameter);
    
    if (!param) {
        return false;
    }
    
    available = param->toBoolean();
    return true;
}

bool AudioControl::setMicLevel(float level) {
    // Validate level (0.0-1.0)
    if (level < 0.0f || level > 1.0f) {
        return false;
    }
    
    // Convert to fixed16 format
    uint16_t fixed16Value = static_cast<uint16_t>(level * 2048.0f);
    
    // Create payload (16-bit little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(fixed16Value & 0xFF),
        static_cast<uint8_t>((fixed16Value >> 8) & 0xFF)
    };
    
    return sendCommand(0x00, DataType::Fixed16, OperationType::Assign, payload);
}

bool AudioControl::getMicLevel(float& level) const {
    auto param = m_controller->getParameter(Category::Audio, 0x00);
    
    if (!param) {
        return false;
    }
    
    level = param->toFloat();
    return true;
}

bool AudioControl::setHeadphoneLevel(float level) {
    // Validate level (0.0-1.0)
    if (level < 0.0f || level > 1.0f) {
        return false;
    }
    
    // Convert to fixed16 format
    uint16_t fixed16Value = static_cast<uint16_t>(level * 2048.0f);
    
    // Create payload (16-bit little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(fixed16Value & 0xFF),
        static_cast<uint8_t>((fixed16Value >> 8) & 0xFF)
    };
    
    return sendCommand(0x01, DataType::Fixed16, OperationType::Assign, payload);
}

bool AudioControl::getHeadphoneLevel(float& level) const {
    auto param = m_controller->getParameter(Category::Audio, 0x01);
    
    if (!param) {
        return false;
    }
    
    level = param->toFloat();
    return true;
}

bool AudioControl::setHeadphoneProgramMix(float mix) {
    // Validate mix (0.0-1.0)
    if (mix < 0.0f || mix > 1.0f) {
        return false;
    }
    
    // Convert to fixed16 format
    uint16_t fixed16Value = static_cast<uint16_t>(mix * 2048.0f);
    
    // Create payload (16-bit little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(fixed16Value & 0xFF),
        static_cast<uint8_t>((fixed16Value >> 8) & 0xFF)
    };
    
    return sendCommand(0x02, DataType::Fixed16, OperationType::Assign, payload);
}

bool AudioControl::getHeadphoneProgramMix(float& mix) const {
    auto param = m_controller->getParameter(Category::Audio, 0x02);
    
    if (!param) {
        return false;
    }
    
    mix = param->toFloat();
    return true;
}

bool AudioControl::setSpeakerLevel(float level) {
    // Validate level (0.0-1.0)
    if (level < 0.0f || level > 1.0f) {
        return false;
    }
    
    // Convert to fixed16 format
    uint16_t fixed16Value = static_cast<uint16_t>(level * 2048.0f);
    
    // Create payload (16-bit little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(fixed16Value & 0xFF),
        static_cast<uint8_t>((fixed16Value >> 8) & 0xFF)
    };
    
    return sendCommand(0x03, DataType::Fixed16, OperationType::Assign, payload);
}

bool AudioControl::getSpeakerLevel(float& level) const {
    auto param = m_controller->getParameter(Category::Audio, 0x03);
    
    if (!param) {
        return false;
    }
    
    level = param->toFloat();
    return true;
}

bool AudioControl::sendCommand(
    uint8_t parameter,
    DataType dataType,
    OperationType operation,
    const std::vector<uint8_t>& payload
) {
    // Use the controller to send the command
    return m_controller->sendCommand(
        Category::Audio,
        parameter,
        dataType,
        operation,
        payload
    );
}

} // namespace BMDCamera
