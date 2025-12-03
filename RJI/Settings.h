#ifndef Settings_h
#define Settings_h


#include <EEPROM.h>
#include <ArduinoJson.h>
#include <string.h>


#include "Debug.h"


// This class is a singleton, it should only be initalised once.
// This class holds address of EEPROM memory and acts as an interface to
// easily read and write to EEPROM memory.
// For this project settings contains definitions class with the names of all
// 'variables' in memory.


// The size (in bytes) of each var
#define Var_InterfaceIP_Size 4
#define Var_WebServerPort_Size 2
#define Var_ResetInterface_Size 1
#define Var_WebSocketPort_Size 2
#define Var_VideoHubIP_Size 4
#define Var_VideoHubPort_Size 2
#define Var_DHCPToggle_Size 1
#define Var_NumberEngineers_Size 1

#define Var_Eng_0_Size 14 // byte 1-2: mask, 3: dest, 4: type, 5-14: name (null terminated list of chars)
#define Var_Eng_1_Size 14 // byte 1-2: mask, 3: dest, 4: type, 5-14: name (null terminated list of chars)
#define Var_Eng_2_Size 14 // byte 1-2: mask, 3: dest, 4: type, 5-14: name (null terminated list of chars)
#define Var_Eng_3_Size 14 // byte 1-2: mask, 3: dest, 4: type, 5-14: name (null terminated list of chars)
#define Var_Eng_4_Size 14 // byte 1-2: mask, 3: dest, 4: type, 5-14: name (null terminated list of chars)
#define Var_Eng_5_Size 14 // byte 1-2: mask, 3: dest, 4: type, 5-14: name (null terminated list of chars)
                          // Type:0 == `Toggle`, Type:1 == `Momentary`

#define Var_Button_0_Source_Size  2
#define Var_Button_1_Source_Size  2
#define Var_Button_2_Source_Size  2
#define Var_Button_3_Source_Size  2
#define Var_Button_4_Source_Size  2
#define Var_Button_5_Source_Size  2
#define Var_Button_6_Source_Size  2
#define Var_Button_7_Source_Size  2
#define Var_Button_8_Source_Size  2
#define Var_Button_9_Source_Size  2
#define Var_Button_10_Source_Size 2
#define Var_Button_11_Source_Size 2

#define Var_InterfaceSub_Size 4
#define Var_InterfaceGW_Size 4

#define Var_RouterProtocol_Size 1   // 0 = VideoHub, 1 = SWP-08
#define Var_SWP08Level_Size 1       // Level number for SWP-08 (0-15)

// The offset (starting byte in EEPROM memory) of each var
#define Var_InterfaceIP 0
#define Var_WebServerPort Var_InterfaceIP + Var_InterfaceIP_Size
#define Var_ResetInterface Var_WebServerPort + Var_WebServerPort_Size
#define Var_WebSocketPort Var_ResetInterface + Var_ResetInterface_Size
#define Var_VideoHubIP Var_WebSocketPort + Var_WebServerPort_Size
#define Var_VideoHubPort Var_VideoHubIP + Var_InterfaceIP_Size
#define Var_DHCPToggle Var_VideoHubPort + Var_VideoHubPort_Size
#define Var_NumberEngineers Var_DHCPToggle + Var_DHCPToggle_Size

#define Var_Eng_0 Var_NumberEngineers + Var_NumberEngineers_Size
#define Var_Eng_1 Var_Eng_0 + Var_Eng_0_Size
#define Var_Eng_2 Var_Eng_1 + Var_Eng_1_Size
#define Var_Eng_3 Var_Eng_2 + Var_Eng_2_Size
#define Var_Eng_4 Var_Eng_3 + Var_Eng_3_Size
#define Var_Eng_5 Var_Eng_4 + Var_Eng_4_Size

