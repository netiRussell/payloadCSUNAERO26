#define DISABLE_CODE_FOR_RECEIVER

#define IR_SEND_PIN 3

#include <Arduino.h>
#include "PinDefinitionsAndMore.h"
#include <IRremote.hpp> 

#define NEC_ADDR 0x00
#define NEC_COMMAND 0x34
#define NEC_NUM_REPEATS 0

#define NEC_COMMAND_DRIVE 0x01
#define NEC_COMMAND_CAPTURE 0x02
#define NEC_COMMAND_STOP 0x03
#define NEC_COMMAND_E_STOP 0x04

void setup() {
    // Enable the feedback LED
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(115200);

    IrSender.begin(ENABLE_LED_FEEDBACK);
}

void loop() {
    // Print current send values
    Serial.println("Loop instance");
    Serial.println(F("Send standard NEC with 8 bit address"));
    Serial.flush();

    // Send the NEC frame
    IrSender.sendNEC(NEC_ADDR, NEC_COMMAND_DRIVE, NEC_NUM_REPEATS);
    delay(1000);
    /*
    // Send the NEC frame
    IrSender.sendNEC(NEC_ADDR, NEC_COMMAND_CAPTURE, NEC_NUM_REPEATS);
    delay(1000);
    // Send the NEC frame
    IrSender.sendNEC(NEC_ADDR, NEC_COMMAND_STOP, NEC_NUM_REPEATS);
    delay(1000);
    // Send the NEC frame
    IrSender.sendNEC(NEC_ADDR, NEC_COMMAND_E_STOP, NEC_NUM_REPEATS);
    delay(1000);
    */
}