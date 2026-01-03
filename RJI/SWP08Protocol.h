#ifndef SWP08Protocol_h
#define SWP08Protocol_h

#include <string>
#include "RouterProtocol.h"
#include "Debug.h"

// SW-P-08 Protocol Implementation
// Binary protocol typically on port 9000
// Supports extended addressing for large matrices (>256 sources/destinations)

// SWP-08 Control bytes
// Note: Some routers use DLE (0x10), others use SYN (0x16) for framing
#define SWP08_SYN           0x16  // Synchronous Idle (framing byte for some routers)
#define SWP08_DLE           0x10  // Data Link Escape
#define SWP08_STX           0x02  // Start of Text
#define SWP08_ETX           0x03  // End of Text
#define SWP08_ACK           0x06  // Acknowledge
#define SWP08_NAK           0x15  // Negative acknowledge

// Command types
#define SWP08_CMD_INTERROGATE       0x01  // Interrogate crosspoint
#define SWP08_CMD_CONNECT           0x02  // Connect command (standard)
#define SWP08_CMD_CROSSPOINT_TALLY  0x03  // Crosspoint tally response
#define SWP08_CMD_CROSSPOINT_TALLY2 0x04  // Crosspoint tally (alternate)
#define SWP08_CMD_CONNECT_TALLY     0x81  // Connect with tally response (extended)

// Parser states
enum SWP08State {
  SWP08_WAIT_SOM,       // Waiting for Start of Message (SYN or DLE)
  SWP08_WAIT_STX,       // Waiting for STX (0x02) after SOM
  SWP08_READ_DATA,      // Reading data bytes
  SWP08_GOT_SOM         // Got SOM while reading data - check next byte
};

class SWP08Protocol : public RouterProtocol {

public:
  uint8_t matrix = 0;  // Default matrix 0
  uint8_t level = 0;   // Default level 0

  SWP08Protocol() {
    reset();
  }

  void parse(uint8_t byte) override {
    // Supports SYN-framed protocol (0x16) used by your router
    // Format: SYN STX [DATA...] BTC CHK SYN ETX

    switch(currentState) {
      case SWP08_WAIT_SOM:
        // Accept either SYN (0x16) or DLE (0x10) as start of message
        if(byte == SWP08_SYN || byte == SWP08_DLE) {
          currentState = SWP08_WAIT_STX;
        }
        break;

      case SWP08_WAIT_STX:
        if(byte == SWP08_STX) {
          // Start of message
          dataIndex = 0;
          currentState = SWP08_READ_DATA;
        } else if(byte == SWP08_ACK) {
          // SYN ACK or DLE ACK response
          wasLastAck = true;
          reset();
        } else if(byte == SWP08_NAK) {
          // SYN NAK or DLE NAK response
          wasLastAck = false;
          reset();
        } else {
          // Not STX after SOM - reset
          reset();
        }
        break;

      case SWP08_READ_DATA:
        // Check for end-of-message marker (SYN or DLE)
        if(byte == SWP08_SYN || byte == SWP08_DLE) {
          currentState = SWP08_GOT_SOM;
        } else {
          // Regular data byte
          if(dataIndex < sizeof(dataBuffer)) {
            dataBuffer[dataIndex++] = byte;
          }
        }
        break;

      case SWP08_GOT_SOM:
        if(byte == SWP08_SYN || byte == SWP08_DLE) {
          // Stuffed byte - store single byte and continue
          if(dataIndex < sizeof(dataBuffer)) {
            dataBuffer[dataIndex++] = byte;
          }
          currentState = SWP08_READ_DATA;
        }
        else if(byte == SWP08_ETX) {
          // End of message - validate and process
          // Last two bytes in buffer are BTC and CHK
          if(dataIndex >= 2) {
            uint8_t chk = dataBuffer[--dataIndex];
            uint8_t btc = dataBuffer[--dataIndex];

            // Validate byte count
            if(btc == dataIndex) {
              // Calculate checksum (sum of data + btc, two's complement)
              uint8_t calcChk = 0;
              for(uint8_t i = 0; i < dataIndex; i++) {
                calcChk += dataBuffer[i];
              }
              calcChk += btc;
              calcChk = (~calcChk + 1) & 0xFF;

              if(calcChk == chk) {
                processMessage();
              }
            }
          }
          reset();
        }
        else if(byte == SWP08_ACK) {
          // ACK response
          wasLastAck = true;
          reset();
        }
        else if(byte == SWP08_NAK) {
          // NAK response
          wasLastAck = false;
          reset();
        }
        else {
          // Unknown sequence - reset
          reset();
        }
        break;
    }
  }

