#ifndef BMD_PIN_INPUT_INTERFACE_H
#define BMD_PIN_INPUT_INTERFACE_H

#include <cstdint>
#include <stdexcept>
#include <memory>

namespace BMDCamera {
    class PinInputInterface {
    public:
        // Virtual destructor for proper inheritance
        virtual ~PinInputInterface() = default;
        
        // Pure virtual method to request PIN
        virtual uint32_t requestPin() = 0;
        
        // Configure maximum PIN entry attempts
        virtual void setMaxAttempts(int maxAttempts) = 0;
        
        // Configure timeout for PIN entry
        virtual void setTimeout(uint32_t timeoutMs) = 0;
        
        // Optional method to reset PIN input state
        virtual void reset() {
            // Default implementation does nothing
        }
    };

    // Type alias for unique pointer to PIN input method
    using PinInputMethodPtr = std::unique_ptr<PinInputInterface>;

    // Factory function for creating PIN input methods
    template<typename T, typename... Args>
    std::unique_ptr<PinInputInterface> createPinInputMethod(Args&&... args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }
}

#endif // BMD_PIN_INPUT_INTERFACE_H
