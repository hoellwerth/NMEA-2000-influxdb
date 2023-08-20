# Software

## Table of Contents

- [Software](#software)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
  - [Usage](#usage)
  - [Libraries](#libraries)
  - [How it works](#how-it-works)

## Introduction

This scipt is based on PlatformIO and will read all the data in the NMEA-2000 (N2k) BUS and will send it to an InfluxDB database. As this thing will be mainly operated in a marine environment, it will save data on an SD-Card as the boat will not always be connected to the internet and then write it to the database when it gets reconnected to a Wifi in harbor.

## Usage

To use this script, you will need to have a running InfluxDB instance. You can use the docker-compose combination of InfluxDB, Grafana and Telegraf from [here](https://github.com/nicolargo/docker-influxdb-grafana). You will also need to have a running N2k BUS. If you want to know which Hardware in particular, you can check out the [Hardware](../Hardware/README.md) section.

## Libraries

This script uses the following libraries:

- [ArduinoJson](https://arduinojson.org/)
- [InfluxDB-Client-for-Arduino](https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino)
- [NMEA2000](https://github.com/ttlappalainen/NMEA2000)
- [NMEA2000_esp32](https://github.com/ttlappalainen/NMEA2000_esp32)
- [RTCLib](https://github.com/adafruit/RTClib?utm_source=platformio&utm_medium=piohome)
- [Time](https://github.com/PaulStoffregen/Time?utm_source=platformio&utm_medium=piohome)

The relevant section in **platformio.ini**:
```
lib_deps = 
	tobiasschuerg/ESP8266 Influxdb@^3.13.1
	ttlappalainen/NMEA2000-library@^4.18.7
	ttlappalainen/NMEA2000_esp32@^1.0.3
	bblanchon/ArduinoJson@^6.21.2
	Wire
	paulstoffregen/Time@^1.6.1
	adafruit/RTClib@^2.1.1
```
