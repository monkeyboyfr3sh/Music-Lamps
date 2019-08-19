#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include "reactive_common.h"

#define LAMP_ID 1  //Set ID

//LED Variables
#define LED_PIN 4
#define NUM_LEDS 1
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#define FASTLED_ALLOW_INTERRUPTS 0
//end of LED Varibles

//MIC&Audio Processing
#define MIC_LOW 1500
#define MIC_HIGH 1900
#define SAMPLE_SIZE 15
#define LONG_TERM_SAMPLES 250
#define BUFFER_DEVIATION 400
#define BUFFER_SIZE 15
//end of mic/audio

//WiFi Setup
WiFiUDP UDP;
const char *ssid = "myHost"; // The SSID (name) of the Wi-Fi network you want to connect to
const char *password = "123456789";  // The password of the Wi-Fi network
//End of Wifi

CRGB leds[NUM_LEDS];

//Structs to hold sample data
struct averageCounter *samples;
struct averageCounter *longTermSamples;
struct averageCounter* sanityBuffer;
//End of structs

//Color Variables
float globalHue;
double globalBrightness = 1; //variable for adjustint brightness, values 0<=brightness desiered<=1
int hueOffset = 240;
float fadeScale = 1.3;
float hueIncrement = 0.25;
int scrollTimer = 0;

int wc = 0;
//End of color varibale

struct led_command {
  uint8_t opmode;
  uint32_t data;
  uint8_t redcolor;
  uint8_t greencolor;
  uint8_t bluecolor;
};

int redcolor;
int greencolor;
int bluecolor;

unsigned long lastReceived = 0;
unsigned long lastHeartBeatSent;
const int heartBeatInterval = 100;
int opMode;
bool fade = false;

struct led_command cmd;
void connectToWifi();

void setup() {
  globalHue = 0;
  if ((globalBrightness < 0) || (globalBrightness > 1)) {
    globalBrightness = 1;
  }

  samples = new averageCounter(SAMPLE_SIZE);
  longTermSamples = new averageCounter(LONG_TERM_SAMPLES);
  sanityBuffer    = new averageCounter(BUFFER_SIZE);

  while (sanityBuffer->setSample(250) == true) {}
  while (longTermSamples->setSample(200) == true) {}

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection( 0xD4FFFF );

  Serial.begin(115200); // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println('\n');

  WiFi.begin(ssid, password); // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(ssid);

  IPAddress ip(192, 168, 4, 100 + LAMP_ID);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(ip, gateway, subnet);
  Serial.println(" ...");

  connectToWifi();
  sendHeartBeat();
  UDP.begin(7001);
  Serial.println(ip);
}

void loop() {
  Serial.print("Op Mode");
  Serial.println(opMode);
  int analogRaw;

  scrollTimer++;
  if (scrollTimer >= 38) {
    scrollTimer = 0;
    hueOffset++;
  }

  if (hueOffset >= 255) {
    hueOffset = 0;
  }
  if (millis() - lastHeartBeatSent > heartBeatInterval) {
    sendHeartBeat();
  }
  if (millis() - lastReceived >= 5000) {
    connectToWifi();
  }
  int packetSize = UDP.parsePacket();
  if (packetSize) {
    UDP.read((char *)&cmd, sizeof(struct led_command));
    lastReceived = millis();
  }
  opMode = cmd.opmode;
  analogRaw = cmd.data;
  if (opMode == 6) {
    redcolor = cmd.redcolor;
    greencolor = cmd.greencolor;
    bluecolor = cmd.bluecolor;
  }

  switch (opMode) {
    case 0://Soft Sleep
      FastLED.clear();
      FastLED.show();
      break;

    case 1://Sound React
      fade = false;
      soundReactive(analogRaw);
      break;

    case 2://All White
      fade = false;
      allWhite();
      break;

    case 3://Fade
      chillFade();
      break;

    case 4://Larson
      fade = false;
      //larsonScanner - speed delay
      larsonScanner(60);
      break;

    case 5://Rainbow
      fade = false;
      // rainbowCycle - speed delay
      rainbowCycle(5);
      break;

    case 6://Color Select
      fade = false;
      ColorSelect(redcolor, greencolor, bluecolor);
      break;
  }
}//end of main

void sendHeartBeat() {
  struct heartbeat_message hbm;
  hbm.client_id = LAMP_ID;
  hbm.chk = 77777;
  Serial.println("Sending heartbeat");
  IPAddress ip(192, 168, 4, 1);
  UDP.beginPacket(ip, 7171);
  int ret = UDP.write((byte*)&hbm, sizeof(hbm));
  printf("Returned: %d, also sizeof hbm: %d \n", ret, sizeof(hbm));
  UDP.endPacket();
  lastHeartBeatSent = millis();
}

