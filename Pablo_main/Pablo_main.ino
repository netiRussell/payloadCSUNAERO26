#include "auto_routines.h"
#include "line_tracker.h"

void setup()
{
  Serial.begin(115200);
  delay(1000); // Give serial time to initialize

  Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
  pixels.begin();
  //ledStart();
  //setRing(2,2,2,0);
   Serial.println("Initializing camera...");
  if (!eyes_init()) {
    Serial.println("CAMERA INIT FAILED!");
    setRing(255, 0, 0, 0); // RED for error
    //while(1); // Stop here if camera fails
    delay(1000);
  }
  else{
  delay(2000); // Give camera time to stabilize
  Serial.println("Camera ready!");
  setRing(0,255,0,0);
  delay(1000);
  }

  leftDrive.attach(4);
  rightDrive.attach(5);
  pinMode(lineID,INPUT);
  IrReceiver.begin(IRpin, ENABLE_LED_FEEDBACK);
  driveControl(0,0);

   ledIdle();
  delay(1000);
 
}

//takes picture every second and changes LED based on detection
void testDetection()
{
  eyes_snap();

  if (eyes_get_yellow_found()) {
    //setRing(255, 255, 0, 0);
    pixels.setPixelColor(1,pixels.Color(255,255,0));
    Serial.println("YELLOW detected!");
  }
  //else if (eyes_get_pink_count() > 0) {
  //  setRing(255, 20, 147, 0);
  //  Serial.println("PINK detected!");
 // }
  else {
    setRing(255, 255, 255, 0); 
    Serial.println("Nothing detected");
  }

  eyes_release();
  delay(1000);  // Wait 1 second
}

void loop()
{
  //setRing(255,255,255,0);
 
  if(IrReceiver.decode())
  {
    if(IrReceiver.decodedIRData.decodedRawData == delivery)
    {
      Serial.println("Delivery Start");
      lineSearch(lineVal());
      IrReceiver.resume();
    }
    else if(IrReceiver.decodedIRData.decodedRawData == capture)
    {
      Serial.println("Capture Start");
      findPillar();
    }
    else if(IrReceiver.decodedIRData.decodedRawData == stop)
    {
      driveControl(0,0);
    }
    else if(IrReceiver.decodedIRData.decodedRawData == ESTOP)
    {
      leftDrive.detach();
      rightDrive.detach();
    }
  }
  //testDetection();
  //lineSearch(lineVal());

  //findPillar();

  //Serial.println(eyes_get_yellow_offset_x());
}
