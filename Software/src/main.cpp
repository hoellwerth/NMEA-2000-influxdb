#define DEVICE "ESP32"

#define ESP32_CAN_TX_PIN GPIO_NUM_17  // Set CAN TX port to 17
#define ESP32_CAN_RX_PIN GPIO_NUM_4  // Set CAN RX port to 4

#define INFLUXDB_URL "http://cloud.baumistlustig.eu:8086"
#define INFLUXDB_TOKEN "h8S4V-g_bvHkhf11t276cOQ8-pxJhUtPGAiF9He1sDyU3Lu1Bmnwjx0__5eb2kIoWn2-iWB8Z3ofQQa9u3G1VA=="
#define INFLUXDB_ORG "554b5bb375690e45"
#define INFLUXDB_BUCKET "boat"

#define WIFI_SSID "JojoNet"
#define WIFI_PASSWORD "Sennahoj08!?"

#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <NMEA2000_CAN.h>
#include <N2kMessages.h>

// SD-Card helper functions
#include "../src/sDCard/sdCard.h"

// Setup Influxdb Client
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

// To store last Node Address
int NodeAddress;

// Nonvolatile storage on ESP32 - To store LastDeviceAddress
Preferences preferences;

// Create influxdb sensor points
Point mainBattery("main_battery");
Point starterBattery("starter_battery");
Point general("general");

WiFiMulti wifiMulti;

unsigned long previousMillis = 0;
unsigned long interval = 30000;

bool previous = false;

// Set the information for other bus devices, which messages we support
const unsigned long ReceiveMessages[] PROGMEM = {
  126992L, // System time
  127250L, // Heading
  127258L, // Magnetic variation
  128259L, // Boat speed
  128267L, // Depth
  129025L, // Position
  129026L, // COG and SOG
  129029L, // GNSS
  130306L, // Wind
  128275L, // Log
  127245L, // Rudder
  127505L, // Fluids
  0
};

void newNetwork() {
  general.clearTags();

  // Add constant tags
  general.addTag("SSID", WiFi.SSID());

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
    return;
  }

  Serial.print("InfluxDB connection failed: ");
  Serial.println(client.getLastErrorMessage());
}

bool connectWifi() {
  Serial.println("Connecting to WiFi");

  // Set Wifi to stationary mdoe
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  if (wifiMulti.run() == WL_CONNECTED) {
    // Successfully connected
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());

    // Synchronize time with NTP servers and set timezone
    timeSync("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nis.gov");

    // Update influxdb connection
    newNetwork();

    return true;
  }
  
  Serial.println("Unable to connect to a Wifi");

  return false;
}

// Write data to buffer
void writeToBuffer(time_t timestamp, const char* data, const char* path) {
  // Append data to SD-Card
  char timestampString[20];
  sprintf(timestampString, "%2.13f", timestamp);

  char* timestampPath = { new char[strlen(path) + strlen("_timestamps.txt") + 1] };
  timestampPath = strcat(timestampPath, path);
  timestampPath = strcat(timestampPath, "_timestamps.txt");

  char* dataPath = { new char[strlen(path) + strlen("_data.txt") + 1] };
  dataPath = strcat(dataPath, path);
  dataPath = strcat(dataPath, "_data.txt");

  appendFile(SD, timestampPath, timestampString);
  appendFile(SD, dataPath, data);
}

