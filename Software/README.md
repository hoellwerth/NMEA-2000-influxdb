# Software

## Table of Contents

- [Software](#software)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
  - [Usage](#usage)
  - [How it works](#how-it-works)

## Introduction

This scipt will read all the data in the NMEA-2000 (N2k) BUS and will send it to an InfluxDB database. As this thing will be mainly operated in a marine environment, it will save data on an SD-Card as the boat will not always be connected to the internet and then write it to the database when it is connected.