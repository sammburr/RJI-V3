#ifndef SWP08Protocol_h
#define SWP08Protocol_h

#include <string>
#include "RouterProtocol.h"
#include "Debug.h"

// SW-P-08 Protocol Implementation
// Binary protocol typically on port 9000
// Supports extended addressing for large matrices (>256 sources/destinations)

// SWP-08 Command bytes
#define SWP08_STX           0x02  // Start of message
#define SWP08_ACK           0x04  // Acknowledge
#define SWP08_NAK           0x05  // Negative acknowledge

// Command types (directly after byte count)
#define SWP08_CMD_CONNECT           0x02  // Connect command
#define SWP08_CMD_CONNECT_TALLY     0x81  // Connect with tally response
#define SWP08_CMD_INTERROGATE       0x01  // Interrogate crosspoint
#define SWP08_CMD_CROSSPOINT_TALLY  0x03  // Crosspoint tally response

// Parser states
enum SWP08State {
  SWP08_WAIT_STX,       // Waiting for start byte (0x02)
  SWP08_WAIT_CMD,       // Waiting for command byte
  SWP08_WAIT_BYTECOUNT, // Waiting for byte count
  SWP08_READ_DATA,      // Reading data bytes
  SWP08_WAIT_CHECKSUM   // Waiting for checksum
};

class SWP08Protocol : public RouterProtocol {

public:
  uint8_t level = 0;  // Default level 0

  SWP08Protocol() {
    reset();
  }

  void parse(uint8_t byte) override {
    switch(currentState) {
      case SWP08_WAIT_STX:
        if(byte == SWP08_STX) {
          currentState = SWP08_WAIT_CMD;
          checksum = 0;
        }
        break;

      case SWP08_WAIT_CMD:
        command = byte;
        checksum ^= byte;

        // Handle simple ACK/NAK (no data follows)
        if(byte == SWP08_ACK) {
          wasLastAck = true;
          info("SWP08: ACK received");
          reset();
          return;
        }
        else if(byte == SWP08_NAK) {
          wasLastAck = false;
          err("SWP08: NAK received");
          reset();
          return;
        }

        currentState = SWP08_WAIT_BYTECOUNT;
        break;

      case SWP08_WAIT_BYTECOUNT:
        byteCount = byte;
        checksum ^= byte;
        dataIndex = 0;

        if(byteCount > 0 && byteCount <= sizeof(dataBuffer)) {
          currentState = SWP08_READ_DATA;
        } else {
          reset();
        }
        break;

      case SWP08_READ_DATA:
        dataBuffer[dataIndex++] = byte;
        checksum ^= byte;

        if(dataIndex >= byteCount) {
          currentState = SWP08_WAIT_CHECKSUM;
        }
        break;

      case SWP08_WAIT_CHECKSUM:
        if(byte == checksum) {
          processMessage();
        } else {
          err("SWP08: Checksum mismatch");
        }
        reset();
        break;
    }
  }

  void sendRoute(uint16_t dest, uint16_t source) override {
    // Build SWP-08 connect command with extended addressing
    // Format: STX CMD BYTECOUNT MATRIX+LEVEL DEST_H DEST_L SRC_H SRC_L CHECKSUM

    msgLength = 0;
    uint8_t chk = 0;

    // STX
    msgBuffer[msgLength++] = SWP08_STX;

    // Command - Connect with tally
    msgBuffer[msgLength++] = SWP08_CMD_CONNECT_TALLY;
    chk ^= SWP08_CMD_CONNECT_TALLY;

    // Byte count (6 bytes: matrix, level, dest_h, dest_l, src_h, src_l)
    msgBuffer[msgLength++] = 0x06;
    chk ^= 0x06;

    // Matrix (0) in upper nibble, level in lower nibble
    // For extended addressing, matrix/multiplier goes here
    uint8_t matrixByte = (0 << 4) | (level & 0x0F);
    msgBuffer[msgLength++] = matrixByte;
    chk ^= matrixByte;

    // Destination high byte (div 128 for extended addressing)
    uint8_t destH = (dest >> 7) & 0x7F;
    msgBuffer[msgLength++] = destH;
    chk ^= destH;

    // Destination low byte (mod 128)
    uint8_t destL = dest & 0x7F;
    msgBuffer[msgLength++] = destL;
    chk ^= destL;

    // Source high byte (div 128 for extended addressing)
    uint8_t srcH = (source >> 7) & 0x7F;
    msgBuffer[msgLength++] = srcH;
    chk ^= srcH;

    // Source low byte (mod 128)
    uint8_t srcL = source & 0x7F;
    msgBuffer[msgLength++] = srcL;
    chk ^= srcL;

    // Checksum
    msgBuffer[msgLength++] = chk;

    lastDest = dest;
    lastSource = source;
  }

