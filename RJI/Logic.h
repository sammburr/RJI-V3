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

  // Check if a destination is locked by active button presses
  // Also returns true if the route matches what we sent (late ACK filtering)
  bool isDestLocked(uint16_t dest) {
    for(int i = 0; i < 6; i++) {
      if(lockedDest[i] == dest) return true;
    }
    return false;
  }

  // Check if an incoming route should be filtered (matches what we sent while locked)
  // Also filters for a short delay after unlock to catch late return-to-source ACKs
  bool shouldFilterRoute(uint16_t dest, uint16_t source) {
    unsigned long now = millis();
    for(int i = 0; i < 6; i++) {
      // Check if actively locked and source is in recent history
      if(lockedDest[i] == dest) {
        for(int j = 0; j < SENT_HISTORY_SIZE; j++) {
          if(sentHistory[i][j] == source) {
            return true;  // This is a confirmation of something we sent
          }
        }
      }
      // Check if recently unlocked (within delay window) and this is either:
      // - the return-to-source ACK, OR
      // - any source in our sent history
      if(unlockTime[i] > 0 && (now - unlockTime[i]) < UNLOCK_FILTER_DELAY_MS) {
        if(pendingReturnDest[i] == dest) {
          if(pendingReturnSource[i] == source) {
            return true;  // This is a late return-to-source ACK
          }
          // Check sent history for late ACKs
          for(int j = 0; j < SENT_HISTORY_SIZE; j++) {
            if(pendingSentHistory[i][j] == source) {
              return true;  // This is a late ACK from something we sent
            }
          }
        }
      } else if(unlockTime[i] > 0 && (now - unlockTime[i]) >= UNLOCK_FILTER_DELAY_MS) {
        // Delay expired, clear the pending state
        unlockTime[i] = 0;
      }
    }
    return false;
  }

  // Add a source to the sent history for an engineer
  void addToSentHistory(int engineerIndex, uint16_t source) {
    // Shift history and add new source at front
    for(int j = SENT_HISTORY_SIZE - 1; j > 0; j--) {
      sentHistory[engineerIndex][j] = sentHistory[engineerIndex][j-1];
    }
    sentHistory[engineerIndex][0] = source;
  }

  // Clear sent history for an engineer
  void clearSentHistory(int engineerIndex) {
    for(int j = 0; j < SENT_HISTORY_SIZE; j++) {
      sentHistory[engineerIndex][j] = 0xFFFF;
    }
  }

  // Copy sent history to pending (for post-unlock filtering)
  void copyToPendingHistory(int engineerIndex) {
    for(int j = 0; j < SENT_HISTORY_SIZE; j++) {
      pendingSentHistory[engineerIndex][j] = sentHistory[engineerIndex][j];
    }
  }

  // Parse a button pin and state to the routing logic
  void parseButton(int _pin, bool _state) {

    int buttonIndex = _pin - 29;  // 0-11 for the 12 GPIs

    // TSL 3.1 mode: simple tally on/off for each GPI
    if(Network.protocolType == PROTOCOL_TSL31) {
      // Read the TSL address for this button from the source field
      // (reusing the source field as TSL address)
      uint16_t tslAddress;
      Settings.read_16bit(tslAddress, Var_Button_0_Source + (buttonIndex * 2));

      // Send tally on for press, tally off for release
      Network.sendTallyToRouter((uint8_t)tslAddress, _state);
      return;
    }

    // VideoHub / SWP-08 mode: routing logic
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

      if(checkMask(mask, buttonIndex)) {
        // This button belongs to this engineer
        // if the _state is true (button down), send route to router
        if(_state) {
          // Read the source from settings
          uint16_t source;
          Settings.read_16bit(source, Var_Button_0_Source + (buttonIndex * 2));

          // On FIRST button press for this engineer, store the pre-press source
          if(lists[i].empty()) {
            returnSource[i] = Network.currentProtocol->routingPairs[*dest];
            lockedDest[i] = *dest;  // Lock this destination
            clearSentHistory(i);  // Start fresh history
          }

          // Track what we're sending for late ACK filtering (keep history)
          addToSentHistory(i, source);

          Network.currentProtocol->lastSource = *dest;
          Network.currentProtocol->lastDest = source;
          Network.sendRouteToRouter(*dest, source);

          if (*type){
            // Add this to the top of this engineer's stack
            lists[i].push_front(buttonIndex);
          }

        }
        // Otherwise we need to check which logic type the engineer is using
        else if(*type) { // Momentary - button released
          // Remove from the list
          lists[i].remove(buttonIndex);

          uint16_t source;
          if(lists[i].empty()) {
            // ALL buttons released - return to stored pre-press source
            source = returnSource[i];

            // Start delayed unlock - keep filtering all sent sources + return-to-source
            pendingReturnDest[i] = *dest;
            pendingReturnSource[i] = source;
            copyToPendingHistory(i);  // Copy sent history for post-unlock filtering
            unlockTime[i] = millis();

            lockedDest[i] = 0xFFFF;  // Unlock destination
            clearSentHistory(i);  // Clear active history
          } else {
            // Still have buttons pressed - use top of stack
            Settings.read_16bit(source, Var_Button_0_Source + ((int)(lists[i].front())*2));
          }

          Network.currentProtocol->lastSource = *dest;
          Network.currentProtocol->lastDest = source;
          Network.sendRouteToRouter(*dest, source);

        }

      }

    }

  }

private:
  static const unsigned long UNLOCK_FILTER_DELAY_MS = 750;  // Filter late ACKs for 750ms after unlock
  static const int SENT_HISTORY_SIZE = 4;  // Track last 4 sources sent per engineer

  bool checkMask(uint16_t _mask, int _bit) {
    return (_mask & (1 << _bit)) != 0;
  }

  std::list<int> lists[6];
  uint16_t returnSource[6] = {0};      // Pre-press source per engineer
  uint16_t lockedDest[6] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};  // Locked destinations (0xFFFF = none)

  // Sent history - track last N sources sent per engineer for ACK filtering
  uint16_t sentHistory[6][4] = {
    {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
    {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
    {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
    {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
    {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
    {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}
  };

  // Delayed unlock tracking - filter late ACKs after button release
  unsigned long unlockTime[6] = {0, 0, 0, 0, 0, 0};  // Time when unlock occurred (0 = not pending)
  uint16_t pendingReturnDest[6] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};  // Dest we sent return-to-source for
  uint16_t pendingReturnSource[6] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};  // Source we returned to
  uint16_t pendingSentHistory[6][4] = {  // Copy of sent history for post-unlock filtering
    {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
    {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
    {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
    {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
    {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
    {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}
  };

};

Logic Logic;


#endif
