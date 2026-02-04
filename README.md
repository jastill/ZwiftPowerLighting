# ZwiftPowerLighting
Make my zwift setup light up with the power

![F822EC98-533F-4C9F-9171-1F8039DD14D0_1_105_c](https://github.com/user-attachments/assets/0fef23d2-81ad-4663-85b0-59e3f1eba43f)

## Goals

The goals here is to create a bridge using a microcontroller to connect to Zwift and control my lights based on the power output. 

The power output is available from the trainer, in my case this is Kickr Core V1. 

The microcontroller will act as a bridge between the trainer and the lights. It will read the power output from the trainer and control the lights based on the power output.

The power output has a range of colors per power 

There is an existing mobile app that will bridge the power from the trainer to Philips Hue lights. More details can be found at the DC Rainmaker Blog.

https://www.dcrainmaker.com/2026/01/philips-hue-lights-with-zwift.html

## Initial Step

Create code for a Raspberry Pi Pico W to read the power from the power trainer and control an LED strip based on the power output. The WS2812B LED Strip will be connected to the Raspberry Pi Pico W directly.

Read the power output from the trainer and control the lights based on the power output using the colors in the Power Zones.

The code can be either micro python or C++.

## Components

- Raspberry Pi Pico W
- WS2812B LED Strip

## Power Zones

There are 6 power zones in Zwift. 

1- < 60% White
2- 60-75% Blue
3- 76-89% Green
4- 90-104% Yellow
5- 105-118% Orange
6- > 118% Red

## Sequence

On startup the following sequence will be executed:

1. Turn on the LED strip and cycle through the colors in the Power Zones with a 500mS interval.
2. Turn the LEDs white.
3. Scan for the trainer using BLE, the current name is KICKR CORE 5D21
4. Connect to the trainer. Once connected, flash the LEDs green for 3 seconds with a 500mS interval.

## Constraints

In the code the following constraints apply:

- Allow the number of LEDs to be configurable.
- Allow the power zones to be configurable.
- Allow the BLE name of the trainer to be configurable.


## Philips Hue

The Philips Hue bridge will be used to control the lights based on the power output. The Philips Hue bridge will be connected to the Raspberry Pi Pico W via WiFi.

https://developers.meethue.com/develop/get-started-2/


