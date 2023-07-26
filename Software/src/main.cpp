#define DEVICE "ESP32"

#define ESP32_CAN_TX_PIN GPIO_NUM_17  // Set CAN TX port to 17
#define ESP32_CAN_RX_PIN GPIO_NUM_4  // Set CAN RX port to 4

#define INFLUXDB_URL "http://cloud.baumistlustig.eu:8086"
#define INFLUXDB_TOKEN "h8S4V-g_bvHkhf11t276cOQ8-pxJhUtPGAiF9He1sDyU3Lu1Bmnwjx0__5eb2kIoWn2-iWB8Z3ofQQa9u3G1VA=="
#define INFLUXDB_ORG "554b5bb375690e45"
#define INFLUXDB_BUCKET "boat"

#define WRITE_PRECISION WritePrecision::S
#define MAX_BATCH_SIZE 10
#define WRITE_BUFFER_SIZE 30

#define WIFI_SSID "JojoNet"
#define WIFI_PASSWORD "Sennahoj08!?"

#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <NMEA2000_CAN.h>
#include <N2kMessages.h>
#include <queue/queue.h>
#include <iostream>
#include <RTClib.h>
#include <Wire.h>
#include <TimeLib.h>

// SD-Card helper functions
#include "../src/sDCard/sdCard.h"

// Setup Influxdb Client
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

// To store last Node Address
int NodeAddress;

// Nonvolatile storage on ESP32 - To store LastDeviceAddress
Preferences preferences;

RTC_DS3231 rtc;

// Create influxdb sensor points
Point mainBattery("main_battery");
Point starterBattery("starter_battery");
Point general("general");

WiFiMulti wifiMulti;

unsigned long previousMillis = 0;
unsigned long interval = 30000;

bool previous = false;

time_t currentTime;

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

const char* months[12] = {
  "Jan",
  "Feb",
  "Mar",
  "Apr",
  "May",
  "Jun",
  "Jul",
  "Aug",
  "Sep",
  "Oct",
  "Nov",
  "Dec"
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
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nis.gov");

    // Wait till time is synced
    Serial.print("Syncing time");
    int i = 0;
    while (time(nullptr) < 1000000000l && i < 40) {
      Serial.print(".");
      delay(500);
      i++;
    }
    Serial.println();

    // Show time
    time_t tnow = time(nullptr);
    Serial.print("Synchronized time: ");
    Serial.println(ctime(&tnow));

    currentTime = tnow;

    Serial.print("Current time: ");
    Serial.println(currentTime);

    // Set time on RTC
    char date[32];
    sprintf(
      date,
      "%03s %02d %02d",
      months[month(currentTime) - 1],
      day(currentTime),
      year(currentTime)
    );

    char time[32];
    sprintf(
      time,
      "%02d:%02d:%02d",
      hour(currentTime),
      minute(currentTime),
      second(currentTime)
    );

    Serial.print("Time: ");
    Serial.print(date);
    Serial.print(" ");
    Serial.println(time);

    rtc.adjust(DateTime(F(date), F(time)));
    
    Serial.print("RTC Temp: ");
    Serial.println(rtc.getTemperature());

    Serial.print("RTC time: ");
    Serial.println(rtc.now().unixtime());

    // Update influxdb connection
    newNetwork();

    return true;
  }
  
  Serial.println("Unable to connect to a Wifi");

  return false;
}

// Write data to buffer
void writeToBuffer(time_t timestamp, const char* data, const char* path) {

  Serial.print("Path: ");
  Serial.println(path);

  // Append data to SD-Card
  char timestampChar[100];
  sprintf(timestampChar, "%2.13f", timestamp);

  Serial.print("Timestamp: ");
  Serial.println(timestampChar);

  std::string pathString = path;

  std::string timestampPathString = pathString + "_timestamps.txt";

  char* timestampPath = { new char[timestampPathString.length() + 1] };
  strcpy(timestampPath, timestampPathString.c_str());

  Serial.print("TimestampPath: ");
  Serial.println(timestampPath);

  std::string dataPathString = pathString + "_data.txt";

  char* dataPath = { new char[dataPathString.length() + 1] };
  strcpy(dataPath, dataPathString.c_str());

  Serial.print("DataPath: ");
  Serial.println(dataPath);

  appendFile(SD, timestampPath, timestampChar);
  appendFile(SD, dataPath, data);
}

// Writes entire data from SD-Card to influxdb
void writeFromBuffer() {
  Serial.println("Writing from buffer");


  char* data = "{\"main_battery\":{\"voltage\":[{\"time\":\"1337\",\"data\":13.2},{\"time\":\"1338\",\"data\":14.1}],\"current\":[{\"time\":\"1337\",\"data\":5.1},{\"time\":\"1338\",\"data\":5.2}],\"temperature\":[{\"time\":\"1337\",\"data\":26.8},{\"time\":\"1338\",\"data\":26.9}]},\"starter_battery\":{\"voltage\":[{\"time\":\"1337\",\"data\":14.7},{\"time\":\"1338\",\"data\":15.2}],\"current\":[{\"time\":\"1337\",\"data\":1.7},{\"time\":\"1338\",\"data\":2.1}],\"temperature\":[{\"time\":\"1337\",\"data\":27.7},{\"time\":\"1338\",\"data\":27.6}]},\"general\":{\"rssi\":[{\"time\":\"1337\",\"data\":30},{\"time\":\"1338\",\"data\":29.9}]}}";
  
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

      // Get current time
      DateTime now = rtc.now();

      char voltage[200];
      sprintf(voltage, "%2.13f", BatteryVoltage);
      writeToBuffer(now.unixtime(), voltage, "/main_battery_voltage");

      char current[200];
      sprintf(current, "%2.13f", BatteryCurrent);
      writeToBuffer(now.unixtime(), current, "/main_battery_current");

      char temperature[200];
      sprintf(temperature, "%2.13f", KelvinToC(BatteryTemperature));
      writeToBuffer(now.unixtime(), temperature, "/main_battery_temperature");
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

      // Get current time
      DateTime now = rtc.now();

      char voltage[200];
      sprintf(voltage, "%2.13f", BatteryVoltage);
      writeToBuffer(now.unixtime(), voltage, "/starter_battery_voltage");

      char current[200];
      sprintf(current, "%2.13f", BatteryCurrent);
      writeToBuffer(now.unixtime(), current, "/starter_battery_current");

      char temperature[200];
      sprintf(temperature, "%2.13f", KelvinToC(BatteryTemperature));
      writeToBuffer(now.unixtime(), temperature, "/starter_battery_temperature");
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

	// init RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  
  // Enable client side influxdb timestamps - set time precision to milliseconds
  client.setWriteOptions(WriteOptions().writePrecision(WRITE_PRECISION).batchSize(MAX_BATCH_SIZE).bufferSize(WRITE_BUFFER_SIZE));

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

  delay(1);
}