# ESP32 Hammer Controller

Hammer contoller using ESP-IDF for the ESP32 to drive 3 tiny baby stepper motors.


## Requirements

* [Follow these instructions.](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#manual-installation)
    * We want to be able to use the IDF console to run the idf.py tool.


## Setup

Copy `main/secrets.h.template` to `main/secrets.h` and edit it to contain the
correct WiFi connection details.


## Build and Deploy

Open the IDF console, change to this directory, and run `idf.py build flash monitor`
to build the project, flash it to the ESP32 and then attach the serial monitor.

*Note:* If the tool fails to detect the serial port you can pass `-p (PORT)` to
the `flash` and `monitor` commands.
