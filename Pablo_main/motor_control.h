#include <ESP32Servo.h>

Servo leftDrive;
Servo rightDrive;

void driveControl(int left, int right) //0 is STOP. -100 is REV. 100 is FOR
{
  left = (-5*left) + 1500;
  right = (-5*right) + 1500;
  leftDrive.writeMicroseconds(left);
  rightDrive.writeMicroseconds(right);
}

bool ramping = false;

void rampUp(int initSpeed, int finalSpeed, int cycles) //Slow initSpeed, high finalSpeed, modulate cycles as needed
{
  static int currentSpeed = 0;
  currentSpeed = initSpeed + currentSpeed + cycles;
  if(abs(currentSpeed - finalSpeed) > 0)
  {
   driveControl(currentSpeed,currentSpeed);
   ramping = true;
   delay(10);
  }
  else
  {
    driveControl(finalSpeed,finalSpeed);
    //currentSpeed = 0;
    ramping = false;
  }
}