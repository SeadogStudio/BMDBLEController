class BMDBLEController {
public:
  // Constructor & initialization
  BMDBLEController(String deviceName = "ESP32BMDControl");
  void begin();
  void begin(Adafruit_SSD1306* display);
  
  // Connection management
  bool scan(uint32_t duration = 10);
  bool connect();
  bool disconnect();
  bool isConnected();
  void clearBondingInfo();
  void setAutoReconnect(bool enabled);
  
  // Camera control methods
  bool setFocus(uint16_t rawValue);
  bool setFocus(float normalizedValue);
  bool setIris(float normalizedValue);
  bool setWhiteBalance(uint16_t kelvin);
  bool toggleRecording();
  bool doAutoFocus();
  bool isRecording();
  
  // Parameter methods
  bool requestParameter(uint8_t category, uint8_t parameterId, uint8_t dataType);
  String getParameterValue(uint8_t category, uint8_t parameterId);
  bool hasParameter(uint8_t category, uint8_t parameterId);
  
  // Display methods
  void updateDisplay();
  void setDisplayMode(uint8_t mode);
  
  // Event handling
  void setResponseCallback(void (*callback)(uint8_t, uint8_t, String));
  void loop(); // Must be called in Arduino loop()
  
private:
  // BLE connection handling
  void setupBLE();
  void setupSecurity();
  void checkConnection();
  
  // Parameter storage
  void processIncomingPacket(uint8_t* data, size_t length);
  void storeParameter(uint8_t category, uint8_t parameterId, 
                       uint8_t dataType, uint8_t* data, size_t length);
  
  // Display helper methods
  void displayConnectionStatus();
  void displayCameraInfo();
  void displayParameterValue(uint8_t category, uint8_t parameterId, int x, int y);
  
  // Member variables (BLE, parameters, display, etc.)
};
