#include "motor_control.h"
#include "ir_receiver.h"
#include "pid.h"

void lineSearch(bool sensorIn)
{
  if(sensorIn == 0)
  {
    rampUp(1500,1350,-20);
  }
  else if(sensorIn == 1)
  {
    driveControl(1500,1500);
    delay(500);
    Serial.println("On line");
    IrReceiver.resume();
  }
}

void findPillar()
{
  pillarPID(tempCam());
  Serial.println("Pillar found");
}

void captureRoutine()
{
  IrReceiver.resume();
  rampUp(1500,1650,10);
}