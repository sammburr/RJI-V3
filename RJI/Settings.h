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


class Settings {


// Default values for settings
byte interface_ip[Var_InterfaceIP_Size] =     {0, 0, 0, 0};
byte videohub_ip[Var_VideoHubIP_Size] =       {172, 16, 21, 55}; //TODO make this something sensible
uint16_t webserver_port =                     8080;
uint16_t websocket_port =                     80;
uint16_t videohub_port =                      9990;
byte reset_flag[Var_ResetInterface_Size] =    {0};
byte dhcp_toggle[Var_DHCPToggle_Size] =       {1};

byte eng_0[Var_Eng_0_Size] =                  {B00000000, B00000011, 0, 0, 'D', 'a', 'v', 'e', '\0', 'x', 'x', 'x', 'x', 'x'};
byte eng_1[Var_Eng_1_Size] =                  {B00000000, B00001100, 1, 0, 'G', 'a', 'r', 'r', 'y', '\0', 'x', 'x', 'x', 'x'};
byte eng_2[Var_Eng_2_Size] =                  {B00000000, B00110000, 2, 0, 'D', 'i', 'c', 'k', 'y', '\0', 'x', 'x', 'x', 'x'};
byte eng_3[Var_Eng_3_Size] =                  {B00000000, B11000000, 3, 1, 'S', 'a', 'm', '\0', 'x', 'x', 'x', 'x', 'x', 'x'};
byte eng_4[Var_Eng_4_Size] =                  {B00000011, B00000000, 4, 1, 'M', 'o', 'h', 'a', 'm', 'm', 'e', 'd', '\0', 'x'};
byte eng_5[Var_Eng_5_Size] =                  {B00001100, B00000000, 5, 1, 'A', 'r', 't', 'h', 'u', 'r', '\0', 'x', 'x', 'x'};


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
    write(videohub_ip, Var_VideoHubIP_Size, Var_VideoHubIP);
    write_16bit(videohub_port, Var_VideoHubPort);
    write(dhcp_toggle, Var_DHCPToggle_Size, Var_DHCPToggle);

    write(eng_0, Var_Eng_0_Size, Var_Eng_0);
    write(eng_1, Var_Eng_1_Size, Var_Eng_1);
    write(eng_2, Var_Eng_2_Size, Var_Eng_2);
    write(eng_3, Var_Eng_3_Size, Var_Eng_3);
    write(eng_4, Var_Eng_4_Size, Var_Eng_4);
    write(eng_5, Var_Eng_5_Size, Var_Eng_5);

  }


  // Helper function prints out all settings in a nice
  // format
  void printSettings() {
    byte ip[Var_InterfaceIP_Size];
    read(ip, Var_InterfaceIP_Size, Var_InterfaceIP);
    uint16_t webServerPort;
    read_16bit(webServerPort, Var_WebServerPort);
    byte resetFlag[Var_ResetInterface_Size];
    read(resetFlag, Var_ResetInterface_Size, Var_ResetInterface);
    uint16_t webSocketPort;
    read_16bit(webSocketPort, Var_WebSocketPort);
    byte videoHubIP[Var_VideoHubIP_Size];
    read(videoHubIP, Var_VideoHubIP_Size, Var_VideoHubIP);
    uint16_t videoHubPort;
    read_16bit(videoHubPort, Var_VideoHubPort);
    byte dhcpToggle[Var_DHCPToggle_Size];
    read(dhcpToggle, Var_DHCPToggle_Size, Var_DHCPToggle);

    Debug.printSubTitle("SETTINGS START");
    
    info("IP: ", ip[0], ".", ip[1], ".", ip[2], ".", ip[3]);
    info("WebServerPort: ", webServerPort);
    info("WebSocketPort: ", webSocketPort);
    info("Reset Flag: ", *resetFlag);
    info("VideoHub IP: ", videoHubIP[0], ".", videoHubIP[1], ".", videoHubIP[2], ".", videoHubIP[3]);
    info("VideoHub Port: ", videoHubPort);
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
    uint16_t webServerPort;
    read_16bit(webServerPort, Var_WebServerPort);
    byte resetFlag[Var_ResetInterface_Size];
    read(resetFlag, Var_ResetInterface_Size, Var_ResetInterface);
    uint16_t webSocketPort;
    read_16bit(webSocketPort, Var_WebSocketPort);

    byte videoHubIP[Var_VideoHubIP_Size];
    read(videoHubIP, Var_VideoHubIP_Size, Var_VideoHubIP);
    uint16_t videoHubPort;
    read_16bit(videoHubPort, Var_VideoHubPort);

    byte dhcpToggle[Var_DHCPToggle_Size];
    read(dhcpToggle, Var_DHCPToggle_Size, Var_DHCPToggle);


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


    // Video Hub IP
    doc[4][0] = "videohub-ip";
    doc[4][1] = videoHubIP[0];
    doc[4][2] = videoHubIP[1];
    doc[4][3] = videoHubIP[2];
    doc[4][4] = videoHubIP[3];


    // Video Hub Port
    doc[5][0] = "videohub-port";
    doc[5][1] = videoHubPort;


    return doc;

  }
  

private:

};

Settings Settings;
#endif