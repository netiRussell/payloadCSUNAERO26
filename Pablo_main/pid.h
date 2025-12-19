#include "eyes.h"

float p_mod = 0.1;//TUNE
float p = 0;
float maxSpeed = 0;
float max_mod = 1.4;//TUNE
#define DEADZONE 1

bool pillarPID(float heading = 0)
{
  eyes_snap();

  int h = fabs(heading);
  p = -p_mod * h; //proportionality
  maxSpeed = h/max_mod; //limiter

  int16_t offset = eyes_get_yellow_offset_x();
  if (offset == 0) offset = 1;
  Serial.println(offset);
  int speedMod = ((h/offset)*p);
  int speedOut = 1500+speedMod;

  int speedOutLeft = speedOut;
  if(speedOutLeft > maxSpeed) speedOutLeft = maxSpeed;

  int speedOutRight = speedOutLeft - 1500;

  bool result;
  if(abs(eyes_get_yellow_offset_x()) > heading + DEADZONE)
  {
    //driveControl(speedOutLeft,speedOutRight);
    leftDrive.writeMicroseconds(speedOutLeft);
    rightDrive.writeMicroseconds(speedOutRight);
    result = true;
  }
  else
  {
    driveControl(0,0);
    result = false;
  }

  eyes_release();
  return result;
}