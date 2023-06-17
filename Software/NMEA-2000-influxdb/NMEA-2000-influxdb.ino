#define DEVICE "ESP32"

#define ESP32_CAN_TX_PIN GPIO_NUM_5  // Set CAN TX port to 5 
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
#include <Preferences.h>
#include <NMEA2000_CAN.h>
#include <N2kMessages.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

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

bool online = false;

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


//*****************************************************************************
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
  }
}


//*****************************************************************************
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

    mainBattery.clearFields(); 

    mainBattery.addField("voltage", BatteryVoltage);
    mainBattery.addField("current", BatteryCurrent);
    mainBattery.addField("temperature", KelvinToC(BatteryTemperature));
    mainBattery.addField("power", BatteryVoltage * BatteryCurrent);
    
    if (!online) {
      // If not online
      Serial.println("Wifi connection lost");
    }

    // If online 

    // Write to influx
    client.pointToLineProtocol(starterBattery);

    // Catch errors
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
    
    if (!online) {
      // If not online
      Serial.println("Wifi connection lost");
    }

    // If online 

    // Write to influx
    client.pointToLineProtocol(starterBattery);

    // Catch errors
    if (!client.writePoint(starterBattery)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());
    }

    return;
  }
  
}

// Writes to the SD-Card in Json format
void writeToBuffer(String time, int data) {
  // Read existing json from SD-Card
}

// Writes entire data from SD-Card to influxdb
void writeFromBuffer() {

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

  // If not online - try connecting to a wifi
  if (!online) {
    online = connectWifi();
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

// ----------------- File System -----------------

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if(!root){
    Serial.println("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if(levels){
        listDir(fs, file.name(), levels -1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char * path){
  Serial.printf("Creating Dir: %s\n", path);
  if(fs.mkdir(path)){
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char * path){
  Serial.printf("Removing Dir: %s\n", path);
  if(fs.rmdir(path)){
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
      Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char * path){
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if(file){
    len = file.size();
    size_t flen = len;
    start = millis();
    while(len){
      size_t toRead = len;
      if(toRead > 512){
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }


  file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for(i=0; i<2048; i++){
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}