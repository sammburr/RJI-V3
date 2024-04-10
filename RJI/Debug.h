#ifndef Debug_h
#define Debug_h


// This header has some helper functions and classes involved in debugging
// the program


// Debug Light, is the led at the front of the interface, this class
// provides a nice way of using the led
// this led is two leds (red and green) at pins 14 and 15 respectively


#define RedDebugPin 14
#define GreenDebugPin 15


class DebugLight {


public:
  DebugLight() {
    // Set pin modes
    pinMode(RedDebugPin, OUTPUT);
    pinMode(GreenDebugPin, OUTPUT);
  }


  void red() {
    digitalWrite(RedDebugPin, HIGH);
    digitalWrite(GreenDebugPin, LOW);
  }


  void green() {
    digitalWrite(RedDebugPin, LOW);
    digitalWrite(GreenDebugPin, HIGH);
  }


  void amber() {
    digitalWrite(RedDebugPin, HIGH);
    digitalWrite(GreenDebugPin, HIGH);
  }


  void off() {
    digitalWrite(RedDebugPin, LOW);
    digitalWrite(GreenDebugPin, LOW);    
  }


private:

};


// Singleton instance of this class, can be used else where 
// by just using; DebugLight.red(), etc.
DebugLight DebugLight;


// This macro adds an 'origin' of sorts to the start of a print function
// This looks nice and is good for debugging when a program has lots of
// headers and classes in seperate files
#define FILENAME (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define info(...) Debug.printInfo(FILENAME, __VA_ARGS__)
#define err(...) Debug.printError(FILENAME, __VA_ARGS__)


// Helper singleton with some functions for ease of use
class Debug {


public:
  // Start and wait for Serial output
  void startSerial() {
    Serial.begin(115200);
    delay(100);
  }


  // The following are nicer ways of formatting the Serial print metods
  
  
  // print out a nice looking message, styled as `title`
  // @param (T) message to add to title
  template <typename T>
  void printTitle(T _message) {
    Serial.print("================================================================================================================================================\n ");
    Serial.println(_message);
    Serial.println("================================================================================================================================================");
  }


  // print out a nice looking message, styled as `subtitle`
  // @param (T) message as subtitle
  template <typename T>
  void printSubTitle(T _message) {
    Serial.print("------------------------------------------------------------------------------------------------------------------------------------------------\n ");
    Serial.println(_message);
    Serial.println("------------------------------------------------------------------------------------------------------------------------------------------------");

  }

  
  // print out a nice looking message, styled as `info`
  // @param (T) header is placed at the front of a message, 
  //            this can be anything such as an author of the
  //            message
  // @param (Args) Any amount of subsequent objects to print out
  template<typename T, typename... Args>
  void printInfo(T _header, Args... _args) {
    Serial.print(_header);
    Serial.print("(i)$: ");
    print(_args...);

  }

template<typename T, typename... Args>
  void printError(T _header, Args... _args) {
    Serial.print(_header);
    Serial.print("(!ERR!)$: ");
    print(_args...);

  }


  template<typename T, typename... Args>
  void print(T _message, Args... _args) {
    Serial.print(_message);
    print(_args...);

  } 

  template<typename T>
  void print(T _message) {
    Serial.println(_message);

  }


};


// Singleton instance of Debug
Debug Debug;


#endif