  // Get the binary message buffer and length
  uint8_t* getRouteMessage() {
    return msgBuffer;
  }

  uint8_t getRouteMessageLength() {
    return msgLength;
  }

  void reset() override {
    currentState = SWP08_WAIT_STX;
    command = 0;
    byteCount = 0;
    dataIndex = 0;
    checksum = 0;
  }

  const char* getName() override {
    return "SWP-08";
  }

  // Send interrogate command for a specific destination
  void interrogate(uint16_t dest) {
    msgLength = 0;
    uint8_t chk = 0;

    msgBuffer[msgLength++] = SWP08_STX;

    msgBuffer[msgLength++] = SWP08_CMD_INTERROGATE;
    chk ^= SWP08_CMD_INTERROGATE;

    // Byte count (4 bytes: matrix, level, dest_h, dest_l)
    msgBuffer[msgLength++] = 0x04;
    chk ^= 0x04;

    uint8_t matrixByte = (0 << 4) | (level & 0x0F);
    msgBuffer[msgLength++] = matrixByte;
    chk ^= matrixByte;

    uint8_t destH = (dest >> 7) & 0x7F;
    msgBuffer[msgLength++] = destH;
    chk ^= destH;

    uint8_t destL = dest & 0x7F;
    msgBuffer[msgLength++] = destL;
    chk ^= destL;

    msgBuffer[msgLength++] = chk;
  }

private:
  SWP08State currentState = SWP08_WAIT_STX;
  uint8_t command = 0;
  uint8_t byteCount = 0;
  uint8_t dataBuffer[16];
  uint8_t dataIndex = 0;
  uint8_t checksum = 0;

  uint8_t msgBuffer[16];
  uint8_t msgLength = 0;

  void processMessage() {
    switch(command) {
      case SWP08_CMD_CROSSPOINT_TALLY:
      case SWP08_CMD_CONNECT_TALLY:
        processCrosspointTally();
        break;

      default:
        info("SWP08: Unknown command ", command);
        break;
    }
  }

  void processCrosspointTally() {
    // Extended format: MATRIX DEST_H DEST_L SRC_H SRC_L [STATUS]
    if(byteCount >= 5) {
      // uint8_t matrix = (dataBuffer[0] >> 4) & 0x0F;
      // uint8_t msgLevel = dataBuffer[0] & 0x0F;

      uint16_t dest = ((uint16_t)(dataBuffer[1] & 0x7F) << 7) | (dataBuffer[2] & 0x7F);
      uint16_t source = ((uint16_t)(dataBuffer[3] & 0x7F) << 7) | (dataBuffer[4] & 0x7F);

      // Check if this is our own echo
      if(dest == lastDest && source == lastSource) {
        lastDest = -1;
        lastSource = -1;
        info("SWP08: Route confirmed D:", dest, " S:", source);
      } else {
        routingPairs[dest] = source;
        info("SWP08: Crosspoint tally D:", dest, " S:", source);
      }
    }
  }

};

#endif