#define Var_Button_0_Source   Var_Eng_5 + Var_Eng_5_Size
#define Var_Button_1_Source   Var_Button_0_Source + Var_Button_0_Source_Size
#define Var_Button_2_Source   Var_Button_1_Source + Var_Button_1_Source_Size 
#define Var_Button_3_Source   Var_Button_2_Source + Var_Button_2_Source_Size 
#define Var_Button_4_Source   Var_Button_3_Source + Var_Button_3_Source_Size 
#define Var_Button_5_Source   Var_Button_4_Source + Var_Button_4_Source_Size 
#define Var_Button_6_Source   Var_Button_5_Source + Var_Button_5_Source_Size 
#define Var_Button_7_Source   Var_Button_6_Source + Var_Button_6_Source_Size 
#define Var_Button_8_Source   Var_Button_7_Source + Var_Button_7_Source_Size 
#define Var_Button_9_Source   Var_Button_8_Source + Var_Button_8_Source_Size 
#define Var_Button_10_Source  Var_Button_9_Source + Var_Button_9_Source_Size 
#define Var_Button_11_Source  Var_Button_10_Source + Var_Button_10_Source_Size 

#define Var_InterfaceSub Var_Button_11_Source + Var_InterfaceSub_Size
#define Var_InterfaceGW Var_InterfaceSub + Var_InterfaceGW_Size

#define Var_RouterProtocol Var_InterfaceGW + Var_InterfaceGW_Size
#define Var_SWP08Level Var_RouterProtocol + Var_RouterProtocol_Size

class Settings {

// Default values for settings
byte interface_ip[Var_InterfaceIP_Size] =     {192,168,10,51};
byte router_ip[Var_VideoHubIP_Size] =         {192,168,10,11};
byte router_protocol[Var_RouterProtocol_Size] = {0};  // 0 = VideoHub, 1 = SWP-08
byte swp08_level[Var_SWP08Level_Size] =       {0};    // Default level 0

byte interface_sub[Var_InterfaceSub_Size] =         {255,255,255,0};
byte interface_gw[Var_InterfaceGW_Size] =           {192,168,10,1};

uint16_t webserver_port =                     80;
uint16_t websocket_port =                     8080;
uint16_t router_port =                        9990;  // VideoHub default, SWP-08 uses 9000
byte reset_flag[Var_ResetInterface_Size] =    {0};
byte dhcp_toggle[Var_DHCPToggle_Size] =       {0};

byte eng_0[Var_Eng_0_Size] =                  {B00000000, B00001111, 0, 1, 'V', 'i', 's', ' ', '1', '\0', 'x', 'x', 'x', 'x'};
byte eng_1[Var_Eng_1_Size] =                  {B00000000, B11110000, 1, 1, 'V', 'i', 's', ' ', '2', '\0', 'x', 'x', 'x', 'x'};
byte eng_2[Var_Eng_2_Size] =                  {B00001111, B00000000, 2, 1, 'V', 'i', 's', ' ', '3', '\0', 'x', 'x', 'x', 'x'};
byte eng_3[Var_Eng_3_Size] =                  {B00000000, B00000000, 3, 0, 'V', 'i', 's', ' ', '4', '\0', 'x', 'x', 'x', 'x'};
byte eng_4[Var_Eng_4_Size] =                  {B00000000, B00000000, 4, 0, 'V', 'i', 's', ' ', '5', '\0', 'x', 'x', 'x', 'x'};
byte eng_5[Var_Eng_5_Size] =                  {B00000000, B00000000, 5, 0, 'V', 'i', 's', ' ', '6', '\0', 'x', 'x', 'x', 'x'};

uint16_t button_0_source =                    0;
uint16_t button_1_source =                    1;
uint16_t button_2_source =                    2;
uint16_t button_3_source =                    3;
uint16_t button_4_source =                    4;
uint16_t button_5_source =                    5;
uint16_t button_6_source =                    6;
uint16_t button_7_source =                    7;
uint16_t button_8_source =                    8;
uint16_t button_9_source =                    9;
uint16_t button_10_source =                   10;
uint16_t button_11_source =                   11;

public:
  Settings() {}


