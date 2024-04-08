#ifndef Logic_h
#define Logic_h

#include <string>
#include <stdint.h>
#include <list>

#include "Settings.h"

// Helper class for using `Engineers` as a data type
class Engineer {

public:
  Engineer(byte _id) {
    // on startup read this engineers settings from EEPROM
    id = _id;

    // The memory location for an engineers's settings of id `n` starts at EEPROM location 
    // Eng_0 + (n * 14)
    Settings.read_16bit(mask, Var_Eng_0 + ((int)id*14));
    Settings.read(dest, 1, Var_Eng_0 + ((int)id*14));

    info(mask);
    info(dest);


  }


private:
  byte id;
  uint16_t mask;
  byte dest;
  char name[14]; // Inclues null termination `\0`


};


// This singleton holds behaviour for managing button logic


class Logic {

public:
  // Parse a button pin and state to the routing logic
  void parseButton(int _pin, bool _state) {

    // First check which engineer this button belongs to 
    // loop through all the engineers
    for(byte i=0; i<6; i++) {

      uint16_t mask;
      Settings.read_16bit(mask, Var_Eng_0 + ((int)i*14));
      byte dest[1];
      Settings.read(dest, 1, Var_Eng_0 + 2 + ((int)i*14));
      byte type[1];
      Settings.read(type, 1, Var_Eng_0 + 3 + ((int)i*14));
      char name[10];
      Settings.read(name, 10, Var_Eng_0 + 4 + ((int)i*14));

      if(checkMask(mask, _pin - 29)) {
        // This button belongs to this engineer
        // if the _state is true (button down), eitherway we send it to the router
        if(_state) {
          // Read the source from settings
          uint16_t source;
          Settings.read_16bit(source, Var_Button_0_Source + ((int)(_pin - 29)*2));

          std::string msg = "VIDEO OUTPUT ROUTING:\n";
          msg += std::to_string(*dest);
          msg += " ";
          msg += std::to_string(source);
          msg += "\n\n";

          info("Setting engineer: ", name, "(", *dest ,")", " to source: ", source);
          Network.sendMessageToVideoHub(msg.c_str());

          if (*type){
            // Add this to the top of this engineer's stack
            lists[i].push_front(_pin - 29);
          }
          
        }
        // Otherwise we need to check which logic type the engineer is using
        else if(*type) { // Momentary
          // Remove from the list and use the top button
          // as the current route

          lists[i].remove(_pin - 29);

          // Now if the list is empty, route to the current mapping set by the router
          uint16_t source;
          Settings.read_16bit(source, Var_Button_0_Source + ((int)(lists[i].front())*2));

          if(lists[i].empty()) {
            source = VideoHub.routingPairs[*dest];
            info(VideoHub.routingPairs[*dest]);
          }

          info(source);

          std::string msg = "VIDEO OUTPUT ROUTING:\n";
          msg += std::to_string(*dest);
          msg += " ";
          msg += std::to_string(source);
          msg += "\n\n";

          info("Setting engineer: ", name, "(", *dest ,")", " to source: ", source);
          Network.sendMessageToVideoHub(msg.c_str());

        }

      }

    }

  }

private:
  bool checkMask(uint16_t _mask, int _bit) {
    return (_mask & (1 << _bit)) != 0;
  }

  std::list<int> lists[6];


};

Logic Logic;


#endif
