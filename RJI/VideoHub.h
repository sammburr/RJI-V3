#include <string>
#include <sys/types.h>
#ifndef VideoHub_h
#define VideoHub_h

#include <string.h>
#include <cctype>


// TODO
// Save sources/dest to the array
// implement checking for an ACK!


// This header defines a singleton to help parse BlackMagic's super cool and funny 
// net protocol

enum State {

  LFHeader,
  LFEndOfHeader,
  LFStartOfDest,
  LFDest,
  LFStartOfSource,
  LFSource,
  EOB

};

class VideoHub {

public:

  void parse(char _c) {
    switch(currentState) {

      case LFHeader:
        lookForHeader(_c);
        break;
      
      case LFEndOfHeader:
        lookForEndOfHeader(_c);
        break;

      case LFStartOfDest:
        lookForStartOfDest(_c);
        break;

      case LFDest:
        lookForDest(_c);
        break;

      case LFStartOfSource:
        lookForStartOfSource(_c);
        break;

      case LFSource:
        lookForSource(_c);
        break;

      case EOB:
        lookForEndOfBuffer(_c);
        break;

    }

  }

  bool wasLastAck = false;

  // List of routing parings
  // index = dest
  // value = source
  u_int16_t routingPairs[1024];

private:


  // Current state
  State currentState = LFHeader;

  // Current vars
  std::string currentHeader;
  std::string currentSource;
  std::string currentDest;

  void lookForHeader(char _c) {

    // If char is uppercase or a space, we continue to add to the header
    if(std::isupper(_c) || _c == ' ') {
      currentHeader += _c;
      return;

    }
    // if the char is a ':', we then want to watch for the end of the header
    else if(_c == ':') {
      currentState = LFEndOfHeader;

      return;  

    }
    // if the char is a new line AND the header so far == "ACK", we have got an ack
    else if(_c == '\n' && currentHeader == "ACK") {
      wasLastAck = true;
    }
    // if the char is anything else we got some wrong info, just return
    // and print a message
    //err("Got an invalid message from VideoHub: Bad Header!");
    resetStates();

  }

  void lookForEndOfHeader(char _c) {
    // If the char is a new line then we have reached the end of
    // the header
    if(_c == '\n') {

      // do some more processing depending on the header we got
      if(currentHeader == "VIDEO OUTPUT ROUTING") {
        currentState = LFStartOfDest;
        return;

      }

    }
    // Anything else -> return with an error
    resetStates();
    //err("Got an invalid message from VideoHub: Bad End of Header!");

  }

  void lookForStartOfDest(char _c) {
    // If the char is a numeric then we can add it to
    // the source string
    if(std::isdigit(_c)) {
      // Add to start of current source
      currentDest += _c;
      currentState = LFDest;
      return;

    }
    // Anything else -> return with an error
    resetStates();
    //err("Got an invalid message from VideoHub: Bad Start of Source!");

  }

  void lookForDest(char _c) {
    // If the char is numeric then continue to add to source
    if(std::isdigit(_c)) {
      currentDest += _c;
      return;
    }
    // If we get a spcae, we want to move on to the destination
    else if(_c == ' ') {
      if(!wasLastAck) {
        currentState = LFStartOfSource;
        return;
      }
    }

    // Anything else -> return with an error
    resetStates();
    //err("Got an invalid message from VideoHub: Bad Start of Source!");

  }

  void lookForStartOfSource(char _c) {
    // If the char is a numeric then we can add it to
    // the dest string
    if(std::isdigit(_c)) {
      // Add to start of current source
      currentSource += _c;
      currentState = LFSource;
      return;

    }
    // Anything else -> return with an error
    resetStates();
    //err("Got an invalid message from VideoHub: Bad Start of Dest!");

  }

  void lookForSource(char _c) {
     // If the char is numeric then continue to add to source
    if(std::isdigit(_c)) {
      currentSource += _c;
      return;
    }
    // If we get a spcae, we want to move on to the next pair
    else if(_c == '\n') {
      if(!wasLastAck) {
        routingPairs[std::stoi(currentDest)] = static_cast<u_int16_t>(std::stoi(currentSource));
        info("Setting Routing Pair: ", currentDest.c_str(), " ", routingPairs[std::stoi(currentDest)]);

        currentState = EOB;
        return;
      }
    }

    // Anything else -> return with an error
    resetStates();
    //err("Got an invalid message from VideoHub: Bad Start of Source!");   

  }

  void lookForEndOfBuffer(char _c) {
    // If this is a new line, the buffer is done!
    if(_c == '\n') {
      resetStates();
      return;

    }
    // If it is a digit, we wanna go back to looking for a source
    else if(std::isdigit(_c)) {
      resetStates();
      currentState = LFDest;
      currentDest += _c;

      return;

    }
    // Anything else -> return with an error
    resetStates();
    //err("Got an invalid message from VideoHub: Bad Start of Source!");   
    wasLastAck = false;

  }

  void resetStates() {
    currentState = LFHeader;
    currentHeader = "";
    currentSource = "";
    currentDest = "";

  }

};
VideoHub VideoHub;



#endif