#ifndef BMD_LENS_CONTROL_H
#define BMD_LENS_CONTROL_H

#include <cstdint>
#include <string>
#include "../Protocol/ProtocolConstants.h"

namespace BMDCamera {

class BMDBLEController; // Forward declaration

class LensControl {
public:
    // Constructor requires parent controller
    explicit LensControl(BMDBLEController* controller);

    // Focus control - normalized and raw values
    bool setFocus(float normalizedValue);
    bool setFocusRaw(uint16_t rawValue);
    bool getFocus(float& normalizedValue) const;
    bool getFocusRaw(uint16_t& rawValue) const;
    
    // Auto focus trigger
    bool triggerAutoFocus();
    
    // Aperture control (multiple representations)
    bool setAperture(float fStopValue);               // Set using f-stop value (f2.8, f4, etc.)
    bool setApertureNormalized(float normalizedValue); // Set using normalized 0.0-1.0 value
    bool setApertureOrdinal(uint8_t ordinalValue);    // Set using lens-specific ordinal steps
    
    bool getAperture(float& fStopValue) const;
    bool getApertureNormalized(float& normalizedValue) const;
    bool getApertureOrdinal(uint8_t& ordinalValue) const;
    
    // Auto aperture trigger
    bool triggerAutoAperture();
    
    // Optical image stabilization
    bool setOpticalImageStabilization(bool enabled);
    bool getOpticalImageStabilization(bool& enabled) const;
    
    // Zoom control
    bool setZoomAbsolute(uint16_t focalLengthMm);     // Set zoom by focal length in mm
    bool setZoomNormalized(float normalizedValue);     // Set zoom by normalized 0.0-1.0 value
    bool setZoomContinuous(float speed);               // Start continuous zoom (-1.0 to +1.0)
    
    bool getZoomAbsolute(uint16_t& focalLengthMm) const;
    bool getZoomNormalized(float& normalizedValue) const;
    
    // Lens info (read-only)
    bool getLensModel(std::string& model) const;
    bool getFocalLength(std::string& focalLength) const;
    bool getFocusDistance(std::string& distance) const;
    
private:
    // Parent controller reference
    BMDBLEController* m_controller;
    
    // Utility methods
    bool sendCommand(
        uint8_t parameter,
        DataType dataType,
        OperationType operation,
        const std::vector<uint8_t>& payload
    );
    
    // Conversion utilities
    static float normalizedToFStop(float normalizedValue);
    static float fStopToNormalized(float fStopValue);
    static uint16_t floatToFixed16(float value);
    static float fixed16ToFloat(uint16_t value);
};

} // namespace BMDCamera

#endif // BMD_LENS_CONTROL_H
