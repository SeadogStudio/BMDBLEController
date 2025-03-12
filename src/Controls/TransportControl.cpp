// TransportControl.cpp
#include "TransportControl.h"
#include "../BMDBLEController.h"
#include "../Protocol/ProtocolUtils.h"

namespace BMDCamera {

TransportControl::TransportControl(BMDBLEController* controller)
    : m_controller(controller) {
    // Constructor implementation
}

bool TransportControl::setTransportMode(TransportMode mode) {
    // Create payload (single byte)
    std::vector<uint8_t> payload = { static_cast<uint8_t>(mode) };
    
    return sendCommand(0x00, DataType::SignedByte, OperationType::Assign, payload);
}

std::optional<TransportControl::TransportMode> TransportControl::getTransportMode() const {
    auto param = m_controller->getParameter(Category::Transport, 0x00);
    
    if (!param) {
        return std::nullopt;
    }
    
    int64_t value = param->toInteger();
    if (value < 0 || value > 2) {
        return std::nullopt;
    }
    
    return static_cast<TransportMode>(value);
}

bool TransportControl::getTransportState(TransportState& state) const {
    auto param = m_controller->getParameter(Category::Transport, 0x01);
    
    if (!param || param->data.size() < 5) {
        return false;
    }
    
    // Byte 0: Mode
    state.mode = static_cast<TransportMode>(param->data[0] & 0x03);
    
    // Byte 1: Speed (signed)
    int8_t speedValue = static_cast<int8_t>(param->data[1]);
    state.speed = static_cast<float>(speedValue);
    
    // Byte 2: Flags
    uint8_t flags = param->data[2];
    state.loop = (flags & 0x01) != 0;
    state.playAll = (flags & 0x02) != 0;
    state.disk1Active = (flags & 0x20) != 0;
    state.disk2Active = (flags & 0x40) != 0;
    state.timeLapseRecording = (flags & 0x80) != 0;
    
    // Byte 3-4: Storage medium
    state.slot1Medium = static_cast<TransportState::StorageMedium>(param->data[3] & 0x03);
    state.slot2Medium = static_cast<TransportState::StorageMedium>(param->data[4] & 0x03);
    
    return true;
}

bool TransportControl::setTransportState(const TransportState& state) {
    std::vector<uint8_t> payload(5, 0);
    
    // Byte 0: Mode
    payload[0] = static_cast<uint8_t>(state.mode);
    
    // Byte 1: Speed (signed)
    payload[1] = static_cast<uint8_t>(static_cast<int8_t>(state.speed));
    
    // Byte 2: Flags
    payload[2] = 
        (state.loop ? 0x01 : 0) |
        (state.playAll ? 0x02 : 0) |
        (state.disk1Active ? 0x20 : 0) |
        (state.disk2Active ? 0x40 : 0) |
        (state.timeLapseRecording ? 0x80 : 0);
    
    // Byte 3-4: Storage medium
    payload[3] = static_cast<uint8_t>(state.slot1Medium);
    payload[4] = static_cast<uint8_t>(state.slot2Medium);
    
    return sendCommand(0x01, DataType::SignedByte, OperationType::Assign, payload);
}

bool TransportControl::stop() {
    return m_controller->sendCommand(
        Category::Transport,
        0x02,
        DataType::Void,
        OperationType::Assign,
        {}
    );
}

bool TransportControl::play() {
    return m_controller->sendCommand(
        Category::Transport,
        0x03,
        DataType::Void,
        OperationType::Assign,
        {}
    );
}

bool TransportControl::record(const std::string& clipName) {
    std::vector<uint8_t> payload = { 0x01 }; // Set recording flag to true
    
    // Optionally add clip name if provided
    if (!clipName.empty()) {
        // Create clipName payload by appending name bytes
        payload.insert(payload.end(), clipName.begin(), clipName.end());
    }
    
    return m_controller->sendCommand(
        Category::Transport,
        0x04,
        DataType::Void,
        OperationType::Assign,
        payload
    );
}

bool TransportControl::isRecording() const {
    auto param = m_controller->getParameter(Category::Transport, 0x04);
    
    if (!param) {
        return false;
    }
    
    return param->toBoolean();
}

bool TransportControl::skipClip(PlaybackDirection direction) {
    std::vector<uint8_t> payload = { static_cast<uint8_t>(direction) };
    
    return sendCommand(0x00, DataType::SignedByte, OperationType::Assign, payload);
}

bool TransportControl::setPlaybackState(const PlaybackState& state) {
    std::vector<uint8_t> payload(12, 0);
    
    // Byte 0: Type
    payload[0] = static_cast<uint8_t>(state.type);
    
    // Byte 1: Loop
    payload[1] = state.loop ? 0x01 : 0x00;
    
    // Byte 2: Single clip
    payload[2] = state.singleClip ? 0x01 : 0x00;
    
    // Bytes 3-6: Speed (float32)
    // Convert float to IEEE 754 format
    union {
        float f;
        uint32_t i;
    } converter;
    converter.f = state.speed;
    
    payload[3] = static_cast<uint8_t>(converter.i & 0xFF);
    payload[4] = static_cast<uint8_t>((converter.i >> 8) & 0xFF);
    payload[5] = static_cast<uint8_t>((converter.i >> 16) & 0xFF);
    payload[6] = static_cast<uint8_t>((converter.i >> 24) & 0xFF);
    
    // Bytes 7-10: Position (int32)
    payload[7] = static_cast<uint8_t>(state.position & 0xFF);
    payload[8] = static_cast<uint8_t>((state.position >> 8) & 0xFF);
    payload[9] = static_cast<uint8_t>((state.position >> 16) & 0xFF);
    payload[10] = static_cast<uint8_t>((state.position >> 24) & 0xFF);
    
    return m_controller->sendCommand(
        Category::Transport,
        0x05,
        DataType::SignedByte,
        OperationType::Assign,
        payload
    );
}

std::optional<TransportControl::PlaybackState> TransportControl::getPlaybackState() const {
    auto param = m_controller->getParameter(Category::Transport, 0x05);
    
    if (!param || param->data.size() < 11) {
        return std::nullopt;
    }
    
    PlaybackState state;
    
    // Byte 0: Type
    state.type = static_cast<PlaybackType>(param->data[0]);
    
    // Byte 1: Loop
    state.loop = param->data[1] != 0;
    
    // Byte 2: Single clip
    state.singleClip = param->data[2] != 0;
    
    // Bytes 3-6: Speed (float32)
    union {
        uint32_t i;
        float f;
    } converter;
    converter.i = 
        static_cast<uint32_t>(param->data[3]) |
        (static_cast<uint32_t>(param->data[4]) << 8) |
        (static_cast<uint32_t>(param->data[5]) << 16) |
        (static_cast<uint32_t>(param->data[6]) << 24);
    state.speed = converter.f;
    
    // Bytes 7-10: Position (int32)
    state.position = 
        static_cast<int32_t>(param->data[7]) |
        (static_cast<int32_t>(param->data[8]) << 8) |
        (static_cast<int32_t>(param->data[9]) << 16) |
        (static_cast<int32_t>(param->data[10]) << 24);
    
    return state;
}

bool TransportControl::setStreamEnabled(bool enabled) {
    std::vector<uint8_t> payload = { enabled ? 0x01 : 0x00 };
    
    return sendCommand(0x05, DataType::Void, OperationType::Assign, payload);
}

bool TransportControl::isStreamEnabled() const {
    auto param = m_controller->getParameter(Category::Transport, 0x05);
    
    if (!param) {
        return false;
    }
    
    return param->toBoolean();
}

bool TransportControl::setStreamInfo(bool enabled) {
    std::vector<uint8_t> payload = { enabled ? 0x01 : 0x00 };
    
    return sendCommand(0x06, DataType::Void, OperationType::Assign, payload);
}

bool TransportControl::getStreamInfo(bool& enabled) const {
    auto param = m_controller->getParameter(Category::Transport, 0x06);
    
    if (!param) {
        return false;
    }
    
    enabled = param->toBoolean();
    return true;
}

bool TransportControl::setStreamDisplay3DLUT(bool enabled) {
    std::vector<uint8_t> payload = { enabled ? 0x01 : 0x00 };
    
    return sendCommand(0x07, DataType::Void, OperationType::Assign, payload);
}

bool TransportControl::getStreamDisplay3DLUT(bool& enabled) const {
    auto param = m_controller->getParameter(Category::Transport, 0x07);
    
    if (!param) {
        return false;
    }
    
    enabled = param->toBoolean();
    return true;
}

bool TransportControl::setCodecFormat(const CodecFormat& format) {
    std::vector<uint8_t> payload(2, 0);
    
    // Byte 0: Codec
    payload[0] = static_cast<uint8_t>(format.codec);
    
    // Byte 1: Variant
    switch (format.codec) {
        case CodecType::CinemaDNG:
            payload[1] = static_cast<uint8_t>(format.cinemaDNG.variant);
            break;
        case CodecType::ProRes:
            payload[1] = static_cast<uint8_t>(format.prores.variant);
            break;
        case CodecType::BlackmagicRAW:
            payload[1] = static_cast<uint8_t>(format.braw.variant);
            break;
        default:
            payload[1] = 0; // No variant for DNxHD
            break;
    }
    
    return sendCommand(0x00, DataType::SignedByte, OperationType::Assign, payload);
}

std::optional<TransportControl::CodecFormat> TransportControl::getCodecFormat() const {
    auto param = m_controller->getParameter(Category::Transport, 0x00);
    
    if (!param || param->data.size() < 2) {
        return std::nullopt;
    }
    
    CodecFormat format;
    
    // Byte 0: Codec
    format.codec = static_cast<CodecType>(param->data[0]);
    
    // Byte 1: Variant
    switch (format.codec) {
        case CodecType::CinemaDNG:
            format.cinemaDNG.variant = static_cast<CodecFormat::cinemaDNG::Variant>(param->data[1]);
            break;
        case CodecType::ProRes:
            format.prores.variant = static_cast<CodecFormat::prores::Variant>(param->data[1]);
            break;
        case CodecType::BlackmagicRAW:
            format.braw.variant = static_cast<CodecFormat::braw::Variant>(param->data[1]);
            break;
    }
    
    return format;
}

bool TransportControl::setTimecodeSource(TimecodeSource source) {
    std::vector<uint8_t> payload = { static_cast<uint8_t>(source) };
    
    return sendCommand(0x07, DataType::SignedByte, OperationType::Assign, payload);
}

std::optional<TransportControl::TimecodeSource> TransportControl::getTimecodeSource() const {
    auto param = m_controller->getParameter(Category::Transport, 0x07);
    
    if (!param) {
        return std::nullopt;
    }
    
    int64_t value = param->toInteger();
    if (value < 0 || value > 1) {
        return std::nullopt;
    }
    
    return static_cast<TimecodeSource>(value);
}

bool TransportControl::getClipFilename(std::string& filename) const {
    // Clip filename is in Extended Lens category, parameter 0x0F
    auto param = m_controller->getParameter(Category::ExtendedLens, 0x0F);
    
    if (!param) {
        return false;
    }
    
    filename = param->toString();
    return true;
}

bool TransportControl::sendCommand(
    uint8_t parameter,
    DataType dataType,
    OperationType operation,
    const std::vector<uint8_t>& payload
) {
    // Use the controller to send the command
    return m_controller->sendCommand(
        Category::Transport,
        parameter,
        dataType,
        operation,
        payload
    );
}

} // namespace BMDCamera
