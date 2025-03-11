#include "LensControl.h"
#include "../BMDBLEController.h"
#include "../Protocol/ProtocolUtils.h"
#include <cmath>

namespace BMDCamera {

LensControl::LensControl(BMDBLEController* controller) 
    : m_controller(controller) {
    // Constructor implementation
}

bool LensControl::setFocus(float normalizedValue) {
    // Validate normalized value
    if (normalizedValue < 0.0f || normalizedValue > 1.0f) {
        return false;
    }
    
    // Convert normalized value to fixed16 format
    uint16_t fixed16Value = floatToFixed16(normalizedValue);
    
    // Create payload (16-bit little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(fixed16Value & 0xFF),
        static_cast<uint8_t>((fixed16Value >> 8) & 0xFF)
    };
    
    // Send focus command
    return sendCommand(0x00, DataType::Fixed16, OperationType::Assign, payload);
}

bool LensControl::setFocusRaw(uint16_t rawValue) {
    // rawValue is already in fixed16 format (0-2048)
    // Create payload (16-bit little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(rawValue & 0xFF),
        static_cast<uint8_t>((rawValue >> 8) & 0xFF)
    };
    
    // Send focus command
    return sendCommand(0x00, DataType::Fixed16, OperationType::Assign, payload);
}

bool LensControl::getFocus(float& normalizedValue) const {
    // Get parameter from controller
    auto param = m_controller->getParameter(Category::Lens, 0x00);
    
    if (!param) {
        return false;
    }
    
    // Convert to float
    normalizedValue = param->toFloat();
    return true;
}

bool LensControl::getFocusRaw(uint16_t& rawValue) const {
    // Get parameter from controller
    auto param = m_controller->getParameter(Category::Lens, 0x00);
    
    if (!param) {
        return false;
    }
    
    // Convert to integer and truncate to 16-bit
    rawValue = static_cast<uint16_t>(param->toInteger() & 0xFFFF);
    return true;
}

bool LensControl::triggerAutoFocus() {
    // Auto focus is triggered with an empty (void) command
    return sendCommand(0x01, DataType::Void, OperationType::Assign, {});
}

bool LensControl::setAperture(float fStopValue) {
    // Validate f-stop value (typical range f1.0 to f22)
    if (fStopValue < 1.0f || fStopValue > 22.0f) {
        return false;
    }
    
    // Convert f-stop to normalized value
    float normalizedValue = fStopToNormalized(fStopValue);
    
    // Set aperture using normalized value
    return setApertureNormalized(normalizedValue);
}

bool LensControl::setApertureNormalized(float normalizedValue) {
    // Validate normalized value
    if (normalizedValue < 0.0f || normalizedValue > 1.0f) {
        return false;
    }
    
    // Convert to fixed16 format
    uint16_t fixed16Value = floatToFixed16(normalizedValue);
    
    // Create payload (16-bit little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(fixed16Value & 0xFF),
        static_cast<uint8_t>((fixed16Value >> 8) & 0xFF)
    };
    
    // Send aperture command (parameter 0x03 is normalized aperture)
    return sendCommand(0x03, DataType::Fixed16, OperationType::Assign, payload);
}

bool LensControl::setApertureOrdinal(uint8_t ordinalValue) {
    // Ordinal aperture uses 16-bit integer type
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(ordinalValue),
        0x00  // High byte is 0 for small values
    };
    
    // Send aperture command (parameter 0x04 is ordinal aperture)
    return sendCommand(0x04, DataType::SignedInt16, OperationType::Assign, payload);
}

bool LensControl::getAperture(float& fStopValue) const {
    // Get normalized aperture
    float normalizedValue;
    if (!getApertureNormalized(normalizedValue)) {
        return false;
    }
    
    // Convert normalized to f-stop
    fStopValue = normalizedToFStop(normalizedValue);
    return true;
}

bool LensControl::getApertureNormalized(float& normalizedValue) const {
    // Get parameter from controller (parameter 0x03 is normalized aperture)
    auto param = m_controller->getParameter(Category::Lens, 0x03);
    
    if (!param) {
        return false;
    }
    
    // Convert to float
    normalizedValue = param->toFloat();
    return true;
}

bool LensControl::getApertureOrdinal(uint8_t& ordinalValue) const {
    // Get parameter from controller (parameter 0x04 is ordinal aperture)
    auto param = m_controller->getParameter(Category::Lens, 0x04);
    
    if (!param) {
        return false;
    }
    
    // Convert to integer and ensure it fits in uint8_t
    int64_t value = param->toInteger();
    if (value < 0 || value > 255) {
        return false;
    }
    
    ordinalValue = static_cast<uint8_t>(value);
    return true;
}

bool LensControl::triggerAutoAperture() {
    // Auto aperture is triggered with an empty (void) command
    return sendCommand(0x05, DataType::Void, OperationType::Assign, {});
}

bool LensControl::setOpticalImageStabilization(bool enabled) {
    // Create boolean payload
    std::vector<uint8_t> payload = { enabled ? 0x01 : 0x00 };
    
    // Send OIS command (parameter 0x06)
    return sendCommand(0x06, DataType::Void, OperationType::Assign, payload);
}

bool LensControl::getOpticalImageStabilization(bool& enabled) const {
    // Get parameter from controller
    auto param = m_controller->getParameter(Category::Lens, 0x06);
    
    if (!param) {
        return false;
    }
    
    // Convert to boolean
    enabled = param->toBoolean();
    return true;
}

