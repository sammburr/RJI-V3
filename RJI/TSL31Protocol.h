#ifndef TSL31Protocol_h
#define TSL31Protocol_h

#include <stdint.h>
#include "RouterProtocol.h"
#include "Debug.h"

// TSL UMD Protocol V3.1 Implementation
// Simple tally protocol over TCP
// Message format: HEADER (1) + CONTROL (1) + DISPLAY DATA (16) = 18 bytes
//
// This is a SEND-ONLY protocol - we send tally on/off for each GPI

class TSL31Protocol : public RouterProtocol {

public:
  TSL31Protocol() {
    reset();
  }

  // TSL 3.1 is send-only, but we implement parse() for the interface
  void parse(uint8_t byte) override {
    // TSL 3.1 is one-way, no incoming messages to parse
    (void)byte;
  }

  // Not used for TSL 3.1 - we use sendTally instead
  void sendRoute(uint16_t dest, uint16_t source) override {
    (void)dest;
    (void)source;
  }

  // Build a TSL 3.1 message to set tally state for a display address
  // @param address - display address (0-126)
  // @param tallyOn - true to turn tally 1 on, false for off
  void sendTally(uint8_t address, bool tallyOn) {
    if(address > 126) {
      err("TSL 3.1: Address out of range (0-126): ", address);
      return;
    }

    // Header: address + 0x80
    msgBuffer[0] = address + 0x80;

    // Control byte:
    // bit 0 = tally 1 (we use this one)
    // bit 1 = tally 2
    // bit 2 = tally 3
    // bit 3 = tally 4
    // bits 4-5 = brightness (11 = full)
    // bits 6-7 = 0
    msgBuffer[1] = tallyOn ? 0x31 : 0x30;  // Full brightness + tally 1 on/off

    // Display data: 16 spaces (ASCII 0x20)
    for(int i = 0; i < 16; i++) {
      msgBuffer[2 + i] = 0x20;  // Space character
    }

    msgLength = 18;

    info("TSL 3.1: Address ", address, " Tally ", tallyOn ? "ON" : "OFF");
  }

  // Get the message buffer
  uint8_t* getRouteMessage() {
    return msgBuffer;
  }

  uint8_t getRouteMessageLength() {
    return msgLength;
  }

  void reset() override {
    msgLength = 0;
  }

  const char* getName() override {
    return "TSL 3.1";
  }

private:
  uint8_t msgBuffer[18];
  uint8_t msgLength = 0;

};

#endif
