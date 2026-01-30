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
    driveControl(25,-25);

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

// Debug: capture and print what camera sees
void captureDebug()
{
  eyes_snap();

  bool yellowFound = eyes_get_yellow_found();
  int16_t yellowOffset = eyes_get_yellow_offset_x();
  uint16_t yellowArea = eyes_get_yellow_area();
  uint8_t pinkCount = eyes_get_pink_count();
  int16_t pinkOffset0 = eyes_get_pink_offset_x(0);
  int16_t pinkOffset1 = eyes_get_pink_offset_x(1);

  eyes_release();

  // Set all pixels white first
  for (int i = 0; i < NUMPIXELS; i++)
  {
    pixels.setPixelColor(i, pixels.Color(255, 255, 255));
  }

  // Top pixel off if yellow found
  if (yellowFound)
  {
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
  }

  // Bottom pixel off if pink found
  if (pinkCount > 0)
  {
    pixels.setPixelColor(4, pixels.Color(0, 0, 0));
  }

  pixels.show();

  Serial.println("=== CAPTURE DEBUG ===");
  Serial.print("Yellow found: "); Serial.println(yellowFound ? "YES" : "NO");
  Serial.print("Yellow offset: "); Serial.println(yellowOffset);
  Serial.print("Yellow area: "); Serial.println(yellowArea);
  Serial.print("Pink count: "); Serial.println(pinkCount);
  Serial.print("Pink[0] offset: "); Serial.println(pinkOffset0);
  Serial.print("Pink[1] offset: "); Serial.println(pinkOffset1);
  Serial.println("=====================");

  delay(1000);
}

// Tuning constants for capture mode
#define YELLOW_FORWARD_SPEED 20
#define PINK_FORWARD_SPEED 15
#define CAPTURE_SCAN_HEADING 20
#define CAPTURE_PINK_GAIN 0.9    // How aggressively to turn away from pink
#define CAPTURE_YELLOW_GAIN 0.4  // How aggressively to turn toward yellow

//PSEUDO CODE FOR VECTOR CAPTURE
/*

for = 0;
turn = 0;

{
  if(pinkCount > 0)
  {
    for = PINK_FORWARD_SPEED * CAPTURE_PINK_GAIN;
    turn = PINK_FORWARD_SPEED; //should be multiplied by a polarity relative to the position of the pink
  }
  else if(yellowFound)
  {
    for = YELLOW_FORWARD_SPEED;

    if(abs(yellowOffset) > DEADZONE)
    {
      y_turn = (YELLOW_FORWARD_SPEED * (abs(yellowOffset))/(yellowOffset))*CAPTURE_YELLOW_GAIN);
    }
    else
    {
      turn = 0;
    }
  }
  else
  {
    for = 0;
    turn = YELLOW_FOWARD_SPEED;
  }

  driveControl(for+turn, -for+turn);
}

*/

// Track previous state for scan-to-yellow transition
static bool wasScanning = false;

void captureMode()
{
  eyes_snap();

  bool yellowFound = eyes_get_yellow_found();
  int16_t yellowOffset = eyes_get_yellow_offset_x();
  uint8_t pinkCount = eyes_get_pink_count();
  int16_t pinkOffset = eyes_get_pink_offset_x(0);

  eyes_release();

  int fwd = 0;
  int turn = 0;

  // 1. PINK - highest priority, avoid
  if (pinkCount > 0)
  {
    fwd = PINK_FORWARD_SPEED * CAPTURE_PINK_GAIN;
    // Turn away: polarity based on pink position
    turn = (pinkOffset > 0) ? -PINK_FORWARD_SPEED : PINK_FORWARD_SPEED;
    pixels.setPixelColor(1, pixels.Color(255, 0, 255)); // magenta
    pixels.show();
    wasScanning = false;
  }
  // 2. YELLOW - drive toward it
  else if (yellowFound)
  {
    fwd = YELLOW_FORWARD_SPEED;

    // Just transitioned from scan? Counter-rotate slightly to kill spin momentum
    if (wasScanning)
    {
      turn = -CAPTURE_SCAN_HEADING * 0.5; // Brief counter-turn (opposite of scan direction)
      wasScanning = false;
    }
    else if (abs(yellowOffset) > DEADZONE)
    {
      // Bang-bang: fixed turn magnitude, direction from offset sign
      turn = (yellowOffset > 0) ? YELLOW_FORWARD_SPEED * CAPTURE_YELLOW_GAIN
                                : -YELLOW_FORWARD_SPEED * CAPTURE_YELLOW_GAIN;
    }
    else
    {
      turn = 0; // Centered, go straight
    }
    pixels.setPixelColor(1, pixels.Color(255, 255, 0)); // yellow
    pixels.show();
  }
  // 3. DEFAULT - scan
  else
  {
    fwd = 0;
    turn = CAPTURE_SCAN_HEADING;
    setRing(255, 255, 255, 0); // white
    wasScanning = true; // Mark that we were scanning
  }

  //Serial.print("fwd: "); Serial.println(fwd);
  //Serial.print("turn: "); Serial.println(turn);

  Serial.print("left: "); Serial.println(fwd+turn);
  Serial.print("right: "); Serial.println(fwd-turn);

  driveControl(fwd + turn, fwd - turn);
}