# psu_wifi_bridge

## So what is it ?
This project is my first attempt at building a smart power supply for the original Playstation 1. The ESP32 monitors the power button(momentary switch), which then turns the console's buck converters on and off. The ESP32 serves up a basic web interface which can also toggle the Power and Reset signals. With the ability to control power remotely, and with the microcontroller doing so little, I decided the ESP could also be tasked with acting as a tcp-serial bridge to make further remote testing possible.

## Why ?
I regularly work around my console with the top shell removed which can be very dangerous due to the exposed power supply and mains voltage. This conversion drops everything to low voltage DC current well before it reaches the console, with the added benefit of extra features.

## What isn't it ?
This is in no way meant to be a pro quality replacement psu, such as Will's ps1 pico psu replacement.

## Current status
The power supply works suitably for my needs, but it does have some quirks. There is basically no filtering or smoothing on the power, so a high quality AC-DC supply must be used or you will get fuzzy AV output, as well as other erratic console behavior. At the moment I am using a Dell laptop charger which has been tested using an oscilliscope to verify smooth current delivery. The laptop charger is unfortunately quite a bit higher voltage than I need, so the console cannot be put under heavy load or the buck converters will build a significant amount of heat, and so I am running my console with no cd drive. The tcp-serial bridge of this project is still a work in progress, along with it's companion software NOTPSXSerial. NOTPSXSerial(or nops) is a brilliant serial transfer software by sickle which is meant to be paired with Unirom. The tcp-serial bridge of NotPSXSerial is also a work in progress and can be found in my fork of the project [https://github.com/johnbaumann/NOTPSXSerial](https://github.com/johnbaumann/NOTPSXSerial)
