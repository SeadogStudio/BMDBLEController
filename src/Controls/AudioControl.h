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

    // Input Types
    enum class InputType : uint8_t {
        None = 0,
        InternalMic = 1,
        LineLevel = 2,
        LowMicLevel = 3,
        HighMicLevel = 4
    };

    // Audio Channel Control
    bool setChannelInput(uint8_t channelIndex, InputType inputType);
    std::optional<InputType> getChannelInput(uint8_t channelIndex) const;

    // Gain/Level Controls
    bool setChannelLevel(uint8_t channelIndex, float normalizedLevel);
    std::optional<float> getChannelLevel(uint8_t channelIndex) const;

    // Phantom Power
    bool setPhantomPower(uint8_t channelIndex, bool enabled);
    std::optional<bool> getPhantomPower(uint8_t channelIndex) const;

    // Headphone/Speaker Level Controls
    bool setHeadphoneLevel(float normalizedLevel);
    std::optional<float> getHeadphoneLevel() const;

    bool setHeadphoneProgramMix(float normalizedLevel);
    std::optional<float> getHeadphoneProgramMix() const;

    bool setSpeakerLevel(float normalizedLevel);
    std::optional<float> getSpeakerLevel() const;

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
