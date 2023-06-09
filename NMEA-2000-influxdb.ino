/*
  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

// Lesen von PGNs vom NMEA2000-Bus
// Version 0.1, 08.01.2021, AK-Homberger
#define DEVICE "ESP32"

#define ESP32_CAN_TX_PIN GPIO_NUM_5  // Set CAN TX port to 5 
#define ESP32_CAN_RX_PIN GPIO_NUM_4  // Set CAN RX port to 4

#define INFLUXDB_URL "http://cloud.baumistlustig.eu:8086"
#define INFLUXDB_TOKEN "h8S4V-g_bvHkhf11t276cOQ8-pxJhUtPGAiF9He1sDyU3Lu1Bmnwjx0__5eb2kIoWn2-iWB8Z3ofQQa9u3G1VA=="
#define INFLUXDB_ORG "554b5bb375690e45"
#define INFLUXDB_BUCKET "boat"

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <Arduino.h>
#include <Preferences.h>
#include <NMEA2000_CAN.h>  // This will automatically choose right CAN library and create suitable NMEA2000 object
#include <N2kMessages.h>

// Setup Influxdb Client
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

int NodeAddress;            // To store last Node Address
Preferences preferences;    // Nonvolatile storage on ESP32 - To store LastDeviceAddress

// Create influxdb sensors
Point mainBattery("main_battery");
Point starterBattery("starter_battery");
Point general("general");

WiFiMulti wifiMulti;

bool online = false;

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


//*****************************************************************************
void setup() {
  uint8_t chipid[6];
  uint32_t id = 0;
  int i = 0;

  // Init USB serial port
  Serial.begin(115200);
  delay(10);

  // Reserve enough buffer for sending all messages.
  NMEA2000.SetN2kCANMsgBufSize(8);
  NMEA2000.SetN2kCANReceiveFrameBufSize(150);
  NMEA2000.SetN2kCANSendFrameBufSize(150);

  // Generate unique number from chip id
  esp_efuse_mac_get_default(chipid);
  for (i = 0; i < 6; i++) id += (chipid[i] << (7 * i));

  // Set product information
  NMEA2000.SetProductInformation("1", // Manufacturer's Model serial code
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
}

bool connectWifi() {
  Serial.println("Connecting to WiFi");

  // Set Wifi to stationary mdoe
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  // Try to connect 10 times
  for (int i = 0; i < 11; i++) {
    Serial.print(".");

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
  }
  
  Serial.println("Unable to connect to a Wifi");

  return false;
}

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

//*****************************************************************************
void MyHandleNMEA2000Msg(const tN2kMsg &N2kMsg) {

  switch (N2kMsg.PGN) {
    case 127508L: handleBatteryStatus(N2kMsg);
    case 127505L: handleFluidTanks(N2kMsg);
  }
}


//*****************************************************************************
void handleBatteryStatus(const tN2kMsg &N2kMsg) {
  unsigned char SID;
  unsigned char BatteryInstance;
  double BatteryVoltage;
  double BatteryCurrent;
  double BatteryTemperature;

  if (ParseN2kPGN127508(N2kMsg, BatteryInstance, BatteryVoltage, BatteryCurrent, BatteryTemperature, SID)) {

    // If main battery
    if (BatteryInstance == 0.0) {
      Serial.printf("MainBattery: (Voltage: %3.1f V, Current: %3.1f A, Temp: %3.1f C)\n", BatteryVoltage, BatteryCurrent, KelvinToC(BatteryTemperature));

      mainBattery.clearFields(); 

      mainBattery.addField("voltage", BatteryVoltage);
      mainBattery.addField("current", BatteryCurrent);
      mainBattery.addField("temperature", KelvinToC(BatteryTemperature));
      mainBattery.addField("power", BatteryVoltage * BatteryCurrent);

      Serial.print("Writing: ");
      Serial.println(client.pointToLineProtocol(mainBattery));

      if (wifiMulti.run() != WL_CONNECTED) {
        Serial.println("Wifi connection lost");
      }

      if (!client.writePoint(mainBattery)) {
        Serial.print("InfluxDB write failed: ");
        Serial.println(client.getLastErrorMessage());
      }

      return;
    }

    // If starter battery
    if (BatteryInstance == 1.0) {
      Serial.printf("Starter Battery: (Voltage: %3.1f V, Current: %3.1f A, Temp: %3.1f C)\n", BatteryVoltage, BatteryCurrent, KelvinToC(BatteryTemperature));

      starterBattery.clearFields();

      starterBattery.addField("voltage", BatteryVoltage);
      starterBattery.addField("current", BatteryCurrent);
      starterBattery.addField("temperature", KelvinToC(BatteryTemperature));
      starterBattery.addField("power", BatteryVoltage * BatteryCurrent);

      Serial.print("Writing: ");
      Serial.println(client.pointToLineProtocol(starterBattery));

      if (wifiMulti.run() != WL_CONNECTED) {
        Serial.println("Wifi connection lost");
      }

      if (!client.writePoint(starterBattery)) {
        Serial.print("InfluxDB write failed: ");
        Serial.println(client.getLastErrorMessage());
      }

      return;
    }
  }
}

void handleFluidTanks(const tN2kMsg &N2kMsg) {
  unsigned char Instance;
  tN2kFluidType FluidType;
  double Level;
  double Capacity;

  if (ParseN2kPGN127505(N2kMsg, Instance, FluidType, Level, Capacity)) {
    Serial.printf("Fluid: Instance: %3.1f , FluiDtype: %3.1f , Capacity: %3.1f \n", Instance, FluidType, Level, Capacity);
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


//*****************************************************************************
void loop() {

  if (!online) {
    online = connectWifi();
  }

  NMEA2000.ParseMessages();

  CheckSourceAddressChange();

  // Dummy to empty input buffer to avoid board to stuck with e.g. NMEA Reader
  if ( Serial.available() ) {
    Serial.read();
  }
}