bool LensControl::setZoomAbsolute(uint16_t focalLengthMm) {
    // Create payload (16-bit little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(focalLengthMm & 0xFF),
        static_cast<uint8_t>((focalLengthMm >> 8) & 0xFF)
    };
    
    // Send zoom command (parameter 0x07 is absolute zoom in mm)
    return sendCommand(0x07, DataType::SignedInt16, OperationType::Assign, payload);
}

bool LensControl::setZoomNormalized(float normalizedValue) {
    // Validate normalized value
    if (normalizedValue < 0.0f || normalizedValue > 1.0f) {
        return false;
    }
    
    // Convert to fixed16 format
    uint16_t fixed16Value = floatToFixed16(normalizedValue);
    
    // Create payload (16-bit little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(fixed16Value & 0xFF),
        static_cast<uint8_t>((fixed16Value >> 8) & 0xFF)
    };
    
    // Send zoom command (parameter 0x08 is normalized zoom)
    return sendCommand(0x08, DataType::Fixed16, OperationType::Assign, payload);
}

bool LensControl::setZoomContinuous(float speed) {
    // Validate speed (-1.0 to +1.0)
    if (speed < -1.0f || speed > 1.0f) {
        return false;
    }
    
    // Convert to fixed16 format
    uint16_t fixed16Value = floatToFixed16(speed);
    
    // Create payload (16-bit little-endian)
    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(fixed16Value & 0xFF),
        static_cast<uint8_t>((fixed16Value >> 8) & 0xFF)
    };
    
    // Send zoom command (parameter 0x09 is continuous zoom)
    return sendCommand(0x09, DataType::Fixed16, OperationType::Assign, payload);
}

bool LensControl::getZoomAbsolute(uint16_t& focalLengthMm) const {
    // Get parameter from controller
    auto param = m_controller->getParameter(Category::Lens, 0x07);
    
    if (!param) {
        return false;
    }
    
    // Convert to integer and ensure it fits in uint16_t
    int64_t value = param->toInteger();
    if (value < 0 || value > UINT16_MAX) {
        return false;
    }
    
    focalLengthMm = static_cast<uint16_t>(value);
    return true;
}

bool LensControl::getZoomNormalized(float& normalizedValue) const {
    // Get parameter from controller
    auto param = m_controller->getParameter(Category::Lens, 0x08);
    
    if (!param) {
        return false;
    }
    
    // Convert to float
    normalizedValue = param->toFloat();
    return true;
}

bool LensControl::getLensModel(std::string& model) const {
    // Get parameter from controller (ExtendedLens category, parameter 0x09)
    auto param = m_controller->getParameter(Category::ExtendedLens, 0x09);
    
    if (!param) {
        return false;
    }
    
    // Convert to string
    model = param->toString();
    return true;
}

bool LensControl::getFocalLength(std::string& focalLength) const {
    // Get parameter from controller (ExtendedLens category, parameter 0x0B)
    auto param = m_controller->getParameter(Category::ExtendedLens, 0x0B);
    
    if (!param) {
        return false;
    }
    
    // Convert to string
    focalLength = param->toString();
    return true;
}

bool LensControl::getFocusDistance(std::string& distance) const {
    // Get parameter from controller (ExtendedLens category, parameter 0x0C)
    auto param = m_controller->getParameter(Category::ExtendedLens, 0x0C);
    
    if (!param) {
        return false;
    }
    
    // Convert to string
    distance = param->toString();
    return true;
}

bool LensControl::sendCommand(
    uint8_t parameter,
    DataType dataType,
    OperationType operation,
    const std::vector<uint8_t>& payload
) {
    // Use the controller to send the command
    return m_controller->sendCommand(
        Category::Lens,
        parameter,
        dataType,
        operation,
        payload
    );
}

float LensControl::normalizedToFStop(float normalizedValue) {
    // This is an approximation based on typical lens behavior
    // F-stop follows a geometric progression
    // normalizedValue 0.0 = minimum f-stop (widest aperture)
    // normalizedValue 1.0 = maximum f-stop (narrowest aperture)
    
    const float minFStop = 1.8f;  // Typical minimum f-stop
    const float maxFStop = 22.0f; // Typical maximum f-stop
    const float steps = 8.0f;     // Typical number of full stops
    
    // Calculate f-stop value using geometric progression
    return minFStop * std::pow(maxFStop / minFStop, normalizedValue);
}

float LensControl::fStopToNormalized(float fStopValue) {
    // Inverse of normalizedToFStop
    const float minFStop = 1.8f;
    const float maxFStop = 22.0f;
    
    // Calculate normalized value
    float normalized = std::log(fStopValue / minFStop) / std::log(maxFStop / minFStop);
    
    // Clamp to 0.0-1.0 range
    return std::max(0.0f, std::min(1.0f, normalized));
}

uint16_t LensControl::floatToFixed16(float value) {
    // Convert normalized float (typically 0.0-1.0) to fixed16 format
    // Fixed16 format: 5 bits integer, 11 bits fractional
    return static_cast<uint16_t>(value * 2048.0f);
}

float LensControl::fixed16ToFloat(uint16_t value) {
    // Convert fixed16 to float
    return static_cast<float>(value) / 2048.0f;
}

} // namespace BMDCamera
