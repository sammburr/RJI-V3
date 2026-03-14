#define FW_VERSION "3.4.2"

#include <ArduinoJson.h>
#include <string.h>

#include "Settings.h"
#include "Debug.h"
#include "Buttons.h"
#include "Ethernet.h"
#include "Logic.h"
#include "OTA.h"

void (*resetTeensy) (void) = 0; // A function defined at memory location zero causes the arduino to
                                // reboot
void buttonCallback(int, bool);

// Network poll callback for OTA - keeps WebSocket alive during flash operations
void otaNetworkPoll() {
  Network.pollWebSocket();
}

Button resetButton(ResetPin, buttonCallback);


void setup() {
  DebugLight.red();
  Debug.startSerial();

  Debug.printTitle("Video Walrus Router Joystick Interface");
  info("RJI Firmware v", FW_VERSION);

  Settings.printSettings();

  attachInterrupt(digitalPinToInterrupt(ResetPin), resetButtonFun, INPUT_PULLUP);

  byte resetFlag[1];
  Settings.read(resetFlag, Var_ResetInterface_Size, Var_ResetInterface);
  if(*resetFlag) {
    // Write default values to Settings
    writeDefaults();
  }

  // info("Hold the reset button to load defaults...");
  // // Perform a small delay to give the user time to press the reset button
  // delay(2000);

  info("Starting Buttons...");
  startButtons(buttonCallback);

  // if (!digitalRead(ResetPin))
  //   resetInterface();

  byte dhcpFlag[1];
  Settings.read(dhcpFlag, Var_DHCPToggle_Size, Var_DHCPToggle);

  byte ip[4];
  byte gw[4];
  byte sub[4];

  // Read saved IP to check if it's valid
  Settings.read(ip, Var_InterfaceIP_Size, Var_InterfaceIP);

  info("DHCP flag: ", *dhcpFlag);
  info("Saved IP: ", ip[0], ".", ip[1], ".", ip[2], ".", ip[3]);

  // Use DHCP if flag is set OR if saved IP is 0.0.0.0 (invalid)
  bool usedhcp = *dhcpFlag || (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);

  if(usedhcp) {
    info("Starting ethernet with DHCP...");

    // Start with DHCP
    Network.startEthernet();

    // Only save the IP if DHCP succeeded
    if(Network.ip[0] != 0 || Network.ip[1] != 0 || Network.ip[2] != 0 || Network.ip[3] != 0) {
      ip[0] = Network.ip[0];
      ip[1] = Network.ip[1];
      ip[2] = Network.ip[2];
      ip[3] = Network.ip[3];
      Settings.write(ip, Var_InterfaceIP_Size, Var_InterfaceIP);
      info("Saved DHCP IP to settings: ", ip[0], ".", ip[1], ".", ip[2], ".", ip[3]);
    } else {
      err("DHCP failed - IP not saved");
    }

  } else {
    info("Starting ethernet with a static IP...");

    Settings.read(gw, Var_InterfaceGW_Size, Var_InterfaceGW);
    Settings.read(sub, Var_InterfaceSub_Size, Var_InterfaceSub);
    Network.startEthernet(ip, gw, sub);

  }

  info("Starting webserver...");

  uint16_t port;
  Settings.read_16bit(port, Var_WebServerPort);
  Network.startWebServer(port);

  info("Starting mDNS...");
  Network.startMDNS();

  info("Starting WebSocket server...");

  Settings.read_16bit(port, Var_WebSocketPort);
  Network.startWebSocketServer(port, websocketMessageCallback);

  // Set up OTA network poll callback to keep WebSocket alive during flash operations
  OTA.setNetworkPollCallback(otaNetworkPoll);

  info("Setting up router protocol...");

  // Read and set protocol type
  byte protocolType[1];
  Settings.read(protocolType, Var_RouterProtocol_Size, Var_RouterProtocol);
  Network.setProtocol((RouterProtocolType)(*protocolType));

  // Read and set SWP-08 level
  byte swp08Level[1];
  Settings.read(swp08Level, Var_SWP08Level_Size, Var_SWP08Level);
  Network.setSWP08Level(*swp08Level);

  // Set up route filtering callback for late ACK detection
  RouterProtocol::shouldFilterRoute = [](uint16_t dest, uint16_t source) -> bool {
    return Logic.shouldFilterRoute(dest, source);
  };

  info("Connecting to Router...");

  Settings.read(ip, Var_VideoHubIP_Size, Var_VideoHubIP);
  Settings.read_16bit(port, Var_VideoHubPort);
  Network.connectToRouter(ip, port);

  DebugLight.green();
  Debug.printTitle("TEENSY SETUP DONE");

}


