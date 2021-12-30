# wifi_bridge

## So what is it ?
ESP32 Serial Bridge over Wifi/TCP, for use with Unirom + nops/NOTPSXSerial

## Hardware connections
Hook the PS1's TX/RD to the ESP's RX2/TX2

## How to use
Add your Wifi AP Details to the SSID/PASS definitions in wifi_client.c
Point nops to the esp's IP address and port 6699 in ip:port format
Fast baud rate detection is not currently supported(can be changed manually, see the main branch for this with web server stuff)

## Current status
The tcp-serial bridge of this project is still a work in progress, along with it's companion software NOTPSXSerial. NOTPSXSerial(or nops) is a brilliant serial transfer software by sickle which is meant to be paired with Unirom. The tcp-serial bridge of NotPSXSerial is also a work in progress and can be found in my fork of the project [https://github.com/johnbaumann/NOTPSXSerial](https://github.com/johnbaumann/NOTPSXSerial)
