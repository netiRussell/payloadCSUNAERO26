#include "motor_control.h"
#include "ir_receiver.h"
#include "pid.h"
#include "eyes.h"

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
  pillarPID(eyes_get_yellow_offset_x());
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