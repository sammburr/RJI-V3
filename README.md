# RJI V3
Router Joystick Interface version 3 — a Teensy 4.1-based hardware interface for controlling video routers via physical buttons and joysticks.

![](https://github.com/sammburr/RJI-V3/blob/35c30b4bf3e6ec1ab24c7fa220e7dccab9611f11/web-interface.png)

## Features
- **Router Protocols**: VideoHub, SWP-08, TSL 3.1
- **12 GPI buttons** with configurable source routing
- **6 engineer positions** with latch/momentary logic
- **Web interface** for configuration and monitoring via embedded HTTP/WebSocket server
- **Multi-client WebSocket** support (up to 4 simultaneous connections)
- **OTA firmware updates** via web interface

## Build
Built with PlatformIO targeting Teensy 4.1.

```bash
pio run                      # Build firmware
pio run --target upload      # Upload to Teensy
pio device monitor --baud 115200  # Monitor serial output
```

## Web Interface
Connect to the device IP in a browser to access:
- **Position** — Engineer destination/source monitoring
- **GPI Patch** — Button-to-source and button-to-engineer assignment
- **Network** — IP, gateway, subnet, DHCP, router protocol and connection settings
- **Firmware** — OTA firmware update via .hex file upload
