// TransportControl.h
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
    
    // Transport mode
    enum class TransportMode : uint8_t {
        Preview = 0,
        Play = 1,
        Record = 2
    };
    
    // Transport state
    struct TransportState {
        TransportMode mode;
        float speed; // -ve = reverse, 0 = pause, +ve = forward
        
        // Flags
        bool loop;
        bool playAll;
        bool disk1Active;
        bool disk2Active;
        bool timeLapseRecording;
        
        // Storage medium types
        enum class StorageMedium : uint8_t {
            CFast = 0,
            SD = 1,
            SSDRecorder = 2
        };
        
        StorageMedium slot1Medium;
        StorageMedium slot2Medium;
    };
    
    // Basic transport control
    bool setTransportMode(TransportMode mode);
    std::optional<TransportMode> getTransportMode() const;
    
    bool getTransportState(TransportState& state) const;
    bool setTransportState(const TransportState& state);
    
    // Simple control methods
    bool stop();
    bool play();
    bool record(const std::string& clipName = "");
    bool isRecording() const;
    
    // Playback control
    enum class PlaybackDirection : uint8_t {
        Previous = 0,
        Next = 1
    };
    
    bool skipClip(PlaybackDirection direction);
    
    // Playback state
    enum class PlaybackType : uint8_t {
        Play = 0,
        Jog = 1,
        Shuttle = 2,
        Var = 3
    };
    
    struct PlaybackState {
        PlaybackType type;
        bool loop;
        bool singleClip;
        float speed;
        int32_t position; // frame position
    };
    
    bool setPlaybackState(const PlaybackState& state);
    std::optional<PlaybackState> getPlaybackState() const;
    
    // Streaming control
    bool setStreamEnabled(bool enabled);
    bool isStreamEnabled() const;
    
    bool setStreamInfo(bool enabled);
    bool getStreamInfo(bool& enabled) const;
    
    bool setStreamDisplay3DLUT(bool enabled);
    bool getStreamDisplay3DLUT(bool& enabled) const;
    
    // Codec settings
    enum class CodecType : uint8_t {
        CinemaDNG = 0,
        DNxHD = 1,
        ProRes = 2,
        BlackmagicRAW = 3
    };
    
    struct CodecFormat {
        CodecType codec;
        
        // Variant depends on codec type
        union {
            struct {
                enum class Variant : uint8_t {
                    Uncompressed = 0,
                    Lossy3to1 = 1,
                    Lossy4to1 = 2
                } variant;
            } cinemaDNG;
            
            struct {
                enum class Variant : uint8_t {
                    HQ = 0,
                    Std422 = 1,
                    LT = 2,
                    Proxy = 3,
                    Std444 = 4,
                    XQ444 = 5
                } variant;
            } prores;
            
            struct {
                enum class Variant : uint8_t {
                    Q0 = 0,
                    Q5 = 1,
                    Ratio3to1 = 2,
                    Ratio5to1 = 3,
                    Ratio8to1 = 4,
                    Ratio12to1 = 5
                } variant;
            } braw;
        };
    };
    
    bool setCodecFormat(const CodecFormat& format);
    std::optional<CodecFormat> getCodecFormat() const;
    
    // Timecode source
    enum class TimecodeSource : uint8_t {
        Timecode = 0,
        Clip = 1
    };
    
    bool setTimecodeSource(TimecodeSource source);
    std::optional<TimecodeSource> getTimecodeSource() const;
    
    // Get current clip information
    bool getClipFilename(std::string& filename) const;
    
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
