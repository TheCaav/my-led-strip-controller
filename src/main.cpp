/*
   Copyright (c) 2015, Majenko Technologies
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

 * * Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.

 * * Redistributions in binary form must reproduce the above copyright notice, this
     list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.

 * * Neither the name of Majenko Technologies nor the names of its
     contributors may be used to endorse or promote products derived from
     this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
   ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* Create a WiFi access point and provide a web server on it. */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Arduino.h>
#include <FastLED.h>
#include <map>

#include <misc_functions.h>

#ifndef APSSID
#define APSSID "ESPap"
#define APPSK "thereisnospoon"
#endif

#define LED_DT D2        // Serial data pin
#define LED_CK 11        // Clock pin for WS2801 or APA102
#define COLOR_ORDER GRB  // It's GRB for WS2812B and BGR for APA102
#define LED_TYPE WS2812B // What kind of strip are you using (APA102, WS2801 or WS2812B)?
#define NUM_LEDS 30
#define MAX_BRIGHT 255
#define FREQUENCY 30

struct CRGB leds[NUM_LEDS];

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
CHSV gHSVColor = CHSV(0,0,0);                  // rotating "base color" used by many of the patterns
CRGB gRGBColor = CRGB(0);
bool changeHue = true;

void solid() {
    /**for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = gRGBColor;
    }*/
    FastLED.showColor(gRGBColor);
}

void solidRainbow() {
    CRGB rgb;
    hsv2rgb_rainbow(CHSV(gHSVColor.hue, 255, 255), rgb);
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = rgb;
    }
}

void maybeChangeHue() {
    if (changeHue) {
        gHSVColor.hue++;
    }
}

// All the Animations
void rainbow()
{
    // FastLED's built-in rainbow generator
    fill_rainbow(leds, NUM_LEDS, gHSVColor.hue, 7);
}

void addGlitter(fract8 chanceOfGlitter)
{
    if (random8() < chanceOfGlitter)
    {
        leds[random16(NUM_LEDS)] += CRGB::White;
    }
}

void rainbowWithGlitter()
{
    // built-in FastLED rainbow, plus some random sparkly glitter
    rainbow();
    addGlitter(80);
}

void confetti()
{
    // random colored speckles that blink in and fade smoothly
    fadeToBlackBy(leds, NUM_LEDS, 10);
    int pos = random16(NUM_LEDS);
    leds[pos] += CHSV(gHSVColor.hue + random8(64), 200, 255);
}

void sinelon()
{
    // a colored dot sweeping back and forth, with fading trails
    fadeToBlackBy(leds, NUM_LEDS, 20);
    int pos = beatsin16(13, 0, NUM_LEDS - 1);
    leds[pos] += CHSV(gHSVColor.hue, 255, 192);
}

void bpm()
{
    // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
    uint8_t BeatsPerMinute = 62;
    CRGBPalette16 palette = PartyColors_p;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    for (int i = 0; i < NUM_LEDS; i++)
    { //9948
        leds[i] = ColorFromPalette(palette, gHSVColor.hue + (i * 2), beat - gHSVColor.hue + (i * 10));
    }
}

void juggle()
{
    // eight colored dots, weaving in and out of sync with each other
    fadeToBlackBy(leds, NUM_LEDS, 20);
    byte dothue = 0;
    for (int i = 0; i < 8; i++)
    {
        leds[beatsin16(i + 7, 0, NUM_LEDS - 1)] |= CHSV(dothue, 200, 255);
        dothue += 32;
    }
}

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = {solid, rainbow, rainbowWithGlitter, solidRainbow, confetti, sinelon, juggle, bpm};
std::map<String, int> animMap{{"solid", 0}, {"rainbow", 1}, {"rainbowWithGlitter", 2}, {"solidRainbow", 3}, {"confetti", 4}, {"sinelon", 5}, {"juggle", 6}, {"bpm", 7}};

int getAnimation(String animationName) {
    std::map<String, int>::iterator anim = animMap.find(animationName);
    if (anim == animMap.end()) {
        return -1;
    } else {
        return anim->second;
    }
}


void setColor(String color)
{
    gRGBColor = CRGB(hexToInt(color));
    gHSVColor = rgb2hsv_approximate(gRGBColor); // not really working I believe. Maybe research HSV more
}

void setupLEDS()
{
    FastLED.addLeds<LED_TYPE, LED_DT, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setTemperature(ClearBlueSky);

    FastLED.setBrightness(MAX_BRIGHT);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
}

/* Set these to your desired credentials. */
const char *ssid = APSSID;
const char *password = APPSK;

ESP8266WebServer server(80);

/* Just a little test message.  Go to http://192.168.4.1 in a web browser
   connected to this access point to see it.
*/
void handleRoot()
{
    if (server.method() == HTTP_POST)
    {
        /**for (int i = 0; i < server.args(); i++) {
        const String name = server.argName(i);
        const String arg = server.arg(i);
        Serial.print(name + ": ");
        Serial.println(arg);
        if (name.equalsIgnoreCase("color")) {
            setColor(arg);
        }
    }*/
        delay(100);
        String message = "POST form was:\n";
        for (uint8_t i = 0; i < server.args(); i++)
        {
            message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
            if (server.argName(i).equalsIgnoreCase("color"))
            {
                setColor(server.arg(i));
                gCurrentPatternNumber = animMap["solid"];
                changeHue = false;
            } else if (server.argName(i).equalsIgnoreCase("animation")) {
                gCurrentPatternNumber = getAnimation(server.arg(i));
            } else if (server.argName(i).equalsIgnoreCase("changeHue")) {
                changeHue = server.arg(i).equalsIgnoreCase("true") ? true : false; 
            } else if (server.argName(i).equalsIgnoreCase("toggleHueRotation")) {
                changeHue = changeHue ? false : true;
            }
        }
        server.send(200, "text/plain", message);
    }
    else
    {
        server.send(200, "text/html", "<h1>This is not a POST</h1>");
    }
}

void setup()
{
    delay(3000);
    Serial.begin(9600);
    Serial.println();
    Serial.print("Configuring access point...");
    /* You can remove the password parameter if you want the AP to be open. */
    WiFi.softAP(ssid, password);

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    server.on("/", handleRoot);
    server.begin();
    Serial.println("HTTP server started");

    setupLEDS();
}

void loop()
{
    server.handleClient();
    EVERY_N_MILLIS(1000 / FREQUENCY)
    {
        gPatterns[gCurrentPatternNumber]();
        if (gCurrentPatternNumber != 0) { // hack to suppress flickering caused by setting colors shortly after each other
            FastLED.show();
        }
        maybeChangeHue();
    }
    /** Check color correction
    Serial.println("Corrected");
    FastLED.setCorrection(TypicalLEDStrip);
    delay(200);
    FastLED.showColor(CRGB::White);
    delay(5000);
    Serial.println("Uncorrected");
    FastLED.setCorrection(UncorrectedColor);
    delay(200);
    FastLED.showColor(CRGB::White);
    delay(5000); */
}
