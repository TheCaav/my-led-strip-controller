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
#include <ESP8266WebServer.h>
#include <Arduino.h>
#include <FastLED.h>
#include <PubSubClient.h>
#include <map>

#include <misc_functions.h>

#ifndef APSSID
#define STASSID "raspi-webgui";
#define STAPSK "PLACEHOLDER"
#endif

const char *ssid = STASSID;
const char *password = STAPSK;
const char* mqtt_server = "10.3.141.1";

#define LED_DT D2        // Serial data pin
#define LED_CK 11        // Clock pin for WS2801 or APA102
#define COLOR_ORDER GRB  // It's GRB for WS2812B and BGR for APA102
#define LED_TYPE WS2812B // What kind of strip are you using (APA102, WS2801 or WS2812B)?
String led_type = "WS2812B";
#define NUM_LEDS 30
#define MAX_BRIGHT 180

#define identifier 1

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];

struct CRGB leds[NUM_LEDS];

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
CHSV gHSVColor = CHSV(0, 0, 0);    // rotating "base color" used by many of the patterns
CRGB gRGBColor = CRGB(0);
bool changeHue = true;

float bpm_freq = 120;

void mqttconnect()
{
    /* Loop until reconnected */
    Serial.print("MQTT connecting ...");
    /* client ID */
    String clientId = "ESP32LED";
    /* connect now */
    if (client.connect(clientId.c_str()))
    {
        Serial.println("connected");
        client.subscribe("/family/CalvinsRoom/LED", 0);
    }
    else
    {
        Serial.print("failed, status code =");
        Serial.print(client.state());
    }
}

void solid()
{
    /**for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = gRGBColor;
    }*/
    FastLED.showColor(gRGBColor);
}

void solidRainbow()
{
    CRGB rgb;
    hsv2rgb_rainbow(CHSV(gHSVColor.hue, 255, 255), rgb);
    for (int i = 0; i < NUM_LEDS; i++)
    {
        leds[i] = rgb;
    }
}

