#include "eyes.h"

float p_mod = 0.3;//TUNE
float p = 0;
float maxSpeed = 0;
float max_mod = 2;//TUNE
#define DEADZONE 10
bool pillarPID(float heading = 0)
{
  eyes_snap();

  int h = fabs(heading);
  p = -p_mod * h; //proportionality
  maxSpeed = h/max_mod; //limiter

  int16_t offset = eyes_get_yellow_offset_x();
  if (offset == 0) offset = 1;
  Serial.print("offset: ");
  Serial.println( offset);
  int speedMod = ((h/offset)*p);

  int speedOutLeft = speedMod;
  if(speedOutLeft > maxSpeed) speedOutLeft = maxSpeed;

  int speedOutRight = -speedOutLeft;

  bool result;
  if(abs(eyes_get_yellow_offset_x()) > heading + DEADZONE)
  {
    driveControl(speedOutLeft,speedOutRight);
    //leftDrive.writeMicroseconds(speedOutLeft);
    //rightDrive.writeMicroseconds(speedOutRight);
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