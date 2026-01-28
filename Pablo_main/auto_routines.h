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
  eyes_snap(); // Capture frame to check for yellow
  bool found = eyes_get_yellow_found();
  eyes_release(); // Release immediately; pillarPID will take its own picture if needed

  if(found)
  {
    pixels.setPixelColor(1,pixels.Color(255,255,0));
    //setRing(255,255,0,0); // Yellow
    if(pillarPID(0))
    {
      Serial.println("Pillar not centered.");
    }
    else
    {
      Serial.println("Pillar centered");
      driveControl(0,0);
     // delay(10);
     // driveControl(20,20); // Drive forward towards it
     rampUp(0,40,2);
    }
  }
  else
  {
    rampUp(0,0,0);
    setRing(255,255,255,0); // White
    driveControl(-20,20); // Spin to find it
  }
}

void gearAvoidance(int position)
{
  eyes_snap(); // Capture frame to check for pink
  uint8_t found = eyes_get_pink_count(); //equals total of discovered pink targets
  uint16_t offset = eyes_get_pink_offset_x(2); //equals the offset of the pink targets
  eyes_release(); // Release immediately; pillarPID will take its own picture if needed

  #define gearTolerance 50; //TODO: adjust accordingly
  position = position + gearTolerance;

  if(found > 0 && offset < position)
  {
    driveControl(-25,25);
    delay(100);
    driveControl(0,0);
    delay(100);
    driveControl(15,15);
    delay(100);
    driveControl(0,0);
    delay(100);
    driveContro.l(25,-25);

    eyes_snap();
    found = eyes_get_pink_count();
    eyes_release();
  }
}

void captureRoutine()
{
  IrReceiver.resume();
  if(!rampUp(0,50,10)) driveControl(50,50);
}

// Tuning constants for capture mode
#define CAPTURE_FORWARD_SPEED 20
#define CAPTURE_SCAN_HEADING 20
#define CAPTURE_PINK_GAIN 0.5    // How aggressively to turn away from pink
#define CAPTURE_YELLOW_GAIN 0.3  // How aggressively to turn toward yellow

void captureMode()
{
  eyes_snap();

  bool yellowFound = eyes_get_yellow_found();
  int16_t yellowOffset = eyes_get_yellow_offset_x();
  uint8_t pinkCount = eyes_get_pink_count();
  int16_t pinkOffset = eyes_get_pink_offset_x(0); // Get closest/largest pink

  eyes_release();

  // State priority: pinkOffset > yellowDetect > yellowOffset > default

  // 1. PINK OFFSET - highest priority, avoid pink
  if (pinkCount > 0)
  {
    forward = CAPTURE_FORWARD_SPEED;
    // Turn AWAY from pink: if pink is on right (+offset), turn left (-heading)
    heading = -pinkOffset * CAPTURE_PINK_GAIN;
    setRing(255, 0, 255, 0); // Magenta - avoiding pink
  }
  // 2. YELLOW DETECT - yellow found, drive straight
  else if (yellowFound && abs(yellowOffset) < DEADZONE)
  {
    forward = CAPTURE_FORWARD_SPEED;
    heading = 0;
    setRing(0, 255, 0, 0); // Green - locked on, driving straight
  }
  // 3. YELLOW OFFSET - yellow found but not centered, turn toward it
  else if (yellowFound)
  {
    // Don't change forward (keep previous value)
    // Turn TOWARD yellow: if yellow is on right (+offset), turn right (+heading)
    heading = yellowOffset * CAPTURE_YELLOW_GAIN;
    setRing(255, 255, 0, 0); // Yellow - centering
  }
  // 4. DEFAULT - nothing found, scan by spinning
  else
  {
    forward = 0;
    heading = CAPTURE_SCAN_HEADING; // Spin in place to scan
    setRing(255, 255, 255, 0); // White - scanning
  }

  applyDrive();
}