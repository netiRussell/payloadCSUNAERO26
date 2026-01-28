#include <ESP32Servo.h>

Servo leftDrive;
Servo rightDrive;

// Global motion variables
int forward = 0;
int heading = 0;



void driveControl(int left, int right) //0 is STOP. -100 is REV. 100 is FOR
{
  int leftSpeed = (-5*left) + 1500;
  int rightSpeed = (-5*right) + 1500;
  leftDrive.writeMicroseconds(leftSpeed);
  rightDrive.writeMicroseconds(rightSpeed);
}

// Apply forward/heading to motors
void applyDrive()
{
  int right = -forward + heading;
  int left = forward - heading;

  driveControl(left,right);
}

bool ramping = false;

int currentSpeed = 0;

bool rampUp(int initSpeed, int finalSpeed, int cycles) //Slow initSpeed, high finalSpeed, modulate cycles as needed
{
  currentSpeed = initSpeed + currentSpeed + cycles;
  if(abs(currentSpeed - finalSpeed) > 0)
  {
   driveControl(currentSpeed,currentSpeed);
   return true;
   delay(10);
  }
  else
  {
    driveControl(finalSpeed,finalSpeed);
    //currentSpeed = 0;
    return false;
  }
}