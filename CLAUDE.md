# CLAUDE.md - Project Guide for RJI-V3

## Project Overview
Router Joystick Interface V3 (RJI-V3) - A Teensy 4.1-based hardware interface for controlling video routers via physical buttons/joysticks. Supports VideoHub, SWP-08, and TSL 3.1 protocols.

## Build Commands
```bash
# Build firmware
pio run

# Upload to Teensy
pio run --target upload

# Clean build
pio run --target clean

# Monitor serial output
pio device monitor --baud 115200
```

## Architecture

### Hardware
- **MCU**: Teensy 4.1
- **Network**: Built-in Ethernet via QNEthernet library
- **Inputs**: 12 GPI buttons + reset button

### Source Files (in `RJI/` or `src/` - symlinked)

| File | Purpose |
|------|---------|
| `RJI.ino` | Main entry point, setup/loop, WebSocket message handling |
| `Ethernet.h` | Network stack, web server, WebSocket server, router client, embedded HTML/CSS/JS |
| `Settings.h` | EEPROM settings storage and retrieval |
| `Logic.h` | Button press logic, engineer routing logic |
| `Buttons.h` | Physical button polling and debouncing |
| `Debug.h` | Serial debug macros (`info()`, `err()`) |
| `RouterProtocol.h` | Base class for router protocols |
| `VideoHubProtocol.h` | Blackmagic VideoHub protocol implementation |
| `SWP08Protocol.h` | Pro-Bel/Grass Valley SWP-08 protocol |
| `TSL31Protocol.h` | TSL 3.1 tally protocol |
| `OTA.h` | Over-the-air firmware update support |

### Key Libraries
- `QNEthernet` - Ethernet stack for Teensy 4.1
- `WebSockets2_Generic` - WebSocket server
- `ArduinoJson` - JSON parsing for WebSocket messages
- `FlasherX` - OTA firmware flashing

## Settings That Require Reboot
These settings only take effect after device restart:
- Interface IP, Gateway, Subnet
- DHCP toggle
- Web Server Port

Settings that take effect immediately:
- Router IP/Port
- Router Protocol
- SWP-08 Level
- Button/Engineer configurations

## Important Patterns

### Debug Logging
```cpp
info("Message: ", variable);  // Prints with timestamp and file location
err("Error message");         // Error format
```

### WebSocket Messages
JSON array format: `["message-type", arg1, arg2, ...]`
- `settings` - Full settings dump
- `conn-stat` - WebSocket connection status
- `router-stat` - Router connection status
- `rts` - Route update (dest, source)
- `gpi` - GPI button state

### Non-Blocking Design
The main loop must remain non-blocking. Avoid:
- `delay()` in loop (except tiny delays for flush)
- Blocking socket operations
- Long-running operations without yielding

## Known Issues & Fixes

### DHCP Wait Loop
The DHCP wait must check for `0.0.0.0`, not `INADDR_NONE` (which is `255.255.255.255`):
```cpp
// Correct
if (currentIP[0] != 0 || currentIP[1] != 0 || currentIP[2] != 0 || currentIP[3] != 0)

// Wrong - exits immediately
while (Ethernet.localIP() == INADDR_NONE)
```

### WebSocket Keepalive
Do NOT use `wsClient.ping()` - it causes ~9 second blocking timeout in WebSockets2_Generic. Use application-level keepalive messages instead:
```cpp
sendMessage("[\"conn-stat\", true]");
```

### Router Connection
`EthernetClient::connect()` is blocking. The connection state machine handles reconnection non-blocking after initial setup.

## Web Interface
The web UI is embedded in `Ethernet.h` as PROGMEM strings (`webpageA`). It includes:
- Status bar showing WebSocket/Router connection state
- GPI button grid for testing
- Network settings tab (with reboot indicators)
- Engineer/button configuration
- OTA firmware update tab
