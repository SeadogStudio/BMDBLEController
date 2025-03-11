#ifndef BMD_VIDEO_CONTROL_H
#define BMD_VIDEO_CONTROL_H

#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include "../Protocol/ProtocolConstants.h"

namespace BMDCamera {

class BMDBLEController; // Forward declaration

class VideoControl {
public:
    // Constructor requires parent controller
    explicit VideoControl(BMDBLEController* controller);
    
    // Video mode settings
    struct VideoMode {
        uint8_t frameRate;
        bool isMRate;
        uint8_t dimensions;
        bool isInterlaced;
        uint8_t colorSpace;
    };
    
    bool setVideoMode(const VideoMode& mode);
    std::optional<VideoMode> getVideoMode() const;
    
    // White balance control
    bool setWhiteBalance(uint16_t kelvin, int16_t tint = 0);
    bool getWhiteBalance(uint16_t& kelvin, int16_t& tint) const;
    bool triggerAutoWhiteBalance();
    bool restoreAutoWhiteBalance();
    
    // Exposure control
    bool setExposure(uint32_t microseconds);
    bool getExposure(uint32_t& microseconds) const;
    bool setExposureOrdinal(uint16_t ordinalValue);
    bool getExposureOrdinal(uint16_t& ordinalValue) const;
    
    // Dynamic range mode
    enum class DynamicRangeMode : uint8_t {
        Film = 0,
        Video = 1,
        ExtendedVideo = 2
    };
    
    bool setDynamicRangeMode(DynamicRangeMode mode);
    std::optional<DynamicRangeMode> getDynamicRangeMode() const;
    
    // Sharpening level
    enum class SharpeningLevel : uint8_t {
        Off = 0,
        Low = 1,
        Medium = 2,
        High = 3
    };
    
    bool setSharpeningLevel(SharpeningLevel level);
    std::optional<SharpeningLevel> getSharpeningLevel() const;
    
    // Recording format
    struct RecordingFormat {
        uint16_t fileFrameRate;
        uint16_t sensorFrameRate;
        uint16_t frameWidth;
        uint16_t frameHeight;
        bool isFileMRate;
        bool isSensorMRate;
        bool isSensorOffSpeed;
        bool isInterlaced;
        bool isWindowed;
    };
    
    bool setRecordingFormat(const RecordingFormat& format);
    std::optional<RecordingFormat> getRecordingFormat() const;
    
    // Auto exposure mode
    enum class AutoExposureMode : uint8_t {
        Manual = 0,
        Iris = 1,
        Shutter = 2,
        IrisShutter = 3,
        ShutterIris = 4
    };
    
    bool setAutoExposureMode(AutoExposureMode mode);
    std::optional<AutoExposureMode> getAutoExposureMode() const;
    
    // Shutter angle/speed (two representations of the same setting)
    bool setShutterAngle(uint32_t angleHundredths);  // Angle in 1/100 degrees (e.g. 18000 = 180°)
    bool getShutterAngle(uint32_t& angleHundredths) const;
    
    bool setShutterSpeed(uint32_t speed);  // Denominator of 1/x second (e.g. 48 = 1/48 sec)
    bool getShutterSpeed(uint32_t& speed) const;
    
    // ISO and gain (two representations of the same setting)
    bool setISO(uint32_t iso);
    bool getISO(uint32_t& iso) const;
    
    bool setGain(int8_t gainDB);
    bool getGain(int8_t& gainDB) const;
    
    // ND filter
    bool setNDFilter(float stop);
    bool getNDFilter(float& stop) const;
    
    enum class NDFilterDisplayMode : uint8_t {
        Stop = 0,
        Number = 1,
        Fraction = 2
    };
    
    bool setNDFilterDisplayMode(NDFilterDisplayMode mode);
    std::optional<NDFilterDisplayMode> getNDFilterDisplayMode() const;
    
    // Display LUT
    enum class DisplayLUT : uint8_t {
        None = 0,
        Custom = 1,
        FilmToVideo = 2,
        FilmToExtendedVideo = 3
    };
    
    struct LUTSettings {
        DisplayLUT selectedLUT;
        bool enabled;
    };
    
    bool setDisplayLUT(const LUTSettings& settings);
    std::optional<LUTSettings> getDisplayLUT() const;
    
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

#endif // BMD_VIDEO_CONTROL_H
