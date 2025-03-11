#include "BMDBLEController.h"
#include "Protocol/ProtocolUtils.h"
#include "Input/SerialPinInputMethod.h"
#include <BLEDevice.h>

namespace BMDCamera {

BMDBLEController::BMDBLEController(const std::string& deviceName)
    : m_deviceName(deviceName) {
    
    // Initialize BLE device with provided name
    BLEDevice::init(deviceName.c_str());
    BLEDevice::setPower(ESP_PWR_LVL_P9); // Maximum power for best range
    
    // Create callback manager
    m_callbackManager = std::make_unique<CallbackManager>();
    
    // Create incoming control manager
    m_incomingControlManager = std::make_unique<IncomingCameraControlManager>(m_callbackManager.get());
    
    // Create connection manager with default serial PIN input method
    m_connectionManager = std::make_unique<BLEConnectionManager>(
        m_callbackManager.get(),
        createPinInputMethod<SerialPinInputMethod>()
    );
    
    // Initialize control modules
    initializeControlModules();
}

BMDBLEController::~BMDBLEController() {
    // Disconnect if connected
    if (isConnected()) {
        disconnect();
    }
}

bool BMDBLEController::startScan(uint32_t duration) {
    return m_connectionManager->startScan(duration);
}

bool BMDBLEController::connect() {
    return m_connectionManager->connect();
}

bool BMDBLEController::connectToSavedCamera() {
    return m_connectionManager->connectToSavedCamera();
}

void BMDBLEController::disconnect() {
    m_connectionManager->disconnect();
}

bool BMDBLEController::isConnected() const {
    return m_connectionManager->isConnected();
}

bool BMDBLEController::isBonded() const {
    // The concept of being "bonded" is tied to having a saved camera address
    return !m_connectionManager->getCurrentCameraAddress().empty();
}

void BMDBLEController::setConnectionCallback(ConnectionCallback callback) {
    m_callbackManager->setConnectionCallback(std::move(callback));
}

void BMDBLEController::setParameterUpdateCallback(ParameterUpdateCallback callback) {
    m_callbackManager->setParameterUpdateCallback(std::move(callback));
}

void BMDBLEController::setStatusUpdateCallback(StatusUpdateCallback callback) {
    m_callbackManager->setStatusUpdateCallback(std::move(callback));
}

void BMDBLEController::setErrorCallback(ErrorCallback callback) {
    m_callbackManager->setErrorCallback(std::move(callback));
}

void BMDBLEController::setPinInputMethod(PinInputMethodPtr pinInputMethod) {
    m_connectionManager->setPinInputMethod(std::move(pinInputMethod));
}

LensControl& BMDBLEController::getLensControl() {
    return *m_lensControl;
}

VideoControl& BMDBLEController::getVideoControl() {
    return *m_videoControl;
}

AudioControl& BMDBLEController::getAudioControl() {
    return *m_audioControl;
}

TransportControl& BMDBLEController::getTransportControl() {
    return *m_transportControl;
}

std::optional<IncomingCameraControlManager::ParameterData> 
BMDBLEController::getParameter(Category category, uint8_t parameter) const {
    return m_incomingControlManager->getParameter(category, parameter);
}

void BMDBLEController::clearBondingInformation() {
    m_connectionManager->disconnect();
    // There should be a method in connection manager to clear bonding info
    // For now, we'll just disconnect
}

std::string BMDBLEController::getStatusString() const {
    if (!m_connectionManager) {
        return "Not initialized";
    }
    
    if (isConnected()) {
        return "Connected to " + m_connectionManager->getCurrentCameraAddress();
    } else if (isBonded()) {
        return "Bonded but disconnected";
    } else {
        return "Not connected";
    }
}

bool BMDBLEController::sendCommand(
    Category category,
    uint8_t parameter,
    DataType dataType,
    OperationType operation,
    const std::vector<uint8_t>& payload
) {
    if (!isConnected()) {
        m_callbackManager->notifyError("Cannot send command: not connected");
        return false;
    }
    
    // Create command packet
    std::vector<uint8_t> packet = ProtocolUtils::createCommandPacket(
        category, parameter, dataType, operation, payload
    );
    
    // The sendCommand would be implemented in control modules
    // This is a placeholder for direct command sending
    // In a complete implementation, this would delegate to the appropriate module
    
    return true;
}

void BMDBLEController::handleIncomingData(const uint8_t* data, size_t length) {
    m_incomingControlManager->processIncomingPacket(data, length);
}

void BMDBLEController::initializeControlModules() {
    // Initialize control modules with this controller as parent
    m_lensControl = std::make_unique<LensControl>(this);
    m_videoControl = std::make_unique<VideoControl>(this);
    m_audioControl = std::make_unique<AudioControl>(this);
    m_transportControl = std::make_unique<TransportControl>(this);
}

} // namespace BMDCamera