void loop() {
  //resetButton.poll();
  pollButtons();

  // Poll web server and WebSocket
  Network.pollWebServer();
  Network.pollWebSocket();

  // Skip router polling during OTA to prevent blocking WebSocket
  if (!OTA.updateInProgress) {
    Network.pollRouter();
  }
  Network.pollWebSocketKeepalive();

  // Check for pending OTA flash operation
  if (OTA.hasPendingFlash()) {
    info("OTA: Executing firmware flash...");
    Network.sendMessage("[\"fw-flashing\"]");
    delay(100);  // Allow WebSocket message to send
    OTA.executeFlash();  // This will reboot the device
  }

}


// Write default values to Settings
void writeDefaults() {
  info("Writing default Settings...");
  Settings.writeDefaults();
  Settings.printSettings();

}


// Reset the device to defaults and use DHCP next time the device
// connects the Ethernet
void resetInterface() {
  info("Resetting to defaults...");
  DebugLight.amber();
  // Write the reset flag setting
  byte flag[] = {1};
  Settings.write(flag, Var_ResetInterface_Size, Var_ResetInterface);
  info("Waiting for teensy to restart...");
  resetTeensy();

}


volatile unsigned long startTime;

void resetButtonFun() {

  delay(3000);
  //is reset button still held after delay?
  if(!digitalRead(ResetPin)) {
    info("Reseting The Interface!");
    resetInterface();
  }
  else {
    info("Rebooting The Interface!");
    resetTeensy();
  }

}

void buttonCallback(int _pin, bool _state) {
  int gpiNum = _pin - 28;
  info("GPI ", gpiNum, _state ? " DOWN" : " UP");
  std::string message = "[\"gpi\",\"gpi-" + std::to_string(gpiNum) + "\"," + std::to_string(_state) + "]";
  Network.sendMessage(message.c_str());
  Logic.parseButton(_pin, _state);
}


