#include <ESP32Servo.h>

Servo leftDrive;
Servo rightDrive;

void driveControl(int left, int right) //1500 is STOP. <1500 is REV. >1500 is FOR
{
  left = (left - 300) * (0.015*left) + 1500;
  right = (right - 300) * (0.015*right) + 1500;
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