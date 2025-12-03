#ifndef VideoHubProtocol_h
#define VideoHubProtocol_h

#include <string>
#include <cctype>
#include "RouterProtocol.h"
#include "Debug.h"

// Blackmagic VideoHub protocol implementation
// Text-based TCP protocol on port 9990
// Message format: "VIDEO OUTPUT ROUTING:\n<dest> <source>\n\n"

enum VideoHubState {
  VH_LFHeader,
  VH_LFEndOfHeader,
  VH_LFStartOfSource,
  VH_LFSource,
  VH_LFStartOfDest,
  VH_LFDest,
  VH_EOB
};

class VideoHubProtocol : public RouterProtocol {

public:
  VideoHubProtocol() {
    reset();
  }

  void parse(uint8_t byte) override {
    char _c = (char)byte;

    switch(currentState) {
      case VH_LFHeader:
        lookForHeader(_c);
        break;

      case VH_LFEndOfHeader:
        lookForEndOfHeader(_c);
        break;

      case VH_LFStartOfSource:
        lookForStartOfSource(_c);
        break;

      case VH_LFSource:
        lookForSource(_c);
        break;

      case VH_LFStartOfDest:
        lookForStartOfDest(_c);
        break;

      case VH_LFDest:
        lookForDest(_c);
        break;

      case VH_EOB:
        lookForEndOfBuffer(_c);
        break;
    }
  }

  void sendRoute(uint16_t dest, uint16_t source) override {
    // Build the VideoHub routing command
    // Format: "VIDEO OUTPUT ROUTING:\n<dest> <source>\n\n"
    routeMessage = "VIDEO OUTPUT ROUTING:\n";
    routeMessage += std::to_string(dest);
    routeMessage += " ";
    routeMessage += std::to_string(source);
    routeMessage += "\n\n";

    lastDest = dest;
    lastSource = source;
  }

  // Get the formatted message to send
  const char* getRouteMessage() {
    return routeMessage.c_str();
  }

  void reset() override {
    currentState = VH_LFHeader;
    currentHeader = "";
    currentSource = "";
    currentDest = "";
  }

  const char* getName() override {
    return "VideoHub";
  }

  bool sentPing = false;

private:
  VideoHubState currentState = VH_LFHeader;
  std::string currentHeader;
  std::string currentSource;
  std::string currentDest;
  std::string routeMessage;

  void lookForHeader(char _c) {
    if(std::isupper(_c) || _c == ' ') {
      currentHeader += _c;
      return;
    }
    else if(_c == ':') {
      currentState = VH_LFEndOfHeader;
      return;
    }
    else if(_c == '\n' && (currentHeader == "ACK" || currentHeader == "NAK")) {
      if(currentHeader == "NAK") {
        err("Got a NAK!");
      }
    }
    resetStates();
  }

  void lookForEndOfHeader(char _c) {
    if(_c == '\n') {
      if(currentHeader == "VIDEO OUTPUT ROUTING") {
        currentState = VH_LFStartOfSource;
        return;
      }
    }
    resetStates();
  }

  void lookForStartOfSource(char _c) {
    if(std::isdigit(_c)) {
      currentSource += _c;
      currentState = VH_LFSource;
      return;
    }
    resetStates();
  }

  void lookForSource(char _c) {
    if(std::isdigit(_c)) {
      currentSource += _c;
      return;
    }
    else if(_c == ' ') {
      if(!wasLastAck) {
        currentState = VH_LFStartOfDest;
        return;
      }
    }
    resetStates();
  }

  void lookForStartOfDest(char _c) {
    if(std::isdigit(_c)) {
      currentDest += _c;
      currentState = VH_LFDest;
      return;
    }
    resetStates();
  }

  void lookForDest(char _c) {
    if(std::isdigit(_c)) {
      currentDest += _c;
      return;
    }
    else if(_c == '\n') {
      if(!wasLastAck) {
        uint16_t source = static_cast<uint16_t>(std::stoi(currentSource));
        uint16_t dest = static_cast<uint16_t>(std::stoi(currentDest));

        if (source == lastSource && dest == lastDest) {
          lastSource = -1;
          lastDest = -1;
        }
        else {
          routingPairs[source] = dest;
          info("Setting Routing Pair: ", currentSource.c_str(), " ", routingPairs[source]);
        }

        currentState = VH_EOB;
        return;
      }
    }
    resetStates();
  }

  void lookForEndOfBuffer(char _c) {
    if(_c == '\n') {
      resetStates();
      return;
    }
    else if(std::isdigit(_c)) {
      resetStates();
      currentState = VH_LFSource;
      currentSource += _c;
      return;
    }
    resetStates();
    wasLastAck = false;
  }

  void resetStates() {
    currentState = VH_LFHeader;
    currentHeader = "";
    currentSource = "";
    currentDest = "";
  }

};

#endif