// Writes entire data from SD-Card to influxdb
void writeFromBuffer() {
  Serial.println("Writing from buffer");

  // const char* data = readFile(SD, "/example.json");

  const char* data = "{\"main_battery\":{\"voltage\":[{\"time\":\"1337\",\"data\":13.2},{\"time\":\"1338\",\"data\":14.1}],\"current\":[{\"time\":\"1337\",\"data\":5.1},{\"time\":\"1338\",\"data\":5.2}],\"temperature\":[{\"time\":\"1337\",\"data\":26.8},{\"time\":\"1338\",\"data\":26.9}]},\"starter_battery\":{\"voltage\":[{\"time\":\"1337\",\"data\":14.7},{\"time\":\"1338\",\"data\":15.2}],\"current\":[{\"time\":\"1337\",\"data\":1.7},{\"time\":\"1338\",\"data\":2.1}],\"temperature\":[{\"time\":\"1337\",\"data\":27.7},{\"time\":\"1338\",\"data\":27.6}]},\"general\":{\"rssi\":[{\"time\":\"1337\",\"data\":30},{\"time\":\"1338\",\"data\":29.9}]}}";
  StaticJsonDocument<1024> doc;

  DeserializationError error = deserializeJson(doc, data);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  JsonObject main_battery = doc["main_battery"];

  for (JsonObject main_battery_voltage_item : main_battery["voltage"].as<JsonArray>()) {

    const char* main_battery_voltage_item_time = main_battery_voltage_item["time"]; // "1337", "1338"
    float main_battery_voltage_item_data = main_battery_voltage_item["data"]; // 13.2, 14.1
    
    Serial.println(main_battery_voltage_item_time);
    Serial.println(main_battery_voltage_item_data);
  }

  for (JsonObject main_battery_current_item : main_battery["current"].as<JsonArray>()) {

    const char* main_battery_current_item_time = main_battery_current_item["time"]; // "1337", "1338"
    float main_battery_current_item_data = main_battery_current_item["data"]; // 5.1, 5.2

    Serial.println(main_battery_current_item_time);
    Serial.println(main_battery_current_item_data);
  }

  for (JsonObject main_battery_temperature_item : main_battery["temperature"].as<JsonArray>()) {

    const char* main_battery_temperature_item_time = main_battery_temperature_item["time"]; // "1337", ...
    float main_battery_temperature_item_data = main_battery_temperature_item["data"]; // 26.8, 26.9

    Serial.println(main_battery_temperature_item_time);
    Serial.println(main_battery_temperature_item_data);
  }

  JsonObject starter_battery = doc["starter_battery"];

  for (JsonObject starter_battery_voltage_item : starter_battery["voltage"].as<JsonArray>()) {

    const char* starter_battery_voltage_item_time = starter_battery_voltage_item["time"]; // "1337", "1338"
    float starter_battery_voltage_item_data = starter_battery_voltage_item["data"]; // 14.7, 15.2

    Serial.println(starter_battery_voltage_item_time);
    Serial.println(starter_battery_voltage_item_data);
  }

  for (JsonObject starter_battery_current_item : starter_battery["current"].as<JsonArray>()) {

    const char* starter_battery_current_item_time = starter_battery_current_item["time"]; // "1337", "1338"
    float starter_battery_current_item_data = starter_battery_current_item["data"]; // 1.7, 2.1

    Serial.println(starter_battery_current_item_time);
    Serial.println(starter_battery_current_item_data);
  }

  for (JsonObject starter_battery_temperature_item : starter_battery["temperature"].as<JsonArray>()) {

    const char* starter_battery_temperature_item_time = starter_battery_temperature_item["time"]; // "1337", ...
    float starter_battery_temperature_item_data = starter_battery_temperature_item["data"]; // 27.7, 27.6

    Serial.println(starter_battery_temperature_item_time);
    Serial.println(starter_battery_temperature_item_data);
  }

  for (JsonObject general_rssi_item : doc["general"]["rssi"].as<JsonArray>()) {

    const char* general_rssi_item_time = general_rssi_item["time"]; // "1337", "1338"
    float general_rssi_item_data = general_rssi_item["data"]; // 30, 29.9

    Serial.println(general_rssi_item_time);
    Serial.println(general_rssi_item_data);
  }
  
  /*
  // Write data to influxdb
  starterBattery.addField("voltage", BatteryVoltage);
  starterBattery.addField("current", BatteryCurrent);
  starterBattery.addField("temperature", KelvinToC(BatteryTemperature));
  starterBattery.addField("power", BatteryVoltage * BatteryCurrent);
  */
    
}

void handleBatteryStatus(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  unsigned char BatteryInstance;
  double BatteryVoltage;
  double BatteryCurrent;
  double BatteryTemperature;

  if (!ParseN2kPGN127508(N2kMsg, BatteryInstance, BatteryVoltage, BatteryCurrent, BatteryTemperature, SID)) {
    return;
  }

  // If main battery
  if (BatteryInstance == 0.0) {
    Serial.printf("MainBattery: (Voltage: %3.1f V, Current: %3.1f A, Temp: %3.1f C)\n", BatteryVoltage, BatteryCurrent, KelvinToC(BatteryTemperature));

    if (wifiMulti.run() == WL_CONNECTED && previous) {
      previous = false;
      writeFromBuffer();
    }

    mainBattery.clearFields(); 

    mainBattery.addField("voltage", BatteryVoltage);
    mainBattery.addField("current", BatteryCurrent);
    mainBattery.addField("temperature", KelvinToC(BatteryTemperature));
    mainBattery.addField("power", BatteryVoltage * BatteryCurrent);

    // Write to influx
    client.pointToLineProtocol(starterBattery);

    // Catch errors
    if (!client.writePoint(mainBattery)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());

      char message[200];

      sprintf(message, "%2.13f", BatteryVoltage);

      appendFile(SD, "/log.txt", "MainBattery Voltage: ");
      appendFile(SD, "/log.txt", message);
      appendFile(SD, "/log.txt", "\n");
    }

    return;
  }

  // If starter battery
  if (BatteryInstance == 1.0) {
    Serial.printf("Starter Battery: (Voltage: %3.1f V, Current: %3.1f A, Temp: %3.1f C)\n", BatteryVoltage, BatteryCurrent, KelvinToC(BatteryTemperature));

    if (wifiMulti.run() == WL_CONNECTED && previous) {
      previous = false;
      writeFromBuffer();
    }

    starterBattery.clearFields();

    starterBattery.addField("voltage", BatteryVoltage);
    starterBattery.addField("current", BatteryCurrent);
    starterBattery.addField("temperature", KelvinToC(BatteryTemperature));
    starterBattery.addField("power", BatteryVoltage * BatteryCurrent);
    
    // Write to influx
    client.pointToLineProtocol(starterBattery);

    // Catch errors
    if (!client.writePoint(starterBattery)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());

      char message[200];

      sprintf(message, "%2.13f", BatteryVoltage);

      appendFile(SD, "/log.txt", "StarterBattery Voltage: ");
      appendFile(SD, "/log.txt", message);
      appendFile(SD, "/log.txt", "\n");
    }

    return;
  }
}

