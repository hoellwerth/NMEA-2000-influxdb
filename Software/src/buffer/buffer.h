#include "Arduino.h"
#include <InfluxDbClient.h>
#include <InfluxDbClient.h>

// SD-Card helper functions
#include "../src/sDCard/sdCard.h"

#define INFLUXDB_URL "http://cloud.hoellwerth.eu:8086"
#define INFLUXDB_TOKEN "h8S4V-g_bvHkhf11t276cOQ8-pxJhUtPGAiF9He1sDyU3Lu1Bmnwjx0__5eb2kIoWn2-iWB8Z3ofQQa9u3G1VA=="
#define INFLUXDB_ORG "554b5bb375690e45"
#define INFLUXDB_BUCKET "boat"

// Setup Influxdb Client
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

// Create influxdb sensor points
Point mainBattery("main_battery");
Point starterBattery("starter_battery");
Point general("general");

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
  
  // Main battery

  // Voltage

  const char* rawMainBatteryVoltageTimestamps = readFile(SD, "/main_battery_voltage_timestamps.txt");
  const char* rawMainBatteryVoltageData = readFile(SD, "/main_battery_voltage_data.txt");

  // Iterate through timestamps every 16 characters is a new timestamp

  for (int i = 0; i < strlen(rawMainBatteryVoltageTimestamps); i += 16) {
    char timestamp[15];
    strncpy(timestamp, rawMainBatteryVoltageTimestamps + i, 14);
    timestamp[14] = '\0';

    char data[16];
    strncpy(data, rawMainBatteryVoltageData + i, 14);
    data[14] = '\0';

    Serial.print("Timestamp: ");
    Serial.println(timestamp);

    Serial.print("Data: ");
    Serial.println(data);

    mainBattery.clearFields();

    const char* finalTimestamp = timestamp;

    mainBattery.setTime(finalTimestamp);

    float voltage = atof(data);
    voltage = (float)((int)(voltage * 100 + .5)) / 100;

    mainBattery.addField("voltage", voltage);

    // Write to influx
    client.pointToLineProtocol(mainBattery);

    // Catch errors
    if (!client.writePoint(mainBattery)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());

      writeToBuffer(atof(timestamp), data, "/main_battery_voltage");
    }
  }

  // Current

  const char* rawMainBatteryCurrentTimestamps = readFile(SD, "/main_battery_current_timestamps.txt");
  const char* rawMainBatteryCurrentData = readFile(SD, "/main_battery_current_data.txt");

  // Iterate through timestamps every 15 characters is a new timestamp

  for (int i = 0; i < strlen(rawMainBatteryCurrentTimestamps); i+=15) {
    char timestamp[15];
    strncpy(timestamp, rawMainBatteryCurrentTimestamps + i, 14);
    timestamp[14] = '\0';

    char data[15];
    strncpy(data, rawMainBatteryCurrentData + i, 14);
    data[14] = '\0';

    Serial.print("Timestamp: ");
    Serial.println(timestamp);

    Serial.print("Data: ");
    Serial.println(data);

    mainBattery.clearFields();

    const char* finalTimestamp = timestamp;

    mainBattery.setTime(finalTimestamp);

    float current = atof(data);
    current = (float)((int)(current * 100 + .5)) / 100;

    mainBattery.addField("current", current);

    // Write to influx
    client.pointToLineProtocol(mainBattery);

    // Catch errors
    if (!client.writePoint(mainBattery)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());

      writeToBuffer(atof(timestamp), data, "/main_battery_current");
    }
  }

  // Temperature

  const char* rawMainBatteryTemperatureTimestamps = readFile(SD, "/main_battery_temperature_timestamps.txt");
  const char* rawMainBatteryTemperatureData = readFile(SD, "/main_battery_temperature_data.txt");

  // Iterate through timestamps every 15 characters is a new timestamp

  for (int i = 0; i < strlen(rawMainBatteryTemperatureTimestamps); i+=15) {
    char timestamp[15];
    strncpy(timestamp, rawMainBatteryTemperatureTimestamps + i, 14);
    timestamp[14] = '\0';

    char data[15];
    strncpy(data, rawMainBatteryTemperatureData + i, 14);
    data[14] = '\0';

    Serial.print("Timestamp: ");
    Serial.println(timestamp);

    Serial.print("Data: ");
    Serial.println(data);

    mainBattery.clearFields();

    const char* finalTimestamp = timestamp;

    mainBattery.setTime(finalTimestamp);

    float temperature = atof(data);
    temperature = (float)((int)(temperature * 100 + .5)) / 100;


    mainBattery.addField("temperature", temperature);

    // Write to influx
    client.pointToLineProtocol(mainBattery);

    // Catch errors
    if (!client.writePoint(mainBattery)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());

      writeToBuffer(atof(timestamp), data, "/main_battery_temperature");
    }
  }