void maybeChangeHue()
{
    if (changeHue)
    {
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
    uint8_t BeatsPerMinute = 120;
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
std::map<String, CRGB> colCorMap{{"UncorrectedColor", UncorrectedColor}, {"TypicalLEDStrip", TypicalLEDStrip}, {"MyWS2812b", CRGB(153, 238, 255)}};
std::map<String, CRGB> colTempMap{{"UncorrectedTemperature", UncorrectedTemperature}, {"ClearBlueSky", ClearBlueSky}, {"DirectSunlight", DirectSunlight}, {"Candle", Candle}};

void changeAnimation(String animationName)
{
    std::map<String, int>::iterator anim = animMap.find(animationName);
    if (anim == animMap.end())
    {
        return;
    }
    else
    {
        switch (anim->second)
        {
        case 0:
            changeHue = false;
            break;
        case 3:
            changeHue = true;
            break;
        }
        gCurrentPatternNumber = anim->second;
    }
}

void setBrightness(int brightness)
{
    if (brightness > 255)
    {
        brightness = 255;
        Serial.println("Brightness to high. Defaulted to 255");
    }
    else if (brightness < 0)
    {
        brightness = 0;
        Serial.println("Brightness to low. Defaulted to 0");
    }
    FastLED.setBrightness(brightness);
}

void setFrequency(float frequency)
{
    if (frequency <= 0)
    {
        Serial.printf("Frequency too low. Set to 1");
        frequency = 1;
    }
    else if (frequency > 9000)
    {
        Serial.printf("FREQUENCY IS OVER 9000!!!");
        frequency = 9000;
    }
    else
    {
        bpm_freq = frequency;
    }
}

void setColorCorrection(String correction)
{
    if (correction.startsWith("0x"))
    {
        FastLED.setCorrection(CRGB(hexToInt(correction)));
    }
    else
    {
        std::map<String, CRGB>::iterator corr = colCorMap.find(correction);
        if (corr == colCorMap.end())
        {
            return;
        }
        else
        {
            FastLED.setCorrection(corr->second);
        }
    }
}

void setColorTemperature(String temperature)
{
    if (temperature.startsWith("0x"))
    {
        FastLED.setTemperature(CRGB(hexToInt(temperature)));
    }
    else
    {
        std::map<String, CRGB>::iterator temp = colTempMap.find(temperature);
        if (temp == colTempMap.end())
        {
            return;
        }
        else
        {
            FastLED.setCorrection(temp->second);
        }
    }
}

void setColor(String color)
{
    gRGBColor = CRGB(hexToInt(color));
    gHSVColor = rgb2hsv_approximate(gRGBColor); // not really working I believe. Maybe research HSV more
}

String printUsage()
{
    String string = "";

    // Current Status:
    string += "Current Status of the led controller " + String(identifier) + ":\n";
    string += "Wifi SSID is: ";
    string += ssid;
    string += "\n";
    string += "Type of strip: " + led_type + "\n";
    string += "Number of leds: " + String(NUM_LEDS) + "\n";
    string += "============================================\n";
    string += "Current Brightness: " + String(FastLED.getBrightness());
    string += "\n";
    string += "Current Color (if applicable):\n";
    string += "red: " + String(gRGBColor.red);
    string += "green: " + String(gRGBColor.green);
    string += "blue: " + String(gRGBColor.blue);
    string += "\n";
    string += "Current Frequency: " + String(bpm_freq);
    string += "\n";
    string += "Rotating Hue? " + String(changeHue);
    string += "\n";
    string += "Current Hue: " + String(gHSVColor.hue);
    string += "\n";
    string += "Current Animation: \n";
    for (std::map<String, int>::iterator i = animMap.begin(); i != animMap.end(); i++)
    {
        if (i->second == gCurrentPatternNumber)
        {
            string += i->first;
        }
    }
    string += "\n";
    string += "============================================\n";
    string += "USAGE: \n";
    string += "To change animation \n";
    string += "animation=<animation name> \n";
    string += "Number of Animations: " + String(animMap.size());
    string += "\n";
    string += "List of possible Animations: \n";
    for (std::map<String, int>::iterator i = animMap.begin(); i != animMap.end(); i++)
    {
        string += i->first;
        string += "\n";
    }
    string += "Set Brightness: \n";
    string += "brightness=<0-255>\n";
    string += "Set Frequency: \n";
    string += "frequency=<0.*-9000>\n";
    string += "Set Color (Changes animation to solid and disables hue rotation)\n";
    string += "color=0x......\n";
    string += "Set Hue Rotation (if applicable):\n";
    string += "changeHue=<true | false>\n";
    string += "Toggle Hue Rotation (if applicable):\n";
    string += "toggleHueRotation=<doesnt matter>\n";
    string += "Set Color Correction: \n";
    string += "ColorCorrection=<0x...... | identifier>\n";
    string += "With Identifier being oBYip3sj5pbnYtswmM3r4ne of: \n";
    for (std::map<String, CRGB>::iterator i = colCorMap.begin(); i != colCorMap.end(); i++)
    {
        string += i->first;
        string += "\n";
    }
    string += "Set Color Temperature: \n";
    string += "ColorTemperature=<0x...... | identifier>\n";
    string += "With Identifier being one of: \n";
    for (std::map<String, CRGB>::iterator i = colTempMap.begin(); i != colTempMap.end(); i++)
    {
        string += i->first;
        string += "\n";
    }

    return string;
}

void setupLEDS()
{
    FastLED.addLeds<LED_TYPE, LED_DT, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setCorrection(CRGB(153, 228, 255));
    FastLED.setBrightness(MAX_BRIGHT);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
}

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
                /**gCurrentPatternNumber = animMap["solid"];
                changeHue = false;*/
                changeAnimation("solid");
            }
            else if (server.argName(i).equalsIgnoreCase("animation"))
            {
                changeAnimation(server.arg(i));
            }
            else if (server.argName(i).equalsIgnoreCase("changeHue"))
            {
                changeHue = server.arg(i).equalsIgnoreCase("true") ? true : false;
            }
            else if (server.argName(i).equalsIgnoreCase("toggleHueRotation"))
            {
                changeHue = changeHue ? false : true;
            }
            else if (server.argName(i).equalsIgnoreCase("ColorCorrection"))
            {
                setColorCorrection(server.arg(i));
            }
            else if (server.argName(i).equalsIgnoreCase("ColorTemperature"))
            {
                setColorTemperature(server.arg(i));
            }
            else if (server.argName(i).equalsIgnoreCase("Brightness"))
            {
                setBrightness(server.arg(i).toInt());
            }
            else if (server.argName(i).equalsIgnoreCase("Frequency"))
            {
                setFrequency(server.arg(i).toFloat());
            }
            else if (server.argName(i).equalsIgnoreCase("Usage"))
            {
                message += printUsage();
            }
        }
        server.send(200, "text/plain", message);
    }
    else
    {
        server.send(200, "text/html", "<h1>This is not a POST</h1>");
    }
}

