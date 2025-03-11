// src/Input/SerialPinInputMethod.h
#ifndef BMD_SERIAL_PIN_INPUT_METHOD_H
#define BMD_SERIAL_PIN_INPUT_METHOD_H

#include "../Interfaces/PinInputInterface.h"
#include <Arduino.h>

namespace BMDCamera {
    class SerialPinInputMethod : public PinInputInterface {
    public:
        // Constructor with default values
        SerialPinInputMethod(
            uint32_t timeoutMs = 30000,  // 30 seconds default timeout
            int maxAttempts = 3           // 3 attempts default
        );

        // Implement PIN request method
        uint32_t requestPin() override;

        // Set maximum PIN entry attempts
        void setMaxAttempts(int maxAttempts) override;

        // Set timeout for PIN entry
        void setTimeout(uint32_t timeoutMs) override;

        // Reset PIN input state
        void reset() override;

    private:
        uint32_t m_timeoutMs;     // Timeout for PIN entry
        int m_maxAttempts;        // Maximum number of attempts
        int m_currentAttempts;    // Current attempt count
    };
}

#endif // BMD_SERIAL_PIN_INPUT_METHOD_H
