# CS122A_Project
Custom class project - "Home Security System": Here you can find the code used to program my Atmega1284 and Raspberry Pi Zero W.


# Milestone I & II

## Atmega1284
- Components used on the Atmega1284 MCU (LCD, LED Matrix, Keypad). This code allows for hardware component functionality with their corresponding programmed logic interacting with the system.

- Milestone I code is a Home Security System with the Atmega1284 code setup. This system has an LCD user interface where the user gets updated information on the current status of the system, like armed (on), unarmed (off), motion detected, alarm activated/deactivated, etc. This home security system allows for a password to be set-up that controls the units’ armed/disarmed/alarm status with a keypad. When motion is detected (replaced by a keypad button) the alarm system is activated and prompts the user to deactivate with their set password.

## Raspberry Pi Zero W
- This code is for the Raspberry Pi Zero W. When armed the system automatically turns on the camera module on the pi zero w and video records the frame until motion is detected (using openCV).
