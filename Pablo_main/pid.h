#include "camera.h"

float p_mod = 0.1;//TUNE
float p = 0;
float maxSpeed = 0;
float max_mod = 1.4;//TUNE
#define DEADZONE 5

void pillarPID(float heading = 0)
{
  int h = fabs(heading);
  p = -p_mod * h; //proportionality
  maxSpeed = h/max_mod; //limiter

  int speedMod = ((h/tempCam())*p);
  int speedOut = 1500+speedMod;
  if(speedOutLeft > maxSpeed) speedOutLeft = maxSpeed;
  int speedOutRight = speedOutLeft - 1500;

  while(abs(tempCam()) > heading + DEADZONE)
  {
    driveControl(speedOutLeft,speedOutRight);
  }
  driveControl(1500,1500);
}