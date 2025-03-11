#ifndef BMD_TRANSPORT_CONTROL_H
#define BMD_TRANSPORT_CONTROL_H

#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include "../Protocol/ProtocolConstants.h"

namespace BMDCamera {

class BMDBLEController; // Forward declaration

class TransportControl {
public:
    // Constructor requires parent controller
    explicit TransportControl(BMDBLEController* controller);

    // Transport Modes
    enum class TransportMode : uint8_t {
        InputPreview = 0,
        InputRecord = 1,
        Output = 2
    };

    // Playback Types
    enum class PlaybackType : uint8_t {
        Play = 0,
        Jog = 1,
        Shuttle = 2,
        Variable = 3
    };

    // Transport Control Methods
    bool setTransportMode(TransportMode mode);
    std::optional<TransportMode> getTransportMode() const;

    bool startRecording(const std::string& clipName = "");
    bool stopRecording();
    bool isRecording() const;

    // Playback Controls
    bool setPlaybackState(
        PlaybackType type, 
        bool loop = false, 
        bool singleClip = false, 
        float speed = 1.0f, 
        int32_t position = 0
    );

    struct PlaybackState {
        PlaybackType type;
        bool loop;
        bool singleClip;
        float speed;
        int32_t position;
    };

    std::optional<PlaybackState> getPlaybackState() const;

    // Timecode Retrieval
    std::optional<std::string> getCurrentTimecode() const;
    std::optional<std::string> getCurrentClipTimecode() const;

    // Specific Transport Actions
    bool gotoHome();
    bool resetHome();
    bool nextClip();
    bool previousClip();

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

#endif // BMD_TRANSPORT_CONTROL_H