void websocketMessageCallback(const char* data, size_t len) {
  // Don't log fw-data messages to reduce serial spam during OTA
  if (strstr(data, "fw-data") == nullptr) {
    info("Got a message: ", data);
  }

  // Parse to a json
  JsonDocument json;
  deserializeJson(json, data);

  // The header of a given message is allways a string
  std::string header = json[0];

  // Prepare for a large if statment, a condition for each setting in the interface
  // There is a better way of doing this I hope
  if(header == "interface-ip") {
    info("Writing to InterfaceIP...");
    byte ip[4] = {json[1], json[2], json[3], json[4]};
    Settings.write(ip, Var_InterfaceIP_Size, Var_InterfaceIP);

  } else if(header == "interface-sub") {
    info("Writing to InterfaceSub...");
    byte sub[4] = {json[1], json[2], json[3], json[4]};
    Settings.write(sub, Var_InterfaceSub_Size, Var_InterfaceSub);

  } else if(header == "interface-gw") {
    info("Writing to InterfaceGW...");
    byte gw[4] = {json[1], json[2], json[3], json[4]};
    Settings.write(gw, Var_InterfaceGW_Size, Var_InterfaceGW);

  } else if(header == "interface-web-port") {
    info("Writing to InterfaceWebPort...");
    Settings.write_16bit(json[1], Var_WebServerPort);

  } else if(header == "interface-dhcp") {
    info("Writing to DHCPToggle: ", json[1].as<bool>());
    byte value[1];

    if(json[1].as<bool>()) {
      value[0] = {1};
    
    } else {
      value[0] = {0};

    }

    Settings.write(value, Var_DHCPToggle_Size, Var_DHCPToggle);

  } else if(header == "videohub-ip" || header == "router-ip") {
    info("Writing to RouterIP...");
    byte ip[4] = {json[1], json[2], json[3], json[4]};
    Settings.write(ip, Var_VideoHubIP_Size, Var_VideoHubIP);

  } else if(header == "videohub-port" || header == "router-port") {
    info("Writing to RouterPort...");
    Settings.write_16bit(json[1], Var_VideoHubPort);

  } else if(header == "router-protocol") {
    info("Writing to RouterProtocol...");
    byte protocol[1] = {(byte)json[1].as<int>()};
    Settings.write(protocol, Var_RouterProtocol_Size, Var_RouterProtocol);
    Network.setProtocol((RouterProtocolType)(protocol[0]));

  } else if(header == "swp08-level") {
    info("Writing to SWP08Level...");
    byte level[1] = {(byte)json[1].as<int>()};
    Settings.write(level, Var_SWP08Level_Size, Var_SWP08Level);
    Network.setSWP08Level(level[0]);

  } else if(header == "reset") {
    info("Restarting the Interface...");
    resetTeensy();

  } else if(header == "eng_0") {
    info("Writing eng_0 (", json[4].as<const char*>(), ") settings...");
    Settings.setEngineer(0, json);

  } else if(header == "eng_1") {
    info("Writing eng_1 (", json[4].as<const char*>(), ") settings...");
    Settings.setEngineer(1, json);

  } else if(header == "eng_2") {
    info("Writing eng_2 (", json[4].as<const char*>(), ") settings...");
    Settings.setEngineer(2, json);

  } else if(header == "eng_3") {
    info("Writing eng_3 (", json[4].as<const char*>(), ") settings...");
    Settings.setEngineer(3, json);

  } else if(header == "eng_4") {
    info("Writing eng_4 (", json[4].as<const char*>(), ") settings...");
    Settings.setEngineer(4, json);

  } else if(header == "eng_5") {
    info("Writing eng_5 (", json[4].as<const char*>(), ") settings...");
    Settings.setEngineer(5, json);

  } else if(header == "button_0") {
    info("Writing button_0 source...");
    Settings.write_16bit(json[1].as<uint16_t>(), Var_Button_0_Source);

  } else if(header == "button_1") {
    info("Writing button_1 source...");
    Settings.write_16bit(json[1].as<uint16_t>(), Var_Button_1_Source);

  } else if(header == "button_2") {
    info("Writing button_2 source...");
    Settings.write_16bit(json[1].as<uint16_t>(), Var_Button_2_Source);

  } else if(header == "button_3") {
    info("Writing button_3 source...");
    Settings.write_16bit(json[1].as<uint16_t>(), Var_Button_3_Source);

  } else if(header == "button_4") {
    info("Writing button_4 source...");
    Settings.write_16bit(json[1].as<uint16_t>(), Var_Button_4_Source);

  } else if(header == "button_5") {
    info("Writing button_5 source...");
    Settings.write_16bit(json[1].as<uint16_t>(), Var_Button_5_Source);

  } else if(header == "button_6") {
    info("Writing button_6 source...");
    Settings.write_16bit(json[1].as<uint16_t>(), Var_Button_6_Source);

  } else if(header == "button_7") {
    info("Writing button_7 source...");
    Settings.write_16bit(json[1].as<uint16_t>(), Var_Button_7_Source);

  } else if(header == "button_8") {
    info("Writing button_8 source...");
    Settings.write_16bit(json[1].as<uint16_t>(), Var_Button_8_Source);

  } else if(header == "button_9") {
    info("Writing button_9 source...");
    Settings.write_16bit(json[1].as<uint16_t>(), Var_Button_9_Source);

  } else if(header == "button_10") {
    info("Writing button_10 source...");
    Settings.write_16bit(json[1].as<uint16_t>(), Var_Button_10_Source);

  } else if(header == "button_11") {
    info("Writing button_11 source...");
    Settings.write_16bit(json[1].as<uint16_t>(), Var_Button_11_Source);

  }  else if(header == "video_hub_retry" || header == "router_retry") {
    info("Trying to connect to the router...");

    byte ip[4];
    uint16_t port;

    Settings.read(ip, Var_VideoHubIP_Size, Var_VideoHubIP);
    Settings.read_16bit(port, Var_VideoHubPort);

    info(ip[0], ".", ip[1], ".",ip[2], ".",ip[3], ":", port);

    Network.reconnectToRouter(ip, port);
    if(json[1].as<bool>())
      Network.autoConnect = true;
    else
      Network.autoConnect = false;

  } else if(header == "gpi_down") {
    buttonCallback(json[1].as<int>() + 28, true);
  } else if(header == "gpi_up") {
    buttonCallback(json[1].as<int>() + 28, false);

  } else if(header == "fw-start") {
    if(OTA.startUpdate()) {
      Network.sendMessage("[\"fw-progress\", 0, 0]");
    } else {
      char errMsg[128];
      snprintf(errMsg, sizeof(errMsg), "[\"fw-error\", \"%s\"]", OTA.lastError.c_str());
      Network.sendMessage(errMsg);
    }

  } else if(header == "fw-data") {
    // Process hex line
    const char* hexLine = json[1].as<const char*>();
    if(!OTA.processHexLine(hexLine)) {
      char errMsg[128];
      snprintf(errMsg, sizeof(errMsg), "[\"fw-error\", \"%s\"]", OTA.lastError.c_str());
      Network.sendMessage(errMsg);
      OTA.abortUpdate();
    }
    // Send progress every 100 lines (but not at 0)
    if(OTA.linesProcessed > 0 && OTA.linesProcessed % 100 == 0) {
      char progressMsg[64];
      snprintf(progressMsg, sizeof(progressMsg), "[\"fw-progress\", %lu, %lu]",
               OTA.linesProcessed, OTA.bytesReceived);
      Network.sendMessage(progressMsg);
    }

  } else if(header == "fw-end") {
    info("OTA: Finishing firmware update...");
    if(OTA.finishUpdate()) {
      Network.sendMessage("[\"fw-ready\"]");
      // The actual flash will happen in loop() when hasPendingFlash() is true
    } else {
      char errMsg[128];
      snprintf(errMsg, sizeof(errMsg), "[\"fw-error\", \"%s\"]", OTA.lastError.c_str());
      Network.sendMessage(errMsg);
    }

  } else if(header == "fw-abort") {
    info("OTA: Aborting firmware update...");
    OTA.abortUpdate();

  }

}
