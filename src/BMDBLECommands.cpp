#include "BMDBLEController.h"

// Set focus with raw value (0-2048)
bool BMDBLEController::setFocus(uint16_t rawValue) {
  if (!isConnected() || !_pOutgoingCameraControl) {
    return false;
  }
  
  // Clamp value to 0-2048
  if (rawValue > 2048) rawValue = 2048;
  
  // Create focus command
  uint8_t focusCommand[12] = {
    0xFF,                    // Protocol identifier
    0x08,                    // Command length
    0x00, 0x00,              // Command ID, Reserved
    BMD_CAT_LENS,            // Category (Lens)
    BMD_PARAM_FOCUS,         // Parameter (Focus)
    BMD_TYPE_FIXED16,        // Data type (fixed16)
    BMD_OP_ASSIGN,           // Operation (assign)
    (uint8_t)(rawValue & 0xFF),         // Low byte
    (uint8_t)((rawValue >> 8) & 0xFF),  // High byte
    0x00, 0x00               // Padding
  };
  
  // Send the command
  _pOutgoingCameraControl->writeValue(focusCommand, sizeof(focusCommand), true);
  return true;
}

// Set focus with normalized value (0.0-1.0)
bool BMDBLEController::setFocus(float normalizedValue) {
  // Clamp to 0.0-1.0
  if (normalizedValue < 0.0f) normalizedValue = 0.0f;
  if (normalizedValue > 1.0f) normalizedValue = 1.0f;
  
  // Convert normalized value to raw value
  uint16_t rawValue = (uint16_t)(normalizedValue * 2048.0f);
  
  return setFocus(rawValue);
}

// Set iris (aperture) with normalized value (0.0-1.0)
bool BMDBLEController::setIris(float normalizedValue) {
  if (!isConnected() || !_pOutgoingCameraControl) {
    return false;
  }
  
  // Clamp to 0.0-1.0
  if (normalizedValue < 0.0f) normalizedValue = 0.0f;
  if (normalizedValue > 1.0f) normalizedValue = 1.0f;
  
  // Convert normalized value to fixed16 format
  int16_t fixed16Value = (int16_t)(normalizedValue * 2048.0f);
  
  // Create iris command
  uint8_t irisCommand[12] = {
    0xFF,                    // Protocol identifier
    0x08,                    // Command length
    0x00, 0x00,              // Command ID, Reserved
    BMD_CAT_LENS,            // Category (Lens)
    BMD_PARAM_IRIS_NORM,     // Parameter (Iris Normalized)
    BMD_TYPE_FIXED16,        // Data type (fixed16)
    BMD_OP_ASSIGN,           // Operation (assign)
    (uint8_t)(fixed16Value & 0xFF),         // Low byte
    (uint8_t)((fixed16Value >> 8) & 0xFF),  // High byte
    0x00, 0x00               // Padding
  };
  
  // Send the command
  _pOutgoingCameraControl->writeValue(irisCommand, sizeof(irisCommand), true);
  return true;
}

// Set white balance (in Kelvin)
bool BMDBLEController::setWhiteBalance(uint16_t kelvin) {
  if (!isConnected() || !_pOutgoingCameraControl) {
    return false;
  }
  
  // Clamp to reasonable range (2500-10000K)
  if (kelvin < 2500) kelvin = 2500;
  if (kelvin > 10000) kelvin = 10000;
  
  // Create white balance command
  uint8_t wbCommand[14] = {
    0xFF,                    // Protocol identifier
    0x08,                    // Command length
    0x00, 0x00,              // Command ID, Reserved
    BMD_CAT_VIDEO,           // Category (Video)
    BMD_PARAM_WB,            // Parameter (White Balance)
    BMD_TYPE_INT16,          // Data type (int16)
    BMD_OP_ASSIGN,           // Operation (assign)
    (uint8_t)(kelvin & 0xFF),         // Value low byte
    (uint8_t)((kelvin >> 8) & 0xFF),  // Value high byte
    0x00, 0x00,              // Tint (0)
    0x00, 0x00               // Padding
  };
  
  // Send the command
  _pOutgoingCameraControl->writeValue(wbCommand, sizeof(wbCommand), true);
  return true;
}

// Trigger auto focus
bool BMDBLEController::doAutoFocus() {
  if (!isConnected() || !_pOutgoingCameraControl) {
    return false;
  }
  
  // Create auto focus command
  uint8_t afCommand[12] = {
    0xFF,                    // Protocol identifier
    0x08,                    // Command length
    0x00, 0x00,              // Command ID, Reserved
    BMD_CAT_LENS,            // Category (Lens)
    BMD_PARAM_AUTO_FOCUS,    // Parameter (Auto Focus)
    BMD_TYPE_VOID,           // Data type (void)
    BMD_OP_ASSIGN,           // Operation (assign)
    0x01,                    // Trigger value (1)
    0x00, 0x00, 0x00         // Padding
  };
  
  // Send the command
  _pOutgoingCameraControl->writeValue(afCommand, sizeof(afCommand), true);
  return true;
}

// Toggle recording state
bool BMDBLEController::toggleRecording() {
  if (_recordingState) {
    return stopRecording();
  } else {
    return startRecording();
  }
}

// Start recording
bool BMDBLEController::startRecording() {
  if (!isConnected() || !_pOutgoingCameraControl) {
    return false;
  }
  
  // Create record command
  uint8_t recordCommand[12] = {
    0xFF,                    // Protocol identifier
    0x05,                    // Command length
    0x00, 0x00,              // Command ID, Reserved
    BMD_CAT_TRANSPORT,       // Category (Transport)
    BMD_PARAM_TRANSPORT_MODE,// Parameter (Transport Mode)
    BMD_TYPE_BYTE,           // Data type (signed byte)
    BMD_OP_ASSIGN,           // Operation (assign)
    0x02,                    // Record mode (2)
    0x00, 0x00, 0x00         // Padding
  };
  
  // Send the command
  _pOutgoingCameraControl->writeValue(recordCommand, sizeof(recordCommand), true);
  return true;
}

// Stop recording
bool BMDBLEController::stopRecording() {
  if (!isConnected() || !_pOutgoingCameraControl) {
    return false;
  }
  
  // Create stop command
  uint8_t stopCommand[12] = {
    0xFF,                    // Protocol identifier
    0x05,                    // Command length
    0x00, 0x00,              // Command ID, Reserved
    BMD_CAT_TRANSPORT,       // Category (Transport)
    BMD_PARAM_TRANSPORT_MODE,// Parameter (Transport Mode)
    BMD_TYPE_BYTE,           // Data type (signed byte)
    BMD_OP_ASSIGN,           // Operation (assign)
    0x00,                    // Preview mode (0)
    0x00, 0x00, 0x00         // Padding
  };
  
  // Send the command
  _pOutgoingCameraControl->writeValue(stopCommand, sizeof(stopCommand), true);
  return true;
}

// Request parameter
bool BMDBLEController::requestParameter(uint8_t category, uint8_t parameterId, uint8_t dataType) {
  if (!isConnected() || !_pOutgoingCameraControl) {
    return false;
  }
  
  // Create parameter request command
  uint8_t requestCommand[12] = {
    0xFF,                    // Protocol identifier
    0x08,                    // Command length
    0x00, 0x00,              // Command ID, Reserved
    category,                // Category
    parameterId,             // Parameter
    dataType,                // Data type
    BMD_OP_REPORT,           // Operation (report/request)
    0x00, 0x00, 0x00, 0x00   // Empty data & padding
  };
  
  // Send the command
  _pOutgoingCameraControl->writeValue(requestCommand, sizeof(requestCommand), true);
  return true;
}
