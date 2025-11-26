#include "motor_control.h"
#include "ir_receiver.h"
#include "pid.h"
#include "eyes.h"

void lineSearch(bool sensorIn)
{
  driveControl(-20,-20);
  if(sensorIn == 0)
  {
    
    //driveControl(1580,1580);
  }
  else if(sensorIn == 1)
  {
    driveControl(0,0);
    delay(500);
    Serial.println("On line");
    IrReceiver.resume();
  }
}

void findPillar()
{
  pillarPID(0);
  Serial.println("Pillar found");
}

void gearAvoidance(int position)
{
  
}

void captureRoutine()
{
  IrReceiver.resume();
  rampUp(1500,1650,10);
}