  // Write to a given array the value of the variable and given size
  // @param (byte*) pointer to array to write to
  // @param (byte) size of variable
  // @param (int) offset of variable
  void read(byte* _array, byte _size, int _offset) {
    // Loop through each element of array as defined by _size
    for (byte i=0; i<_size; i++) {
      // Increment pointer for _array and EEPROM mem
      int ptrAddr = _offset + i;
      byte* ptrArray = _array + i;
      // Read memory at this location in EEPROM mem
      int value = EEPROM.read(ptrAddr);
      *ptrArray = value;
    }

  }


  // Update EEPROM memory with a given array of a given size
  // @param (byte*) pointer to array to write from
  // @param (byte) size of variable
  // @param (int) offset of variable
  void write(byte* _array, byte _size, int _offset) {
    // Loop through each element of array as defined by _size
    for (byte i=0; i<_size; i++) {
      // Increment pointer for _array and EEPROM mem
      int ptrAddr = _offset + i;
      byte* ptrArray = _array + i;
      // Write to memory at this location in EEPROM mem
      EEPROM.update(ptrAddr, *ptrArray);
    }

  }


  // Write a 16bit value into EEPROM at a given offset
  // @param (uint16_t) value to write
  // @param (int) memory offset
  void write_16bit(uint16_t _value, int _offset) {
    // Split 16bit number into an array of 2 bytes
    byte arr[] = {(_value >> 8) & 0xFF, _value & 0xFF};
    // Write to memory
    write(arr, 2, _offset);

  }


  // Read a 16bit value into a given var at a given offset
  // @param (uint16_t&) value to write to
  // @param (int) offset in memory to read from
  void read_16bit(uint16_t& _value, int _offset) {
    // Read offset into an array of 2 bytes
    byte arr[2];
    read(arr, 2, _offset);
    // Combine bytes into a 16bit value
    _value = (static_cast<uint16_t>(arr[0]) << 8) | arr[1];

  }


  // Helper function to write default values to settings
  void writeDefaults() {
    write(interface_ip, Var_InterfaceIP_Size, Var_InterfaceIP);
    write_16bit(webserver_port, Var_WebServerPort);
    write(reset_flag, Var_ResetInterface_Size, Var_ResetInterface);
    write_16bit(websocket_port, Var_WebSocketPort);
    write(router_ip, Var_VideoHubIP_Size, Var_VideoHubIP);
    write_16bit(router_port, Var_VideoHubPort);
    write(router_protocol, Var_RouterProtocol_Size, Var_RouterProtocol);
    write(swp08_level, Var_SWP08Level_Size, Var_SWP08Level);
    write(dhcp_toggle, Var_DHCPToggle_Size, Var_DHCPToggle);

    write(eng_0, Var_Eng_0_Size, Var_Eng_0);
    write(eng_1, Var_Eng_1_Size, Var_Eng_1);
    write(eng_2, Var_Eng_2_Size, Var_Eng_2);
    write(eng_3, Var_Eng_3_Size, Var_Eng_3);
    write(eng_4, Var_Eng_4_Size, Var_Eng_4);
    write(eng_5, Var_Eng_5_Size, Var_Eng_5);

    write_16bit(button_0_source, Var_Button_0_Source);
    write_16bit(button_1_source, Var_Button_1_Source);
    write_16bit(button_2_source, Var_Button_2_Source);
    write_16bit(button_3_source, Var_Button_3_Source);
    write_16bit(button_4_source, Var_Button_4_Source);
    write_16bit(button_5_source, Var_Button_5_Source);
    write_16bit(button_6_source, Var_Button_6_Source);
    write_16bit(button_7_source, Var_Button_7_Source);
    write_16bit(button_8_source, Var_Button_8_Source);
    write_16bit(button_9_source, Var_Button_9_Source);
    write_16bit(button_10_source, Var_Button_10_Source);
    write_16bit(button_11_source, Var_Button_11_Source);

    write(interface_sub, Var_InterfaceSub_Size, Var_InterfaceSub);
    write(interface_gw, Var_InterfaceGW_Size, Var_InterfaceGW);
  }