// Function to check if SourceAddress has changed (due to address conflict on bus)
void CheckSourceAddressChange() {
  int SourceAddress = NMEA2000.GetN2kSource();

  if (SourceAddress != NodeAddress) { // Save potentially changed Source Address to NVS memory
    NodeAddress = SourceAddress;      // Set new Node Address (to save only once)
    preferences.begin("nvs", false);
    preferences.putInt("LastNodeAddress", SourceAddress);
    preferences.end();
    Serial.printf("Address Change: New Address=%d\n", SourceAddress);
  }
}

void MyHandleNMEA2000Msg(const tN2kMsg &N2kMsg) {

  switch (N2kMsg.PGN) {
    case 127508L: handleBatteryStatus(N2kMsg);
  }
}

void setup() {
  uint8_t chipid[6];
  uint32_t id = 0;
  int i = 0;

  // Init USB serial port
  Serial.begin(115200);
  delay(10);

  // Enable client side influxdb timestamps - set time precision to milliseconds
  client.setWriteOptions(WriteOptions().writePrecision(WritePrecision::MS));

  // Reserve enough buffer for sending all messages.
  NMEA2000.SetN2kCANMsgBufSize(8);
  NMEA2000.SetN2kCANReceiveFrameBufSize(150);
  NMEA2000.SetN2kCANSendFrameBufSize(150);

  // Generate unique number from chip id
  esp_efuse_mac_get_default(chipid);
  for (i = 0; i < 6; i++) id += (chipid[i] << (7 * i));

  // Set product information
  NMEA2000.SetProductInformation(
    "1", // Manufacturer's Model serial code
    100, // Manufacturer's product code
    "NMEA Reader",  // Manufacturer's Model ID
    "1.0.2.25 (2019-07-07)",  // Manufacturer's Software version code
    "1.0.2.0 (2019-07-07)" // Manufacturer's Model version
  );
  
  // Set device information
  NMEA2000.SetDeviceInformation(id,  // Unique number. Use e.g. Serial number.
    131, // Device function=NMEA 2000 to Analog Gateway. See codes on http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
    25,  // Device class=Inter/Intranetwork Device. See codes on  http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
    2046 // Just choosen free from code list on http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
  );

  preferences.begin("nvs", false);                          // Open nonvolatile storage (nvs)
  NodeAddress = preferences.getInt("LastNodeAddress", 34);  // Read stored last NodeAddress, default 34
  preferences.end();
  Serial.printf("NodeAddress=%d\n", NodeAddress);

  // If you also want to see all traffic on the bus use N2km_ListenAndNode instead of N2km_NodeOnly below
  NMEA2000.SetMode(tNMEA2000::N2km_ListenOnly, NodeAddress);
  NMEA2000.ExtendReceiveMessages(ReceiveMessages);
  NMEA2000.SetMsgHandler(MyHandleNMEA2000Msg);

  NMEA2000.Open();
  delay(200);

  setupSD();

  connectWifi();
}


void loop() {

  // If not online - try connecting to a wifi
  unsigned long currentMillis = millis();

  // if WiFi is down, try reconnecting
  if ((wifiMulti.run() != WL_CONNECTED) && (currentMillis - previousMillis >=interval)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;

    previous = true;
  }

  // Parse the N2k messages
  NMEA2000.ParseMessages();

  // Check if adress has changed
  CheckSourceAddressChange();

  // Dummy to empty input buffer to avoid board to stuck with e.g. NMEA Reader
  if ( Serial.available() ) {
    Serial.read();
  }
}