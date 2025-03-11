// src/Input/SerialPinInputMethod.cpp
#include "SerialPinInputMethod.h"

namespace BMDCamera {
    SerialPinInputMethod::SerialPinInputMethod(
        uint32_t timeoutMs, 
        int maxAttempts
    ) : 
        m_timeoutMs(timeoutMs), 
        m_maxAttempts(maxAttempts),
        m_currentAttempts(0) {}

    uint32_t SerialPinInputMethod::requestPin() {
        // Increment attempt counter
        m_currentAttempts++;

        // Check if max attempts exceeded
        if (m_currentAttempts > m_maxAttempts) {
            Serial.println("Maximum PIN entry attempts exceeded.");
            return 0;  // Indicate failure
        }

        // Prompt for PIN
        Serial.println("Enter 6-digit PIN (Press Enter to submit):");
        
        uint32_t pinCode = 0;
        unsigned long startTime = millis();

        // Input loop with timeout
        while (millis() - startTime < m_timeoutMs) {
            if (Serial.available()) {
                char ch = Serial.read();
                
                // Handle digit input
                if (ch >= '0' && ch <= '9') {
                    pinCode = pinCode * 10 + (ch - '0');
                    Serial.print(ch);
                }
                
                // Handle submission (newline or 6 digits)
                if (ch == '\n' || pinCode >= 100000) {
                    Serial.println();
                    return pinCode;
                }
            }
            
            // Prevent CPU blocking
            delay(10);
        }

        // Timeout occurred
        Serial.println("\nPIN entry timed out.");
        return 0;
    }

    void SerialPinInputMethod::setMaxAttempts(int maxAttempts) {
        m_maxAttempts = maxAttempts;
    }

    void SerialPinInputMethod::setTimeout(uint32_t timeoutMs) {
        m_timeoutMs = timeoutMs;
    }

    void SerialPinInputMethod::reset() {
        // Reset attempt counter
        m_currentAttempts = 0;
    }
}
