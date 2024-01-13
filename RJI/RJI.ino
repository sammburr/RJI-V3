#include <ArduinoJson.h>
#include <string.h>


#include "Settings.h"
#include "Debug.h"
#include "Buttons.h"
#include "Ethernet.h"


void (*resetTeensy) (void) = 0; // A function defined at memory location zero causes the arduino to
                                // reboot
void buttonCallback(int, bool);


Button resetButton(ResetPin, buttonCallback);


void setup() {
  Debug.startSerial();
  DebugLight.red();

  Debug.printTitle("TEENSY SETUP");

  Settings.printSettings();

  byte resetFlag[1];
  Settings.read(resetFlag, Var_ResetInterface_Size, Var_ResetInterface);
  if(*resetFlag) {
    // Write default values to Settings
    writeDefaults();

  }

  info("Starting Buttons...");
  startButtons(buttonCallback);

  byte dhcpFlag[1];
  Settings.read(dhcpFlag, Var_DHCPToggle_Size, Var_DHCPToggle);

  byte ip[4];

  if(*dhcpFlag) {
    info("Starting ethernet with DHCP...");

    // Start with DHCP
    Network.startEthernet();
    // Reset dhcp flag
    dhcpFlag[0] = 0;
    Settings.write(dhcpFlag, Var_DHCPToggle_Size, Var_DHCPToggle);
    // Write the new ip to settings
    ip[0] = Network.ip[0];
    ip[1] = Network.ip[1];
    ip[2] = Network.ip[2];
    ip[3] = Network.ip[3];
    Settings.write(ip, Var_InterfaceIP_Size, Var_InterfaceIP);

  } else {
    info("Starting ethernet with a static IP...");

    Settings.read(ip, Var_InterfaceIP_Size, Var_InterfaceIP);  
    Network.startEthernet(ip);

  }

  info("Starting webserver...");

  uint16_t port;
  Settings.read_16bit(port, Var_WebServerPort);
  Network.startWebServer(port);

  info("Starting WebSocket server...");

  Settings.read_16bit(port, Var_WebSocketPort);
  Network.startWebSocketServer(port, websocketMessageCallback);

  info("Connecting to VideoHub...");

  Settings.read(ip, Var_VideoHubIP_Size, Var_VideoHubIP);
  Settings.read_16bit(port, Var_VideoHubPort);
  Network.connectToVideoHub(ip, port);

  DebugLight.green();
  Debug.printTitle("TEENSY SETUP DONE");

}


void loop() {
  resetButton.poll();
  pollButtons();
  Network.pollWebServer();
  Network.pollWebSocketServer();

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


void buttonCallback(int _pin, bool _state) {
  info(_pin, ", ", _state);
  if (_pin == ResetPin && _state == true) {
    // Reset the device
    resetInterface();

  } else {
    std::string message = "[\"gpi\",\"gpi-" + std::to_string(_pin - 28) + "\"," + std::to_string(_state) + "]";
    Network.sendMessage(Network.webSocketClient, message.c_str());

  }

}


void websocketMessageCallback(WebsocketsClient& _client, WebsocketsMessage _message) {
  info("Got a message: ", _message.data());

  // Parse to a json
  JsonDocument json;
  deserializeJson(json, _message.data());

  // The header of a given message is allways a string
  std::string header = json[0];

  // Prepare for a large if statment, a condition for each setting in the interface
  // There is a better way of doing this I hope
  if(header == "interface-ip") {
    info("Writing to InterfaceIP...");
    byte ip[4] = {json[1], json[2], json[3], json[4]};
    Settings.write(ip, Var_InterfaceIP_Size, Var_InterfaceIP);

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

  } else if(header == "videohub-ip") {
    info("Writing to VideoHubIP...");
    byte ip[4] = {json[1], json[2], json[3], json[4]};
    Settings.write(ip, Var_VideoHubIP_Size, Var_VideoHubIP);

  } else if(header == "videohub-port") {
    info("Writing to VideoHubPort...");
    Settings.write_16bit(json[1], Var_VideoHubPort);

  } else if(header == "reset") {
    info("Restarting the Interface...");
    resetTeensy();

  }


  Settings.printSettings();

}