#ifndef RouterProtocol_h
#define RouterProtocol_h

#include <stdint.h>

// Abstract base class for router protocols
// Implementations: VideoHubProtocol, SWP08Protocol

class RouterProtocol {

public:
  virtual ~RouterProtocol() {}

  // Parse incoming byte from router
  virtual void parse(uint8_t byte) = 0;

  // Send a crosspoint route command
  // @param dest - destination/output number (0-indexed)
  // @param source - source/input number (0-indexed)
  virtual void sendRoute(uint16_t dest, uint16_t source) = 0;

  // Reset parser state
  virtual void reset() = 0;

  // Get the protocol name for display
  virtual const char* getName() = 0;

  // Check if last command was acknowledged
  bool wasLastAck = false;

  // Routing state: routingPairs[dest] = source
  uint16_t routingPairs[1024];

  // Track last requested route (to filter echo responses)
  int16_t lastSource = -1;
  int16_t lastDest = -1;

  // Track routing pair updates for UI notification
  int16_t updatedDest = -1;  // Set when a routing pair is updated
  int16_t updatedSource = -1;

  // Expected responses counter
  int expected_resp = 0;

};

#endif