  void sendRoute(uint16_t dest, uint16_t source) override {
    // Build SWP-08 connect command using DLE-framed format
    // Note: Router expects DLE (0x10) framing for commands but may respond with SYN (0x16) framing
    // Format: DLE STX [DATA with DLE stuffing] BTC CHK DLE ETX
    // Standard command data: CMD MATRIX|LEVEL MULTIPLIER DEST SOURCE

    // Build the data payload first (without framing)
    uint8_t data[8];
    uint8_t dataLen = 0;
    uint8_t crc = 0;

    // Command byte - standard connect (0x02)
    data[dataLen++] = SWP08_CMD_CONNECT;

    // Matrix (upper nibble) | Level (lower nibble)
    // Matrix is 0-based, level is 0-based
    uint8_t matrixLevel = (matrix << 4) | (level & 0x0F);
    data[dataLen++] = matrixLevel;

    // Multiplier byte: combines high bits of dest and source
    // dest_H in upper nibble (bits 4-6), src_H in lower nibble (bits 0-2)
    uint8_t destH = (dest >> 7) & 0x07;
    uint8_t srcH = (source >> 7) & 0x07;
    uint8_t multiplier = (destH << 4) | srcH;
    data[dataLen++] = multiplier;

    // Destination low byte (0-127)
    data[dataLen++] = dest & 0x7F;

    // Source low byte (0-127)
    data[dataLen++] = source & 0x7F;

    // Calculate CRC (sum of all data bytes + length byte)
    for(uint8_t i = 0; i < dataLen; i++) {
      crc += data[i];
    }
    crc += dataLen;  // Add byte count to CRC
    crc = (~crc + 1) & 0xFF;  // Two's complement

    // Now build the framed message with DLE stuffing
    msgLength = 0;

    // Start of message: DLE STX
    msgBuffer[msgLength++] = SWP08_DLE;
    msgBuffer[msgLength++] = SWP08_STX;

    // Data with DLE stuffing (double any DLE bytes)
    for(uint8_t i = 0; i < dataLen; i++) {
      if(data[i] == SWP08_DLE) {
        msgBuffer[msgLength++] = SWP08_DLE;
      }
      msgBuffer[msgLength++] = data[i];
    }

    // Byte count (with DLE stuffing if needed)
    if(dataLen == SWP08_DLE) {
      msgBuffer[msgLength++] = SWP08_DLE;
    }
    msgBuffer[msgLength++] = dataLen;

    // Checksum (with DLE stuffing if needed)
    if(crc == SWP08_DLE) {
      msgBuffer[msgLength++] = SWP08_DLE;
    }
    msgBuffer[msgLength++] = crc;

    // End of message: DLE ETX
    msgBuffer[msgLength++] = SWP08_DLE;
    msgBuffer[msgLength++] = SWP08_ETX;

    lastDest = dest;
    lastSource = source;

    info("SWP08 TX: D:", dest, " S:", source);
  }

  // Get the binary message buffer and length
  uint8_t* getRouteMessage() {
    return msgBuffer;
  }

  uint8_t getRouteMessageLength() {
    return msgLength;
  }

  void reset() override {
    currentState = SWP08_WAIT_SOM;
    dataIndex = 0;
  }

  const char* getName() override {
    return "SWP-08";
  }

