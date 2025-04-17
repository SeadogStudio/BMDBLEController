/**
* @file IncomingCameraControlManager.h
* @brief Management of incoming data from Blackmagic Design cameras via BLE
* @author BMDBLEController Contributors
*/

#ifndef INCOMING_CAMERA_CONTROL_MANAGER_H
#define INCOMING_CAMERA_CONTROL_MANAGER_H

#include <Arduino.h>
#include <unordered_map>
#include <vector>
#include <functional>
#include "ProtocolConstants.h"
#include "ProtocolUtils.h"

namespace BMDBLEController {

/**
* @class IncomingCameraControlManager
* @brief Manages the storage and processing of data received from the camera
*/
class IncomingCameraControlManager {
public:
   /**
    * @brief Constructor
    */
   IncomingCameraControlManager();

   /**
    * @brief Process and store incoming data from the camera
    * @param data Pointer to the incoming data
    * @param length Length of the incoming data
    * @return True if the data was processed successfully, false otherwise
    */
   bool processIncomingData(const uint8_t* data, size_t length);

   /**
    * @brief Get the most recent raw data for a specific category and parameter
    * @param category The category ID
    * @param parameter The parameter ID
    * @return Vector of bytes containing the raw data, or empty vector if not found
    */
   std::vector<uint8_t> getRawParameterData(uint8_t category, uint8_t parameter) const;

   /**
    * @brief Check if data exists for a specific category and parameter
    * @param category The category ID
    * @param parameter The parameter ID
    * @return True if data exists, false otherwise
    */
   bool hasParameter(uint8_t category, uint8_t parameter) const;

   /**
    * @brief Get the data type for a specific category and parameter
    * @param category The category ID
    * @param parameter The parameter ID
    * @return The data type, or 0 if not found
    */
   uint8_t getParameterDataType(uint8_t category, uint8_t parameter) const;

   /**
    * @brief Get the timestamp of when a parameter was last updated
    * @param category The category ID
    * @param parameter The parameter ID
    * @return Timestamp in milliseconds, or 0 if not found
    */
   unsigned long getParameterTimestamp(uint8_t category, uint8_t parameter) const;

   /**
    * @brief Get parameter value as int8_t
    * @param category The category ID
    * @param parameter The parameter ID
    * @param defaultValue Value to return if parameter not found
    * @return The parameter value as int8_t
    */
   int8_t getInt8Parameter(uint8_t category, uint8_t parameter, int8_t defaultValue = 0) const;

   /**
    * @brief Get parameter value as int16_t
    * @param category The category ID
    * @param parameter The parameter ID
    * @param defaultValue Value to return if parameter not found
    * @return The parameter value as int16_t
    */
   int16_t getInt16Parameter(uint8_t category, uint8_t parameter, int16_t defaultValue = 0) const;

   /**
    * @brief Get parameter value as int32_t
    * @param category The category ID
    * @param parameter The parameter ID
    * @param defaultValue Value to return if parameter not found
    * @return The parameter value as int32_t
    */
   int32_t getInt32Parameter(uint8_t category, uint8_t parameter, int32_t defaultValue = 0) const;

   /**
    * @brief Get parameter value as float (from fixed16)
    * @param category The category ID
    * @param parameter The parameter ID
    * @param defaultValue Value to return if parameter not found
    * @return The parameter value as float
    */
   float getFixed16Parameter(uint8_t category, uint8_t parameter, float defaultValue = 0.0f) const;

   /**
    * @brief Get parameter value as String
    * @param category The category ID
    * @param parameter The parameter ID
    * @param defaultValue Value to return if parameter not found
    * @return The parameter value as String
    */
   String getStringParameter(uint8_t category, uint8_t parameter, const String& defaultValue = "") const;

   /**
    * @brief Get most recent complete raw packet
    * @return Vector containing the most recent packet
    */
   std::vector<uint8_t> getLastPacket() const;

   /**
    * @brief Clear all stored parameter data
    */
   void clearAllParameters();

   /**
    * @brief Type definition for packet callback function
    */
   using PacketCallback = std::function<void(const std::vector<uint8_t>&)>;

   /**
    * @brief Type definition for parameter callback function
    */
   using ParameterCallback = std::function<void(uint8_t, uint8_t, const std::vector<uint8_t>&)>;

   /**
    * @brief Set callback for all received packets
    * @param callback Function to call for each packet
    */
   void setAllPacketsCallback(PacketCallback callback);

   /**
    * @brief Set callback for a specific parameter
    * @param category The category ID
    * @param parameter The parameter ID
    * @param callback Function to call when the parameter is updated
    */
   void setParameterCallback(uint8_t category, uint8_t parameter, ParameterCallback callback);

   /**
    * @brief Set callback for all parameters in a category
    * @param category The category ID
    * @param callback Function to call when any parameter in the category is updated
    */
   void setCategoryCallback(uint8_t category, ParameterCallback callback);

private:
   /**
    * @brief Structure to hold parameter data
    */
   struct ParameterData {
       std::vector<uint8_t> data;  // Raw data bytes
       uint8_t dataType;           // Data type
       unsigned long timestamp;    // Time of last update
   };

   // Map of category -> parameter -> data
   std::unordered_map<uint8_t, std::unordered_map<uint8_t, ParameterData>> m_parameters;

   // Most recent complete packet
   std::vector<uint8_t> m_lastPacket;

   // Callback for all packets
   PacketCallback m_allPacketsCallback;

   // Callbacks for specific parameters
   std::unordered_map<uint8_t, std::unordered_map<uint8_t, ParameterCallback>> m_parameterCallbacks;

   // Callbacks for all parameters in a category
   std::unordered_map<uint8_t, ParameterCallback> m_categoryCallbacks;
};

} // namespace BMDBLEController

#endif // INCOMING_CAMERA_CONTROL_MANAGER_H
