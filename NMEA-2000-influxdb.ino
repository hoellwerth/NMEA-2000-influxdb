#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <Arduino.h>
#include <Preferences.h>
#include <NMEA2000_CAN.h>
#include <N2kMessages.h>

#define DEVICE "ESP32"

// Wifi credentials
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// CAN TX/RX Pins
#define ESP32_CAN_TX_PIN GPIO_NUM_5
#define ESP32_CAN_RX_PIN GPIO_NUM_4

// InfluxDB credentials
#define INFLUXDB_URL "influxdb-url"
#define INFLUXDB_TOKEN "toked-id"
#define INFLUXDB_ORG "org"
#define INFLUXDB_BUCKET "bucket"

// Setup Influxdb Client
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

// Create influxdb sensors
Point mainBattery("main_battery");
Point general("general");

WiFiMulti wifiMulti;
Preferences preferences;

// Store last Node Address
int NodeAddress;

// Set the information for other bus devices, which messages we support
const unsigned long ReceiveMessages[] PROGMEM = {/*126992L,*/ // System time
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
      0
    };

void setup() {
  // Start Serial Stream
  Serial.begin(115200);
  delay(10);

  uint8_t chipid[6];
  uint32_t id = 0;
  int i = 0;

  // Reserve enough buffer for sending all messages.
  NMEA2000.SetN2kCANMsgBufSize(8);
  NMEA2000.SetN2kCANReceiveFrameBufSize(150);
  NMEA2000.SetN2kCANSendFrameBufSize(150);

  // Generate unique number from chip id
  esp_efuse_mac_get_default(chipid);
  for (i = 0; i < 6; i++) {
    id += (chipid[i] << (7 * i));
  }

  // Set product information
  NMEA2000.SetProductInformation("1", // Manufacturer's Model serial code
                                 100, // Manufacturer's product code
                                 "NMEA Reader -> InfluxDB",  // Manufacturer's Model ID
                                 "1.0.2.25 (2019-07-07)",  // Manufacturer's Software version code
                                 "1.0.2.0 (2019-07-07)" // Manufacturer's Model version
                                );
  // Set device information
  NMEA2000.SetDeviceInformation(id,  // Unique number. Use e.g. Serial number.
                                131, // Device function=NMEA 2000 to Analog Gateway. See codes on http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                25,  // Device class=Inter/Intranetwork Device. See codes on  http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                2046 // Just choosen free from code list on http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
                               );

  // Open nonvolatile storage (nvs)
  preferences.begin("nvs", false);
  
  // Read stored last NodeAddress, default 34
  NodeAddress = preferences.getInt("LastNodeAddress", 34);
  preferences.end();
  Serial.printf("NodeAddress=%d\n", NodeAddress);

  // Configure NMEA-Listener
  NMEA2000.SetMode(tNMEA2000::N2km_ListenOnly, NodeAddress);
  NMEA2000.ExtendReceiveMessages(ReceiveMessages);
  NMEA2000.SetMsgHandler(MsgHandler);

  NMEA2000.Open();

  delay(200);

  // Connect WiFi
  Serial.println("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("Connected to ");
  Serial.println(WIFI_SSID);
}

void newNetwork() {
  general.clearTags();
  // Add constant tags
  general.addTag("SSID", WiFi.SSID());

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void MsgHandler(const tN2kMsg &N2kMsg) {

  // Route PGN to Handler function
  switch (N2kMsg.PGN) {
    case 127508L: handleBatteryStatus(N2kMsg);
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

void loop() {

  NMEA2000.ParseMessages();

  CheckSourceAddressChange();

  // Store measured value into point
  general.clearFields();
  // Report RSSI of currently connected network
  general.addField("rssi", WiFi.RSSI());
  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(client.pointToLineProtocol(general));
  // If no Wifi signal, try to reconnect it
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }
  // Write point
  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  // Wait 10s
  Serial.println("Wait 10s");
  delay(10000);

  // Dummy to empty input buffer to avoid board to stuck with e.g. NMEA Reader
  if ( Serial.available() ) {
    Serial.read();
  }
}