  // Helper function prints out all settings in a nice
  // format
  void printSettings() {
    byte ip[Var_InterfaceIP_Size];
    read(ip, Var_InterfaceIP_Size, Var_InterfaceIP);
    byte sub[Var_InterfaceSub_Size];
    read(sub, Var_InterfaceSub_Size, Var_InterfaceSub);
    byte gw[Var_InterfaceGW_Size];
    read(gw, Var_InterfaceGW_Size, Var_InterfaceGW);
    uint16_t webServerPort;
    read_16bit(webServerPort, Var_WebServerPort);
    byte resetFlag[Var_ResetInterface_Size];
    read(resetFlag, Var_ResetInterface_Size, Var_ResetInterface);
    uint16_t webSocketPort;
    read_16bit(webSocketPort, Var_WebSocketPort);
    byte routerIP[Var_VideoHubIP_Size];
    read(routerIP, Var_VideoHubIP_Size, Var_VideoHubIP);
    uint16_t routerPort;
    read_16bit(routerPort, Var_VideoHubPort);
    byte dhcpToggle[Var_DHCPToggle_Size];
    read(dhcpToggle, Var_DHCPToggle_Size, Var_DHCPToggle);
    byte routerProtocol[Var_RouterProtocol_Size];
    read(routerProtocol, Var_RouterProtocol_Size, Var_RouterProtocol);
    byte swp08Level[Var_SWP08Level_Size];
    read(swp08Level, Var_SWP08Level_Size, Var_SWP08Level);

    Debug.printSubTitle("SETTINGS START");

    info("IP: ", ip[0], ".", ip[1], ".", ip[2], ".", ip[3]);
    info("Subnet: ", sub[0], ".", sub[1], ".", sub[2], ".", sub[3]);
    info("Gateway: ", gw[0], ".", gw[1], ".", gw[2], ".", gw[3]);
    info("WebServerPort: ", webServerPort);
    info("WebSocketPort: ", webSocketPort);
    info("Reset Flag: ", *resetFlag);
    info("Router IP: ", routerIP[0], ".", routerIP[1], ".", routerIP[2], ".", routerIP[3]);
    info("Router Port: ", routerPort);
    info("Router Protocol: ", *routerProtocol == 0 ? "VideoHub" : "SWP-08");
    info("SWP-08 Level: ", *swp08Level);
    info("DHCP Flag: ", *dhcpToggle);


    for(byte i=0; i<6; i++) {
      uint16_t mask;
      read_16bit(mask, Var_Eng_0 + ((int)i*14));
      byte dest[1];
      read(dest, 1, Var_Eng_0 + 2 + ((int)i*14));
      byte type[1];
      read(type, 1, Var_Eng_0 + 3 + ((int)i*14));
      char name[10];
      read(name, 10, Var_Eng_0 + 4 + ((int)i*14));

      std::string strName = name;
      std::string strType;

      if(*type) strType = "Momentary";
      else strType = "Toggle";

      info("");
      info("Eng ", i, " mask: ", mask);
      info("Eng ", i, " dest: ", *dest);
      info("Eng ", i, " type: ", strType.c_str());
      info("Eng ", i, " name: ", strName.c_str());

    }

    info("");

    for(byte i=0; i<12; i++) {
      uint16_t source;
      read_16bit(source, Var_Button_0_Source + ((int)i*2));

      info("Button ", i, " source: ", source);

    }

    Debug.printSubTitle("SETTINGS END");


  }


