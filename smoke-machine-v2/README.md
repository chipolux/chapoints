# ESP-12E Smoke Machine Controller v2

New smoke machine controller codebase using a modern version of the
ESP8266 RTOS SDK which is not supported by PlatformIO.

## Requirements

* [Follow these instructions.](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/get-started/index.html#setup-toolchain)
    * When checking out the RTOS SDK we are using the `release/v3.4` branch.
    * Up to and through the `make menuconfig` step in this directory so you can
      choose the correct port name for your machine.


## Build and Deploy

Inside a mingw console run `make flash monitor` to build and flash the firmware
and attach a serial monitor.


## Dev Notes

[SDK Reference](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/api-reference/index.html)
[Examples](https://github.com/espressif/ESP8266_RTOS_SDK/tree/release/v3.4/examples)