void printMessage(byte *message, unsigned int length)
{
    for (unsigned int i = 0; i < length; i++)
    {
        Serial.print((char)message[i]);
    }
    Serial.println("");
}

void callback(char *topic, byte *message, unsigned int length)
{
    Serial.print("Message arrived on topic: ");
    Serial.print(topic);
    Serial.print(". Message: ");
    printMessage(message, length);
    String arg;
    String val;
    bool value = false;

    for (unsigned int i = 0; i < length; i++)
    {
        if ((char)message[i] == '=')
        {
            value = true;
        }
        else if (value)
        {
            val[i] += (char)message[i];
        }
        else
        {
            arg[i] += (char)message[i];
        }
    }

    if (arg.equalsIgnoreCase("color"))
    {
        setColor(val);
        /**gCurrentPatternNumber = animMap["solid"];
                changeHue = false;*/
        changeAnimation("solid");
    }
    else if (arg.equalsIgnoreCase("animation"))
    {
        changeAnimation(val);
    }
    else if (arg.equalsIgnoreCase("changeHue"))
    {
        changeHue = val.equalsIgnoreCase("true") ? true : false;
    }
    else if (arg.equalsIgnoreCase("toggleHueRotation"))
    {
        changeHue = changeHue ? false : true;
    }
    else if (arg.equalsIgnoreCase("ColorCorrection"))
    {
        setColorCorrection(val);
    }
    else if (arg.equalsIgnoreCase("ColorTemperature"))
    {
        setColorTemperature(val);
    }
    else if (arg.equalsIgnoreCase("Brightness"))
    {
        setBrightness(val.toInt());
    }
    else if (arg.equalsIgnoreCase("Frequency"))
    {
        setFrequency(val.toFloat());
    }
}

void setup()
{
    delay(3000);
    Serial.begin(9600);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    /* You can remove the password parameter if you want the AP to be open. */
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    WiFi.setAutoReconnect(true);

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    server.on("/", handleRoot);
    server.begin();
    Serial.println("HTTP server started");

    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    mqttconnect();

    setupLEDS();
}

void loop()
{
      /* if client was disconnected then try to reconnect again */
    if (!client.connected()) {
        mqttconnect();
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wifi disconnected. Reconnecting...");
        WiFi.reconnect();
    }
    client.loop();
    server.handleClient();

    gPatterns[gCurrentPatternNumber]();
    if (gCurrentPatternNumber != 0)
    { // hack to suppress flickering caused by setting colors shortly after each other
        FastLED.show();
    }
    maybeChangeHue();
    delay(60000 / bpm_freq);
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
