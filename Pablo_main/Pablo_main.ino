#include "led_ring.h"
#include "auto_routines.h"
#include "line_tracker.h"

void setup()
{
  pixels.begin();
  Serial.begin(115200);
  //ledStart();
  setRing(2,2,2,0);

  leftDrive.attach(3);
  rightDrive.attach(4);
  pinMode(lineID,INPUT);
  IrReceiver.begin(IRpin, ENABLE_LED_FEEDBACK);
}

void loop()
{
  setRing(255,255,255,0);

  //Motor Test:
  rampUp(1500,1350,-20);
  delay(4000);
  driveControl(1350,1750);
  delay(10000);
}
