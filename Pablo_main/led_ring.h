#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

#define PIN 43
#define NUMPIXELS 8

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setRing(uint8_t r, uint8_t g, uint8_t b, int delayVal) //Set RGB vals to 0-255 for brightness. delayVal is if you want it to light up sequentially.
{
  pixels.clear();
  for(int i=0; i<8; i++)
  {
    pixels.setPixelColor(i, pixels.Color(r,g,b));
    pixels.show();

    delay(delayVal);
  }
}

void ledStart() //Premade LED boot up routine
{
  setRing(255,0,0,200);
  delay(300);
  setRing(0,255,0,0);
  delay(300);
  setRing(0,0,0,0);
}