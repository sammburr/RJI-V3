#include "core_pins.h"
#ifndef Buttons_h
#define Buttons_h


// This header contains helper functions, classes and defintions for interfacing with
// buttons on the interface.


#define ResetPin 41


typedef void (*ButtonCallback)(int, bool);


// Reset button is a seperate button on the back of the interface, next to the
// debug light
class Button {


private:
  int pin;
  ButtonCallback callback;
  bool lastState = false;

public:
  // @param (ButtonCallback) function to call on button press/release
  Button(int _pin, ButtonCallback _callback) {
    // Set pinmode
    pinMode(ResetPin, INPUT_PULLUP);
    pin = _pin;
    callback = _callback;
    
  }


  // Poll this button, calls the button callback
  void poll() {
    bool state = digitalReadFast(pin);
    // Flip state as this is a INPUT_PULLUP
    state = !state;

    // Callfunction if the state has changed
    if(state != lastState) {
      callback(pin, state);
    }
    lastState = state;

  }

};


#endif