  // Helper functions to get all settings in a JSON format
  JsonDocument getJson() {
    JsonDocument doc;

    // Header
    doc[0] = "settings";

    // Body
    byte ip[Var_InterfaceIP_Size];
    read(ip, Var_InterfaceIP_Size, Var_InterfaceIP);
    byte sub[Var_InterfaceSub_Size];
    read(sub, Var_InterfaceSub_Size, Var_InterfaceSub);
    byte gw[Var_InterfaceGW_Size];
    read(gw, Var_InterfaceGW_Size, Var_InterfaceGW);
    uint16_t webServerPort;
    read_16bit(webServerPort, Var_WebServerPort);
    byte resetFlag[Var_ResetInterface_Size];
    read(resetFlag, Var_ResetInterface_Size, Var_ResetInterface);
    uint16_t webSocketPort;
    read_16bit(webSocketPort, Var_WebSocketPort);

    byte routerIP[Var_VideoHubIP_Size];
    read(routerIP, Var_VideoHubIP_Size, Var_VideoHubIP);
    uint16_t routerPort;
    read_16bit(routerPort, Var_VideoHubPort);

    byte dhcpToggle[Var_DHCPToggle_Size];
    read(dhcpToggle, Var_DHCPToggle_Size, Var_DHCPToggle);

    byte routerProtocol[Var_RouterProtocol_Size];
    read(routerProtocol, Var_RouterProtocol_Size, Var_RouterProtocol);

    byte swp08Level[Var_SWP08Level_Size];
    read(swp08Level, Var_SWP08Level_Size, Var_SWP08Level);


    // IP
    doc[1][0] = "interface-ip"; // This needs to be the same `id` as the inputObject in the webpage!!
    doc[1][1] = ip[0];
    doc[1][2] = ip[1];
    doc[1][3] = ip[2];
    doc[1][4] = ip[3];


    // Web Server Port
    doc[2][0] = "interface-web-port"; // This needs to be the same `id` as the inputObject in the webpage!!
    doc[2][1] = webServerPort;


    // DHCP Toggle
    doc[3][0] = "interface-dhcp";
    doc[3][1] = *dhcpToggle;


    // Router IP
    doc[4][0] = "router-ip";
    doc[4][1] = routerIP[0];
    doc[4][2] = routerIP[1];
    doc[4][3] = routerIP[2];
    doc[4][4] = routerIP[3];


    // Router Port
    doc[5][0] = "router-port";
    doc[5][1] = routerPort;

    // Router Protocol
    doc[10][0] = "router-protocol";
    doc[10][1] = *routerProtocol;

    // SWP-08 Level
    doc[11][0] = "swp08-level";
    doc[11][1] = *swp08Level;


    // Engineers
    doc[6][0] = "engineers";

    for(byte i=0; i<6; i++) {
      uint16_t mask;
      read_16bit(mask, Var_Eng_0 + ((int)i*14));
      byte dest[1];
      read(dest, 1, Var_Eng_0 + 2 + ((int)i*14));
      byte type[1];
      read(type, 1, Var_Eng_0 + 3 + ((int)i*14));
      char name[10];
      read(name, 10, Var_Eng_0 + 4 + ((int)i*14));

      std::string strName = name;

      doc[6][i+1][0] = mask;
      doc[6][i+1][1] = *dest;
      doc[6][i+1][2] = *type;
      doc[6][i+1][3] = strName;


    }


    doc[7][0] = "buttons";

    for(byte i=0; i<12; i++) {
      uint16_t source;
      read_16bit(source, Var_Button_0_Source + ((int)i*2));

      doc[7][i+1] = source;

    }


    // Subnet
    doc[8][0] = "interface-sub";
    doc[8][1] = sub[0];
    doc[8][2] = sub[1];
    doc[8][3] = sub[2];
    doc[8][4] = sub[3];

    // Gateway
    doc[9][0] = "interface-gw";
    doc[9][1] = gw[0];
    doc[9][2] = gw[1];
    doc[9][3] = gw[2];
    doc[9][4] = gw[3];

    return doc;

  }
  

  void setEngineer(int _indx, JsonDocument _json) {
    // 1 = mask (16 bit)
    write_16bit(_json[1].as<uint16_t>(), Var_Eng_0 + (_indx*14));

    // // 2 = dest ( byte )
    byte dest[1];
    dest[0] = _json[2].as<byte>();

    write(dest, 1, Var_Eng_0 + (_indx*14) + 2);

    // // 3 = type ( bool )
    byte type[1];
    type[0] = {0};
    if(_json[3].as<bool>()) {
      type[0] = {1};
    }
    write(type, 1, Var_Eng_0 + (_indx*14) + 2 + 1);

    // 4 = name (String)
    const char* name;
    name = _json[4].as<const char*>();

    write(name, 10, Var_Eng_0 + (_indx*14) + 2 + 1 + 1);

  }

private:

};

Settings Settings;
#endif