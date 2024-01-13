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

  info("Starting ethernet...");

  byte ip[4];
  Settings.read(ip, Var_InterfaceIP_Size, Var_InterfaceIP);  
  Network.startEthernet(ip);

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
  if (_pin == ResetPin && _state == true) {
    // Reset the device
    resetInterface();

  }

}


void websocketMessageCallback(WebsocketsClient& _client, WebsocketsMessage _message) {
  info("Got a message: ", _message.data());

  // Parse to a json
  DynamicJsonDocument json(1024);
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

  }

  Settings.printSettings();

}