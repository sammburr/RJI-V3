#include <ArduinoJson.h>


#include "Settings.h"
#include "Debug.h"
#include "Buttons.h"
#include "Ethernet.h"


void (*resetTeensy) (void) = 0; // A function defined at memory location zero causes the arduino to
                                // reboot
void buttonCallback(int, bool);


Settings settings;
Button resetButton(ResetPin, buttonCallback);


void setup() {
  Debug.startSerial();
  DebugLight.red();

  Debug.printTitle("TEENSY SETUP");

  settings.printSettings();

  byte resetFlag[1];
  settings.read(resetFlag, Var_ResetInterface_Size, Var_ResetInterface);
  if(*resetFlag) {
    // Write default values to settings
    writeDefaults();

  }

  info("Starting ethernet...");

  byte ip[4];
  settings.read(ip, Var_InterfaceIP_Size, Var_InterfaceIP);  
  Network.startEthernet(ip);

  info("Starting webserver...");

  uint16_t port;
  settings.read_16bit(port, Var_WebServerPort);
  Network.startWebServer(port);

  info("Starting WebSocket server...");

  settings.read_16bit(port, Var_WebSocketPort);
  Network.startWebSocketServer(port, websocketMessageCallback);


  DebugLight.green();
  Debug.printTitle("TEENSY SETUP DONE");

}


void loop() {
  resetButton.poll();
  Network.pollWebServer();
  Network.pollWebSocketServer();

}


// Write default values to settings
void writeDefaults() {
  info("Writing default settings...");
  settings.writeDefaults();
  settings.printSettings();

}


// Reset the device to defaults and use DHCP next time the device
// connects the Ethernet
void resetInterface() {
  info("Resetting to defaults...");
  DebugLight.amber();
  // Write the reset flag setting
  byte flag[] = {1};
  settings.write(flag, Var_ResetInterface_Size, Var_ResetInterface);
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

}