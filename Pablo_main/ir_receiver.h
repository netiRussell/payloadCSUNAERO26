#include <IRremote.hpp>

#define IRpin 3

//IR COMMANDS:
IRRawDataType delivery = 0xFC03EF00;  //0x03
IRRawDataType capture = 0xFD02EF00;   //0x02
IRRawDataType stop = 0xFE01EF00;      //0x01
IRRawDataType ESTOP = 0xFF00EF00;     //0x00