/*
  // Starter Battery

  // Voltage

  const char* rawStarterBatteryVoltageTimestamps = readFile(SD, "/starter_battery_voltage_timestamps.txt");
  const char* rawStarterBatteryVoltageData = readFile(SD, "/starter_battery_voltage_data.txt");

  // Iterate through timestamps every 16 characters is a new timestamp

  for (int i = 0; i < strlen(rawStarterBatteryVoltageTimestamps); i += 16) {
    char timestamp[15];
    strncpy(timestamp, rawStarterBatteryVoltageTimestamps + i, 14);
    timestamp[14] = '\0';

    char data[16];
    strncpy(data, rawStarterBatteryVoltageData + i, 14);
    data[14] = '\0';

    Serial.print("Timestamp: ");
    Serial.println(timestamp);

    Serial.print("Data: ");
    Serial.println(data);

    starterBattery.clearFields();

    const char* finalTimestamp = timestamp;

    starterBattery.setTime(finalTimestamp);

    float voltage = atof(data);
    voltage = (float)((int)(voltage * 100 + .5)) / 100;

    starterBattery.addField("voltage", voltage);

    // Write to influx
    client.pointToLineProtocol(starterBattery);

    // Catch errors
    if (!client.writePoint(starterBattery)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());

      writeToBuffer(atof(timestamp), data, "/starter_battery_voltage");
    }
  }

  // Current

  const char* rawStarterBatteryCurrentTimestamps = readFile(SD, "/starter_battery_current_timestamps.txt");
  const char* rawStarterBatteryCurrentData = readFile(SD, "/starter_battery_current_data.txt");

  // Iterate through timestamps every 15 characters is a new timestamp

  for (int i = 0; i < strlen(rawStarterBatteryCurrentTimestamps); i+=15) {
    char timestamp[15];
    strncpy(timestamp, rawStarterBatteryCurrentTimestamps + i, 14);
    timestamp[14] = '\0';

    char data[15];
    strncpy(data, rawStarterBatteryCurrentData + i, 14);
    data[14] = '\0';

    Serial.print("Timestamp: ");
    Serial.println(timestamp);

    Serial.print("Data: ");
    Serial.println(data);

    starterBattery.clearFields();

    const char* finalTimestamp = timestamp;

    starterBattery.setTime(finalTimestamp);

    float current = atof(data);
    current = (float)((int)(current * 100 + .5)) / 100;

    starterBattery.addField("current", current);

    // Write to influx
    client.pointToLineProtocol(starterBattery);

    // Catch errors
    if (!client.writePoint(starterBattery)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());

      writeToBuffer(atof(timestamp), data, "/starter_battery_current");
    }
  }

  // Temperature

  const char* rawStarterBatteryTemperatureTimestamps = readFile(SD, "/starter_battery_temperature_timestamps.txt");
  const char* rawStarterBatteryTemperatureData = readFile(SD, "/starter_battery_temperature_data.txt");

  // Iterate through timestamps every 15 characters is a new timestamp

  for (int i = 0; i < strlen(rawStarterBatteryTemperatureTimestamps); i+=15) {
    char timestamp[15];
    strncpy(timestamp, rawStarterBatteryTemperatureTimestamps + i, 14);
    timestamp[14] = '\0';

    char data[15];
    strncpy(data, rawStarterBatteryTemperatureData + i, 14);
    data[14] = '\0';

    Serial.print("Timestamp: ");
    Serial.println(data);

    starterBattery.clearFields();

    const char* finalTimestamp = timestamp;

    starterBattery.setTime(finalTimestamp);

    float temperature = atof(data);
    temperature = (float)((int)(temperature * 100 + .5)) / 100;


    starterBattery.addField("temperature", temperature);

    // Write to influx
    client.pointToLineProtocol(starterBattery);

    // Catch errors
    if (!client.writePoint(starterBattery)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());

      writeToBuffer(atof(timestamp), data, "/starter_battery_temperature");
    }
  }
  
  client.flushBuffer();*/
}