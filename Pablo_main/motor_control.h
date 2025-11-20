#include <ESP32Servo.h>

Servo leftDrive;
Servo rightDrive;

void driveControl(int left, int right) //1500 is STOP. <1500 is REV. >1500 is FOR
{
  leftDrive.writeMicroseconds(left);
  rightDrive.writeMicroseconds(right);
}

void rampUp(int initSpeed, int finalSpeed, int cycles) //Slow initSpeed, high finalSpeed, modulate cycles as needed
{
  static int currentSpeed = 1500;
  currentSpeed = initSpeed + currentSpeed + cycles - 1500;
  if(abs(currentSpeed - finalSpeed) > 0)
  {
   driveControl(currentSpeed,currentSpeed);
  }
  else
  {
    driveControl(finalSpeed,finalSpeed);
    currentSpeed = 1500;
  }
}