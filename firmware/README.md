This directory contains firmware for auxiliary microcontrollers

#### 3D Printer auxiliary I/O controller ####

This contains the firmware requitred to use one of those cheap STM32F103 Minimal System Development Boards as fan/heater controller.
This firmware contains an interface to NTC sensors, Steinhart-Hart for temperature calculations, a full PID controller per output, and PDM outputs for DC and AC channels.
Interface to LinuxCNC is though USB using custom HID device class.
