#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <BLEAddress.h>

// Blackmagic Camera Service and Characteristic UUIDs
#define BLACKMAGIC_SERVICE_UUID        "291d567a-6d75-11e6-8b77-86f30ca893d3"
#define OUTGOING_CAMERA_CONTROL_UUID "5dd3465f-1aee-4299-8493-d2eca2f8e1bb"
#define INCOMING_CAMERA_CONTROL_UUID "b864e140-76a0-416a-bf30-5876504537d9"
#define TIMECODE_CHARACTERISTIC_UUID "6d8f2110-86f1-41bf-9afb-451d87e976c8"
#define CAMERA_STATUS_UUID            "7fe8691d-95dc-4fc5-8abd-ca74339b51b9"
#define DEVICE_NAME_UUID              "ffac0c52-c9fb-41a0-b063-cc76282eb89c" //Not used here
#define PROTOCOL_VERSION_UUID         "8f1fd018-b508-456f-8f82-3d392bee2706" //Not used here
#define DEVICE_INFO_SERVICE_UUID      "180a"
#define CAMERA_MANUFACTURER_UUID      "2a29"
#define CAMERA_MODEL_UUID             "2a24"


static BLEUUID serviceUUID(BLACKMAGIC_SERVICE_UUID);
static BLEUUID charUUID(OUTGOING_CAMERA_CONTROL_UUID); // For sending data

static bool doConnect = false;
static bool connected = false;
static bool doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;
static BLEClient*  pClient  = nullptr; //Declare the client

BLERemoteCharacteristic* pOutgoingCharacteristic = nullptr;
BLERemoteCharacteristic* pIncomingCharacteristic = nullptr;
BLERemoteCharacteristic* pTimecodeCharacteristic = nullptr;
BLERemoteCharacteristic* pCameraStatusCharacteristic = nullptr;
BLERemoteCharacteristic* pCameraManufacturerCharacteristic = nullptr;
BLERemoteCharacteristic* pCameraModelCharacteristic = nullptr;
bool servicesResolved = false; // Flag to track if services/characteristics have been discovered


// Replace with the actual name or address of your Blackmagic Camera
static String cameraName = "Pocket Cinema Camera 4K";
//static String cameraAddress = "xx:xx:xx:xx:xx:xx"; // Example MAC address

// Callback for notifications
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {

    if (pBLERemoteCharacteristic == pIncomingCharacteristic) {
      // Handle incoming camera control messages
      Serial.print("Incoming Camera Control: ");
      for (int i = 0; i < length; i++) {
        Serial.printf("%02X ", pData[i]); // Print as hex
      }
      Serial.println();
      // Parse the incoming SDI protocol message (see Blackmagic documentation)

    } else if (pBLERemoteCharacteristic == pTimecodeCharacteristic) {
        // Handle timecode
        if (length == 4) {  // Timecode is a 32-bit (4 byte) value
          uint32_t timecodeBCD = (pData[0] << 24) | (pData[1] << 16) | (pData[2] << 8) | pData[3]; //Combine the bytes

          //BCD Conversions
          uint8_t hours   = (timecodeBCD >> 24) & 0xFF;
          uint8_t minutes = (timecodeBCD >> 16) & 0xFF;
          uint8_t seconds = (timecodeBCD >> 8)  & 0xFF;
          uint8_t frames  = timecodeBCD & 0xFF;
          Serial.printf("Timecode: %02X:%02X:%02X:%02X\n", hours, minutes, seconds, frames);
        }
    } else if (pBLERemoteCharacteristic == pCameraStatusCharacteristic) {
          if(length>0){
            Serial.print("Camera Status Changed: 0x");
            Serial.println(pData[0],HEX); //Print the first data as hex
          }
    }
}

// Callback for connection events
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    servicesResolved = false; //Reset services flag
    Serial.println("onDisconnect");
     doConnect = true; //Reconect
  }
};