void connectToWifi() {
  WiFi.mode(WIFI_STA);
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CHSV(0, 0, 0);
  }
  leds[0] = CRGB(0, 255, 0);
  FastLED.show();

  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i);
    Serial.print(' ');
  }
  Serial.println('\n');
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP()); // Send the IP address of the ESP8266 to the computer
  leds[0] = CRGB(0, 0, 255);
  FastLED.show();
  lastReceived = millis();
}

boolean wifiBreak() {
  int packetSize = UDP.parsePacket();
  if (packetSize) {
    UDP.read((char *)&cmd, sizeof(struct led_command));
    lastReceived = millis();
  }

  if (cmd.opmode != opMode) return true;
}

void softSleep() {//Function to set the LEDs off
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(0, 0, 0);
  }
  delay(5);
  FastLED.show();

}//end of allWhite

void allWhite() {//Function to set the LEDs to white
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(globalBrightness * 255, globalBrightness * 255, globalBrightness * 255);
  }
  delay(5);
  FastLED.show();

}//end of allWhite

void chillFade() {
  static int fadeVal = 0;
  static int counter = 0;
  static int from[3] = {0, 234, 255};
  static int to[3]   = {255, 0, 214};
  static int i, j;
  static double dsteps = 500.0;
  static double s1, s2, s3, tmp1, tmp2, tmp3;
  static bool reverse = false;
  if (fade == false) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB(from[0], from[1], from[2]);
    }
    s1 = double((to[0] - from[0])) / dsteps;
    s2 = double((to[1] - from[1])) / dsteps;
    s3 = double((to[2] - from[2])) / dsteps;
    tmp1 = from[0], tmp2 = from[1], tmp3 = from[2];
    fade = true;
  }

  if (!reverse)
  {
    tmp1 += s1;
    tmp2 += s2;
    tmp3 += s3;
  }
  else
  {
    tmp1 -= s1;
    tmp2 -= s2;
    tmp3 -= s3;
  }

  for (j = 0; j < NUM_LEDS; j++)
    leds[j] = CRGB(globalBrightness * tmp1, globalBrightness * tmp2, globalBrightness * tmp3);
  FastLED.show();
  delay(5);

  counter++;
  if (counter == (int)dsteps) {
    reverse = !reverse;
    tmp1 = to[0], tmp2 = to[1], tmp3 = to[2];
    counter = 0;
  }
}//end of chill fade


//ANIMATIONS FROM https://www.tweaking4all.com/hardware/arduino/arduino-all-ledstrip-effects-in-one/

//These are base functions for the animations
// Apply LED color changes
void showStrip() {
#ifdef ADAFRUIT_NEOPIXEL_H
  // NeoPixel
  strip.show();
#endif

#ifndef ADAFRUIT_NEOPIXEL_H
  // FastLED
  FastLED.show();
#endif
}

// Set all LEDs to a given color and apply it (visible)
void setAll(byte red, byte green, byte blue) {
  for (int i = 0; i < NUM_LEDS; i++ ) {
    setPixel(i, red, green, blue);
  }
  showStrip();
}

// Set a LED color (not yet visible)
void setPixel(int Pixel, byte red, byte green, byte blue) {
#ifdef ADAFRUIT_NEOPIXEL_H
  // NeoPixel
  strip.setPixelColor(Pixel, strip.Color(red, green, blue));
#endif
#ifndef ADAFRUIT_NEOPIXEL_H
  // FastLED
  leds[Pixel].r = globalBrightness * red;
  leds[Pixel].g = globalBrightness * green;
  leds[Pixel].b = globalBrightness * blue;
#endif
}

//Animations
void colorWipe(byte red, byte green, byte blue, int SpeedDelay) {
  for (uint16_t i = 0; i < NUM_LEDS; i++) {

    int packetSize = UDP.parsePacket();
    if (packetSize) {
      UDP.read((char *)&cmd, sizeof(struct led_command));
      lastReceived = millis();
    }

    if (cmd.opmode != 4) break;

    setPixel(i, red, green, blue);
    showStrip();
    delay(SpeedDelay);
  }
}

