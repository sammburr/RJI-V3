#include <ArduinoJson.h>
#include <string.h>

#include "Settings.h"
#include "Debug.h"
#include "Buttons.h"
#include "Ethernet.h"
#include "Logic.h"

void (*resetTeensy) (void) = 0; // A function defined at memory location zero causes the arduino to
                                // reboot
void buttonCallback(int, bool);

Button resetButton(ResetPin, buttonCallback);


void setup() {
  DebugLight.red();
  Debug.startSerial();

  Debug.printTitle("TEENSY SETUP");

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
    Settings.read(gw, Var_InterfaceGW_Size, Var_InterfaceGW);  
    Settings.read(sub, Var_InterfaceSub_Size, Var_InterfaceSub);  
    Network.startEthernet(ip, gw, sub);

  }

  info("Starting webserver...");

  uint16_t port;
  Settings.read_16bit(port, Var_WebServerPort);
  Network.startWebServer(port);

  info("Starting WebSocket server...");

  Settings.read_16bit(port, Var_WebSocketPort);
  Network.startWebSocketServer(port, websocketMessageCallback);

  // TODO: REFACTOR
  info("Connecting to VideoHub...");

  Settings.read(ip, Var_VideoHubIP_Size, Var_VideoHubIP);
  Settings.read_16bit(port, Var_VideoHubPort);
  Network.connectToVideoHub(ip, port);

  DebugLight.green();
  Debug.printTitle("TEENSY SETUP DONE");

}


void loop() {
  //resetButton.poll();
  pollButtons();
  Network.pollWebServer();
  Network.pollWebSocketServer();
  
  // TODO: REMOVE
  Network.pollVideoHub();

  Network.clock += 1;

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
  info(_pin, ", ", _state);
  {
    std::string message = "[\"gpi\",\"gpi-" + std::to_string(_pin - 28) + "\"," + std::to_string(_state) + "]";
    Network.sendMessage(Network.webSocketClient, message.c_str());
    Logic.parseButton(_pin, _state);

  }

}


// TODO REFACTOR
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
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Button_0_Source);

  } else if(header == "button_1") {
    info("Writing button_1 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Button_1_Source);

  } else if(header == "button_2") {
    info("Writing button_2 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Button_2_Source);

  } else if(header == "button_3") {
    info("Writing button_3 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Button_3_Source);

  } else if(header == "button_4") {
    info("Writing button_4 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Button_4_Source);

  } else if(header == "button_5") {
    info("Writing button_5 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Button_5_Source);

  } else if(header == "button_6") {
    info("Writing button_6 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Button_6_Source);

  } else if(header == "button_7") {
    info("Writing button_7 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Button_7_Source);

  } else if(header == "button_8") {
    info("Writing button_8 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Button_8_Source);

  } else if(header == "button_9") {
    info("Writing button_9 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Button_9_Source);

  } else if(header == "button_10") {
    info("Writing button_10 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Button_10_Source);

  } else if(header == "button_11") {
    info("Writing button_11 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Button_11_Source);

  }  else if(header == "video_hub_retry") {
    info("Trying to connect to the video hub...");

    byte ip[4];
    uint16_t port;

    Settings.read(ip, Var_VideoHubIP_Size, Var_VideoHubIP);
    Settings.read_16bit(port, Var_VideoHubPort);

    info(ip[0], ".", ip[1], ".",ip[2], ".",ip[3], ":", port);

    Network.reconnectToVideoHub(ip, port);
    if(json[1].as<bool>())
      Network.autoConnect = true;
    else
      Network.autoConnect = false;


  } else if(header == "ret_to_source_0") {
    info("Writing return_to_source_0 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Return_To_Source_0);

  } else if(header == "ret_to_source_1") {
    info("Writing return_to_source_1 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Return_To_Source_1);
    
  } else if(header == "ret_to_source_2") {
    info("Writing return_to_source_2 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Return_To_Source_2);
    
  } else if(header == "ret_to_source_3") {
    info("Writing return_to_source_3 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Return_To_Source_3);
    
  } else if(header == "ret_to_source_4") {
    info("Writing return_to_source_4 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Return_To_Source_4);
    
  } else if(header == "ret_to_source_5") {
    info("Writing return_to_source_5 source...");
    Settings.write(json[1].as<const char*>(), Var_Return_To_Source_Size, Var_Return_To_Source_5);
    
  } else if(header == "gpi_down") {
    buttonCallback(json[1].as<int>() + 28, true);
  } else if(header == "gpi_up") {
    buttonCallback(json[1].as<int>() + 28, false);

  }

}