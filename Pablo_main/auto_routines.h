#include "motor_control.h"
#include "ir_receiver.h"
#include "pid.h"
#include "eyes.h"

void lineSearch(bool sensorIn)
{
  if(sensorIn == 0)
  {
    driveControl(-25,-25);
    Serial.println("not on line");
  }
  else if(sensorIn == 1)
  {
    driveControl(0,0);
    delay(100);
    Serial.println("On line");
    IrReceiver.resume();
  }
}

void findPillar()
{
  if(!pillarPID(0))
  {
    Serial.println("Pillar found");
  }

  driveControl(30,30);
}

void gearAvoidance(int position)
{
  
}

void captureRoutine()
{
  IrReceiver.resume();
  if(!rampUp(0,50,10)) driveControl(50,50);
}