void colorWipeInvert(byte red, byte green, byte blue, int SpeedDelay) {
  for (uint16_t i = NUM_LEDS - 1; i > 0; i--) {

    int packetSize = UDP.parsePacket();
    if (packetSize) {
      UDP.read((char *)&cmd, sizeof(struct led_command));
      lastReceived = millis();
    }

    if (cmd.opmode != 4) break;

    setPixel(i, red, green, blue);
    showStrip();
    delay(SpeedDelay);
  }
}

void PixelSwipe(byte red, byte green, byte blue, int SpeedDelay) {
  for (uint16_t i = 0; i < NUM_LEDS; i++) {

    int packetSize = UDP.parsePacket();
    if (packetSize) {
      UDP.read((char *)&cmd, sizeof(struct led_command));
      lastReceived = millis();
    }

    if (cmd.opmode != 4) break;

    FastLED.clear();
    setPixel(i, red, green, blue);
    showStrip();
    delay(SpeedDelay);
  }
}

void PixelSwipeInvert(byte red, byte green, byte blue, int SpeedDelay) {
  for (uint16_t i = NUM_LEDS - 1; i > 0; i--) {

    int packetSize = UDP.parsePacket();
    if (packetSize) {
      UDP.read((char *)&cmd, sizeof(struct led_command));
      lastReceived = millis();
    }

    if (cmd.opmode != 4) break;

    FastLED.clear();
    setPixel(i, red, green, blue);
    showStrip();
    delay(SpeedDelay);
  }
}

void larsonScanner(int delayTime) {
  // knight rider LEDs
  int t;
  byte *c;
  for (int i = 6; i < NUM_LEDS; i++) {
    if (wifiBreak() == true) break;

    c = Wheel(wc);

    FastLED.clear();
    setPixel(i, *c, *(c + 1), *(c + 2));
    setPixel(i - 1, *c, *(c + 1), *(c + 2));
    setPixel(i - 2, *c, *(c + 1), *(c + 2));
    setPixel(i - 3, *c, *(c + 1), *(c + 2));
    setPixel(i - 4, *c, *(c + 1), *(c + 2));
    setPixel(i - 5, *c, *(c + 1), *(c + 2));

    showStrip();
    delay(delayTime);
    wc++;
  }

  for (int i = NUM_LEDS - 1; i >= 6; i--) {
    if (wifiBreak() == true) break;

    c = Wheel(wc);

    FastLED.clear();
    setPixel(i, *c, *(c + 1), *(c + 2));
    setPixel(i - 1, *c, *(c + 1), *(c + 2));
    setPixel(i - 2, *c, *(c + 1), *(c + 2));
    setPixel(i - 3, *c, *(c + 1), *(c + 2));
    setPixel(i - 4, *c, *(c + 1), *(c + 2));
    setPixel(i - 5, *c, *(c + 1), *(c + 2));

    showStrip();
    delay(delayTime);
    wc++;
  }

  wc++;
}

/*
  void Fire(int Cooling, int Sparking, int SpeedDelay) {
  static byte heat[NUM_LEDS];
  int cooldown;

  // Step 1.  Cool down every cell a little
  for ( int i = 0; i < NUM_LEDS; i++) {
    cooldown = random(0, ((Cooling * 10) / NUM_LEDS) + 2);

    if (cooldown > heat[i]) {
      heat[i] = 0;
    }
    else {
      heat[i] = heat[i] - cooldown;
    }
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for ( int k = NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' near the bottom
  if ( random(255) < Sparking ) {
    int y = random(7);

    heat[y] = heat[y] + random(160, 255);
    //heat[y] = random(160,255);
  }

  // Step 4.  Convert heat to LED colors
  for ( int j = 0; j < NUM_LEDS; j++) {
    setPixelHeatColor(j, heat[j] );
  }
  showStrip();

  delay(SpeedDelay);
  }

  void setPixelHeatColor (int Pixel, byte temperature) {
  // Scale 'heat' down from 0-255 to 0-191
  byte t192 = round((temperature / 255.0) * 191);

  // calculate ramp up from
  byte heatramp = t192 & 0x3F; // 0..63
  heatramp <<= 2; // scale up to 0..252

  // figure out which third of the spectrum we're in:
  if ( t192 > 0x80) {                    // hottest
    setPixel(Pixel, 255, 255, heatramp);
  }
  else if ( t192 > 0x40 ) {            // middle
    setPixel(Pixel, 255, heatramp, 0);
  }
  else {                               // coolest
    setPixel(Pixel, heatramp, 0, 0);
  }
  }
*/