  // Send interrogate command for a specific destination
  void interrogate(uint16_t dest) {
    // Build interrogate command using DLE-framed format
    // Format: CMD MATRIX|LEVEL MULTIPLIER DEST

    uint8_t data[8];
    uint8_t dataLen = 0;
    uint8_t crc = 0;

    // Command byte
    data[dataLen++] = SWP08_CMD_INTERROGATE;

    // Matrix | Level
    uint8_t matrixLevel = (matrix << 4) | (level & 0x0F);
    data[dataLen++] = matrixLevel;

    // Multiplier (only dest high bits needed)
    uint8_t destH = (dest >> 7) & 0x07;
    data[dataLen++] = (destH << 4);

    // Destination low byte
    data[dataLen++] = dest & 0x7F;

    // Calculate CRC
    for(uint8_t i = 0; i < dataLen; i++) {
      crc += data[i];
    }
    crc += dataLen;
    crc = (~crc + 1) & 0xFF;

    // Build framed message with DLE
    msgLength = 0;
    msgBuffer[msgLength++] = SWP08_DLE;
    msgBuffer[msgLength++] = SWP08_STX;

    for(uint8_t i = 0; i < dataLen; i++) {
      if(data[i] == SWP08_DLE) {
        msgBuffer[msgLength++] = SWP08_DLE;
      }
      msgBuffer[msgLength++] = data[i];
    }

    if(dataLen == SWP08_DLE) {
      msgBuffer[msgLength++] = SWP08_DLE;
    }
    msgBuffer[msgLength++] = dataLen;

    if(crc == SWP08_DLE) {
      msgBuffer[msgLength++] = SWP08_DLE;
    }
    msgBuffer[msgLength++] = crc;

    msgBuffer[msgLength++] = SWP08_DLE;
    msgBuffer[msgLength++] = SWP08_ETX;
  }

private:
  SWP08State currentState = SWP08_WAIT_SOM;
  uint8_t dataBuffer[32];
  uint8_t dataIndex = 0;

  uint8_t msgBuffer[32];
  uint8_t msgLength = 0;

  void processMessage() {
    // dataBuffer[0] is the command byte
    if(dataIndex < 1) return;

    uint8_t cmd = dataBuffer[0];

    switch(cmd) {
      case SWP08_CMD_CROSSPOINT_TALLY:
      case SWP08_CMD_CROSSPOINT_TALLY2:
      case SWP08_CMD_CONNECT:
        // Standard format: CMD MATRIX|LEVEL MULTIPLIER DEST SOURCE
        processCrosspointTallyStandard();
        break;

      case SWP08_CMD_CONNECT_TALLY:
        // Extended format: CMD MATRIX LEVEL DEST_H DEST_L SRC_H SRC_L
        processCrosspointTallyExtended();
        break;

      default:
        info("SWP08: Unknown command 0x", cmd);
        break;
    }
  }

  void processCrosspointTallyStandard() {
    // Standard format: CMD MATRIX|LEVEL MULTIPLIER DEST SOURCE
    // dataBuffer[0] = cmd, [1] = matrix|level, [2] = multiplier, [3] = dest, [4] = source
    if(dataIndex >= 5) {
      uint8_t multiplier = dataBuffer[2];
      uint8_t destH = (multiplier >> 4) & 0x07;
      uint8_t srcH = multiplier & 0x07;

      uint16_t dest = ((uint16_t)destH << 7) | (dataBuffer[3] & 0x7F);
      uint16_t source = ((uint16_t)srcH << 7) | (dataBuffer[4] & 0x7F);

      // Check if this is our own echo
      if(dest == lastDest && source == lastSource) {
        lastDest = -1;
        lastSource = -1;
        info("SWP08: Route confirmed D:", dest, " S:", source);
      } else {
        routingPairs[dest] = source;
        updatedDest = dest;
        updatedSource = source;
        info("SWP08: Crosspoint tally D:", dest, " S:", source);
      }
    }
  }

  void processCrosspointTallyExtended() {
    // Extended format: CMD MATRIX LEVEL DEST_H DEST_L SRC_H SRC_L
    if(dataIndex >= 7) {
      uint16_t dest = ((uint16_t)dataBuffer[3] << 8) | dataBuffer[4];
      uint16_t source = ((uint16_t)dataBuffer[5] << 8) | dataBuffer[6];

      // Check if this is our own echo
      if(dest == lastDest && source == lastSource) {
        lastDest = -1;
        lastSource = -1;
        info("SWP08: Route confirmed D:", dest, " S:", source);
      } else {
        routingPairs[dest] = source;
        updatedDest = dest;
        updatedSource = source;
        info("SWP08: Crosspoint tally (ext) D:", dest, " S:", source);
      }
    }
  }

};

#endif
