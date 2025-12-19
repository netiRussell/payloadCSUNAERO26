#include "motor_control.h"
#include "ir_receiver.h"
#include "pid.h"
#include "eyes.h"
#include "led_ring.h"

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
  setRing(255,255,255,0);

  if(eyes_get_yellow_found())
  {
    if(pillarPID(0))
    {
      Serial.println("Pillar not centered.");
    }
    else
    {
      Serial.println("Pillar centered");
      driveControl(20,20);
      setRing(255,255,0,0);
    }
  }
  else
  {
    driveControl(-20,20);
  }
}

void gearAvoidance(int position)
{
  
}

void captureRoutine()
{
  IrReceiver.resume();
  if(!rampUp(0,50,10)) driveControl(50,50);
}