void rainbowCycle(int SpeedDelay) {
  boolean skipNow = false;
  byte *c;
  uint16_t i, j;

  for (j = 0; j < 256 * 5; j++) { // 5 cycles of all colors on wheel
    for (i = 0; i < NUM_LEDS; i++) {
      if (wifiBreak() == true) {
        skipNow = true;
        break;
      }

      c = Wheel(((i * 256 / NUM_LEDS) + j) & 255);
      setPixel(i, *c, *(c + 1), *(c + 2));
    }
    if (skipNow == true) break;

    showStrip();
    delay(SpeedDelay);
  }
}

// used by rainbowCycle and theaterChaseRainbow
byte * Wheel(byte WheelPos) {
  static byte c[3];

  if (WheelPos < 85) {
    c[0] = WheelPos * 3;
    c[1] = 255 - WheelPos * 3;
    c[2] = 0;
  }
  else if (WheelPos < 170) {
    WheelPos -= 85;
    c[0] = 255 - WheelPos * 3;
    c[1] = 0;
    c[2] = WheelPos * 3;
  }
  else {
    WheelPos -= 170;
    c[0] = 0;
    c[1] = WheelPos * 3;
    c[2] = 255 - WheelPos * 3;
  }

  return c;
}

//Bias function to make the lower volumes more pronunced
float fscale(float originalMin, float originalMax, float newBegin, float newEnd, float inputValue, float curve)
{

  float OriginalRange = 0;
  float NewRange = 0;
  float zeroRefCurVal = 0;
  float normalizedCurVal = 0;
  float rangedValue = 0;
  boolean invFlag = 0;

  // condition curve parameter
  // limit range

  if (curve > 10)
    curve = 10;
  if (curve < -10)
    curve = -10;

  curve = (curve * -.1);  // - invert and scale - this seems more intuitive - postive numbers give more weight to high end on output
  curve = pow(10, curve); // convert linear scale into lograthimic exponent for other pow function

  // Check for out of range inputValues
  if (inputValue < originalMin)
  {
    inputValue = originalMin;
  }
  if (inputValue > originalMax)
  {
    inputValue = originalMax;
  }

  // Zero Refference the values
  OriginalRange = originalMax - originalMin;

  if (newEnd > newBegin)
  {
    NewRange = newEnd - newBegin;
  }
  else
  {
    NewRange = newBegin - newEnd;
    invFlag = 1;
  }

  zeroRefCurVal = inputValue - originalMin;
  normalizedCurVal = zeroRefCurVal / OriginalRange; // normalize to 0 - 1 float

  // Check for originalMin > originalMax  - the math for all other cases i.e. negative numbers seems to work out fine
  if (originalMin > originalMax)
  {
    return 0;
  }

  if (invFlag == 0)
  {
    rangedValue = (pow(normalizedCurVal, curve) * NewRange) + newBegin;
  }
  else // invert the ranges
  {
    rangedValue = newBegin - (pow(normalizedCurVal, curve) * NewRange);
  }

  return rangedValue;
}

void ColorSelect(int redcolor, int greencolor, int bluecolor) {
  setAll(redcolor, greencolor, bluecolor);
}

//Sound react
void soundReactive(int analogRaw) {

  int sanityValue = sanityBuffer->computeAverage();

  if (!(abs(analogRaw - sanityValue) > BUFFER_DEVIATION)) {
    sanityBuffer->setSample(analogRaw);
  }
  analogRaw = fscale(MIC_LOW, MIC_HIGH, MIC_LOW, MIC_HIGH, analogRaw, 0.5);

  if (samples->setSample(analogRaw))
    return;

  uint16_t longTermAverage = longTermSamples->computeAverage();
  uint16_t useVal = samples->computeAverage();
  longTermSamples->setSample(useVal);

  int diff = (useVal - longTermAverage);

  if (diff > 5) {
    if (globalHue < 235)
    {
      globalHue += hueIncrement;
    }
  }

  else if (diff < -5)
  {
    if (globalHue > 2)
    {
      globalHue -= hueIncrement;
    }
  }


  int curshow = fscale(MIC_LOW, MIC_HIGH, 0.0, (float)NUM_LEDS, (float)useVal, 0);
  //int curshow = map(useVal, MIC_LOW, MIC_HIGH, 0, NUM_LEDS)

  for (int i = 0; i < NUM_LEDS; i++) {
    if (i < curshow) {
      leds[i] = CHSV(globalHue + hueOffset + (i * 2), 255, 255);
    }

    else {
      leds[i] = CRGB(leds[i].r / fadeScale, leds[i].g / fadeScale, leds[i].b / fadeScale);
    }
  }
  delay(5);
  FastLED.show();
}
