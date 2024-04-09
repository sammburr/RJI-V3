#include "core_pins.h"
#ifndef Buttons_h
#define Buttons_h

#include <Bounce.h>

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
  Bounce* debounced;

public:
  // @param (ButtonCallback) function to call on button press/release
  Button(int _pin, ButtonCallback _callback) {
    // Set pinmode
    pinMode(_pin, INPUT_PULLUP);
    pin = _pin;
    callback = _callback;
    
    // Create a debounced button
    debounced = new Bounce(_pin, 40);

  }


  // Poll this button, calls the button callback
  void poll() {
    // bool state = digitalRead(pin);
    // // Flip state as this is a INPUT_PULLUP
    // state = !state;

    // // Callfunction if the state has changed
    // if(state != lastState) {
    //   callback(pin, state);
    // }
    // lastState = state;

    // Debounced Solution
    if(debounced->update()) {
      if(debounced->fallingEdge()) {
        callback(pin, true);
      }
      else {
        callback(pin, false);
      }
    }

  }

};


// GPIS start pin
#define GPI_First_Pin 29


// Array of Buttons 
Button* GPIs[12];


// Helper function to init all buttons
void startButtons(ButtonCallback _callback) {
  info("Populating button array...");

  for(byte i=0; i<12; i++) {
    GPIs[i] = new Button(GPI_First_Pin + i, _callback);

  }

}


// Helper function to poll all buttons
void pollButtons() {
  for(byte i=0; i<12; i++) {
    GPIs[i]->poll();

  }

}

#endif