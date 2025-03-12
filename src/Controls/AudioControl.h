// AudioControl.h
#ifndef BMD_AUDIO_CONTROL_H
#define BMD_AUDIO_CONTROL_H

#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include "../Protocol/ProtocolConstants.h"

namespace BMDCamera {

class BMDBLEController; // Forward declaration

class AudioControl {
public:
    // Constructor requires parent controller
    explicit AudioControl(BMDBLEController* controller);
    
    // Input type enumeration
    enum class InputType : uint8_t {
        None = 0,
        CameraLeft = 1,
        CameraRight = 2,
        CameraMono = 3,
        XLR1Mic = 4,
        XLR1Line = 5,
        XLR2Mic = 6,
        XLR2Line = 7,
        Line3_5mmLeft = 8,
        Mic3_5mmLeft = 9,
        Line3_5mmRight = 10,
        Mic3_5mmRight = 11,
        Line3_5mmMono = 12,
        Mic3_5mmMono = 13
    };
    
    // Channel input settings
    bool setChannelInput(uint8_t channelIndex, InputType inputType);
    std::optional<InputType> getChannelInput(uint8_t channelIndex) const;
    
    // Channel input description
    struct InputDescription {
        struct GainRange {
            float min;
            float max;
        };
        
        struct Capabilities {
            bool supportsPhantomPower;
            bool supportsLowCutFilter;
            
            struct Padding {
                bool available;
                bool forced;
                float value;
            } padding;
        };
        
        GainRange gainRange;
        Capabilities capabilities;
    };
    
    bool getInputDescription(uint8_t channelIndex, InputDescription& description) const;
    
    // Get available inputs for a channel
    struct InputAvailability {
        InputType inputType;
        bool available;
    };
    
    bool getSupportedInputs(uint8_t channelIndex, std::vector<InputAvailability>& inputs) const;
    
    // Level control
    bool setChannelLevel(uint8_t channelIndex, float gain, float normalizedValue = -1.0f);
    bool getChannelLevel(uint8_t channelIndex, float& gain, float& normalizedValue) const;
    
    // Phantom power
    bool setPhantomPower(uint8_t channelIndex, bool enabled);
    bool getPhantomPower(uint8_t channelIndex, bool& enabled) const;
    
    // Padding
    bool setPadding(uint8_t channelIndex, bool enabled);
    bool getPadding(uint8_t channelIndex, bool& enabled) const;
    
    // Low cut filter
    bool setLowCutFilter(uint8_t channelIndex, bool enabled);
    bool getLowCutFilter(uint8_t channelIndex, bool& enabled) const;
    
    // Availability
    bool getChannelAvailable(uint8_t channelIndex, bool& available) const;
    
    // Global audio levels
    bool setMicLevel(float level);
    bool getMicLevel(float& level) const;
    
    bool setHeadphoneLevel(float level);
    bool getHeadphoneLevel(float& level) const;
    
    bool setHeadphoneProgramMix(float mix);
    bool getHeadphoneProgramMix(float& mix) const;
    
    bool setSpeakerLevel(float level);
    bool getSpeakerLevel(float& level) const;
    
private:
    // Parent controller reference
    BMDBLEController* m_controller;
    
    // Utility method to send commands
    bool sendCommand(
        uint8_t parameter,
        DataType dataType,
        OperationType operation,
        const std::vector<uint8_t>& payload
    );
};

} // namespace BMDCamera

#endif // BMD_AUDIO_CONTROL_H