//Connect to the BLE Server
bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());

     if (pClient == nullptr) { //Create just in case
        pClient = BLEDevice::createClient();
    }
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");


    // Obtain references to characteristics
    pOutgoingCharacteristic = pRemoteService->getCharacteristic(OUTGOING_CAMERA_CONTROL_UUID);
    pIncomingCharacteristic = pRemoteService->getCharacteristic(INCOMING_CAMERA_CONTROL_UUID);
    pTimecodeCharacteristic = pRemoteService->getCharacteristic(TIMECODE_CHARACTERISTIC_UUID);
    pCameraStatusCharacteristic = pRemoteService->getCharacteristic(CAMERA_STATUS_UUID);

    if (pOutgoingCharacteristic == nullptr || pIncomingCharacteristic == nullptr ||
        pTimecodeCharacteristic == nullptr || pCameraStatusCharacteristic == nullptr) {
      Serial.println("Failed to find all necessary characteristics");
      pClient->disconnect();
      return false;
    }

    Serial.println(" - Found all characteristics");

    BLERemoteService* pDeviceInfoService = pClient->getService(DEVICE_INFO_SERVICE_UUID);

      if(pDeviceInfoService != nullptr){
          pCameraManufacturerCharacteristic = pDeviceInfoService->getCharacteristic(CAMERA_MANUFACTURER_UUID);
          pCameraModelCharacteristic = pDeviceInfoService->getCharacteristic(CAMERA_MODEL_UUID);

        if(pCameraManufacturerCharacteristic != nullptr){
            std::string manufacturer = pCameraManufacturerCharacteristic->readValue(); //Correct way
            Serial.print("Manufacturer: ");
            Serial.println(manufacturer.c_str());
        }
        if(pCameraModelCharacteristic != nullptr){
          std::string model = pCameraModelCharacteristic->readValue();  //Correct way
          Serial.print("Model: ");
          Serial.println(model.c_str());
        }

      } else {
        Serial.print("Device Info Service Not Found");
      }



    // Register for notifications. VERY IMPORTANT. Do this *before* writing to the status.
    pIncomingCharacteristic->registerForNotify(notifyCallback);
    pTimecodeCharacteristic->registerForNotify(notifyCallback);
    pCameraStatusCharacteristic->registerForNotify(notifyCallback);

    //Set the flag to service resolved
     servicesResolved = true;

    // Send Power on Command. This is critical.
    uint8_t powerOnValue = 0x01;
    pCameraStatusCharacteristic->writeValue((uint8_t*)&powerOnValue, 1, true); // With response! Very important
    Serial.println("Sent power on command. Waiting for bonding...");

    return true;
}

// Scan for BLE servers and find the Blackmagic Camera
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
     if (advertisedDevice.haveName() && advertisedDevice.getName().find(cameraName.c_str()) == 0) //Correct way
      {
        BLEDevice::getScan()->stop(); // Stop scanning.
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        doScan = true;
      }

  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}

void loop() {
    // If the flag "doConnect" is true then we have scanned for and found the desired
    // BLE Server.  Now we connect to it.  Once we are connected we set the connected
    // flag to be true.
    if (doConnect == true) {
        if (connectToServer()) {
            Serial.println("We are now connected to the BLE Server.");
            connected = true;

        } else {
            Serial.println("We have failed to connect to the server; there is nothin more to do.");
        }
        doConnect = false;
    }

    // If we are connected and services resolved, send test command
    if (connected && servicesResolved) {

        delay(1000); //Short Delay

        //Send a sample command to change the iso to 800
         uint8_t isoCommand[] = {
          0x00, // Destination device (0 for camera)
          0x06, // Command length (6 bytes of data)
          0x00, // Command ID (0 for change configuration)
          0x00, // Reserved
          0x01, // Category (Video)
          0x0E, // Parameter (ISO)
          0x03, // Data type (signed 32-bit integer)
          0x00, // Operation type (assign value)
          0x00, 0x00, 0x03, 0x20  // ISO value (800 = 0x0320). Little Endian
        };

        // Send the command.  Use writeValue with response = true.  Essential for bonding.
        if (pOutgoingCharacteristic) {  // Check that characteristic is valid
              pOutgoingCharacteristic->writeValue(isoCommand, sizeof(isoCommand), true);
              Serial.println("Sent ISO command");
          }
    }

    if(doScan && !connected){
        BLEDevice::getScan()->start(0);  // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
    }

    delay(1000); // Delay a second between loops
}
