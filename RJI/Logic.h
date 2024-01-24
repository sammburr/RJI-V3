#ifndef Logic_h
#define Logic_h


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


  // Helper function to create and populate engineers


};





Logic Logic;


#endif