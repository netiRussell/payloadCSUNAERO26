#include "led_ring.h"
#include "auto_routines.h"
#include "line_tracker.h"

void setup()
{
  Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
  pixels.begin();
  Serial.begin(115200);
  //ledStart();
  setRing(2,2,2,0);

  eyes_init(); // Initialize vision system

  leftDrive.attach(4);
  rightDrive.attach(5);
  pinMode(lineID,INPUT);
  IrReceiver.begin(IRpin, ENABLE_LED_FEEDBACK);
}

void loop()
{
  setRing(255,255,255,0);
  

  if(IrReceiver.decode())
  {
    if(IrReceiver.decodedIRData.decodedRawData == 0xFC03EF00)
    {
      lineSearch(lineVal());
      IrReceiver.resume();
    }
  }
  else
  driveControl(0,0);
}
