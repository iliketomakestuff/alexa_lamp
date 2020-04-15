/*
This code is A PROTOTYPE put together by Bob Clagett of I Like To Make Stuff
It's not efficient, not guaranteed to work, etc
I also cannot help you get it to run, debug your issues etc.  

Feel free to use it, modify it, but you're on you're own.

See the accompanying project at
https://iliketomakestuff.com/making-a-modern-lamp-with-inductive-charging/

*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

#ifdef ESP32
    #include <WiFi.h>
#else
    #include <ESP8266WiFi.h>
#endif
#include "fauxmoESP.h"

// Rename the credentials.sample.h file to credentials.h and 
// edit it according to your router configuration
#include "credentials.h"

fauxmoESP fauxmo;

#define SERIAL_BAUDRATE     115200

#define LED_PIN     D1
#define DEVICE_ID   "Charging Lamp"
#define SWITCH_PIN  D4
boolean switchState = 0;
boolean lastSwitchState = 0;
int pendingSoftwareState = 0;
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(8, 8, LED_PIN,
      NEO_MATRIX_TOP     + NEO_MATRIX_RIGHT +
      NEO_MATRIX_COLUMNS + NEO_MATRIX_PROGRESSIVE,
      NEO_GRB            + NEO_KHZ800);

int x    = matrix.width();
int pass = 0;
int fadeDelay = 40;

struct RGB {
  byte r;
  byte g;
  byte b;
};

RGB white = { 255, 255, 255 };
RGB red = { 255, 0, 0 };
RGB blue = { 0, 0, 255 };
RGB off = { 0, 0, 0 };
int brightness = 35; //(assuming 0-100)

void wifiSetup() {

    // Set WIFI module to STA mode
    WiFi.mode(WIFI_STA);

    // Connect
    Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Wait
    matrix.setBrightness(1);
    bool loadSwap = 0;
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(100);
        if(loadSwap){
          loadSwap=0;
          matrix.drawPixel(3, 3, matrix.Color(0, 0, 255)); 
          matrix.drawPixel(3, 4, matrix.Color(255, 255, 255)); 
          matrix.drawPixel(4, 3, matrix.Color(255, 255, 255)); 
          matrix.drawPixel(4, 4, matrix.Color(0, 0, 255));
        }else{
          loadSwap=1;
          matrix.drawPixel(3, 3, matrix.Color(255, 255, 255)); 
          matrix.drawPixel(3, 4, matrix.Color(0, 0, 255)); 
          matrix.drawPixel(4, 3, matrix.Color(0, 0, 255)); 
          matrix.drawPixel(4, 4, matrix.Color(255, 255, 255));
        }
        matrix.show(); 
        delay(fadeDelay);
    }
    matrix.fillScreen(0);
    matrix.show();
    Serial.println();
    
    matrix.setBrightness(brightness);
    // Connected!
    Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  
}

void setup() {

    // Init serial port and clean garbage
    Serial.begin(SERIAL_BAUDRATE);
    Serial.println();
    Serial.println();

    // LEDs
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    lastSwitchState = digitalRead(SWITCH_PIN);
  matrix.begin();
  matrix.setTextWrap(false);

    wifiSetup();

    // By default, fauxmoESP creates it's own webserver on the defined port
    // The TCP port must be 80 for gen3 devices (default is 1901)
    // This has to be done before the call to enable()
    fauxmo.createServer(true); // not needed, this is the default value
    fauxmo.setPort(80); // This is required for gen3 devices

    // You have to call enable(true) once you have a WiFi connection
    // You can enable or disable the library at any moment
    // Disabling it will prevent the devices from being discovered and switched
    fauxmo.enable(true);

    // You can use different ways to invoke alexa to modify the devices state:
    // "Alexa, turn yellow lamp on"
    // "Alexa, turn on yellow lamp
    // "Alexa, set yellow lamp to fifty" (50 means 50% of brightness, note, this example does not use this functionality)

    // Add virtual devices
    fauxmo.addDevice(DEVICE_ID);

    fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
        
        // Callback when a command from Alexa is received. 
        // You can use device_id or device_name to choose the element to perform an action onto (relay, LED,...)
        // State is a boolean (ON/OFF) and value a number from 0 to 255 (if you say "set kitchen light to 50%" you will receive a 128 here).
        // Just remember not to delay too much here, this is a callback, exit as soon as possible.
        // If you have to do something more involved here set a flag and process it in your main loop.
        
        Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);

        // Checking for device_id is simpler if you are certain about the order they are loaded and it does not change.
        // Otherwise comparing the device_name is safer.

        if (strcmp(device_name, DEVICE_ID)==0) {
            if(state != switchState){
              switchState = state;
              changeLight(switchState);  
            }
            
            int beginningBRT = brightness;
            brightness = map(value,0,255,0,100);
            Serial.println(pendingSoftwareState);
            Serial.println(brightness);
            Serial.println(switchState);
            if(beginningBRT>brightness){
              while(beginningBRT>brightness){
                beginningBRT--;
                matrix.setBrightness(beginningBRT);
                matrix.show();
                delay(fadeDelay);
              }
            } else if(beginningBRT<brightness){
                while(beginningBRT<brightness){
                  beginningBRT++;
                  matrix.setBrightness(beginningBRT);
                  matrix.show();
                  delay(fadeDelay);
                }

            }
            
            
        }

    });

}

void loop() {
        
    // fauxmoESP uses an async TCP server but a sync UDP server
    // Therefore, we have to manually poll for UDP packets
    fauxmo.handle();

    // This is a sample code to output free heap every 5 seconds
    // This is a cheap way to detect memory leaks
    static unsigned long last = millis();
    if (millis() - last > 5000) {
        last = millis();
        Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
    }
    int switchRead = digitalRead(SWITCH_PIN);
    
    //now check if physical switch has changed
    if(switchRead!= lastSwitchState){
      lastSwitchState = switchRead;
      switchState = switchRead;
      //send new state back to Alexa (so app can update)
      changeLight(switchRead);
      fauxmo.setState(DEVICE_ID, switchRead, map(brightness,0,100,0,255));
    }
}

void changeLight(boolean newState) {
    Serial.println("changeLight");
    Serial.println(newState);
    switch(newState){
      case HIGH:
        crossFade(off,white,10,fadeDelay);
        break;
      case LOW:
        crossFade(white,off,10,fadeDelay);
        break;
    }
    delay(500);
}

void crossFade(RGB startColor, RGB endColor, int steps, int wt) {
  for(int i = 0; i <= steps; i++)
  {
     int newR = startColor.r + (endColor.r - startColor.r) * i / steps;
     int newG = startColor.g + (endColor.g - startColor.g) * i / steps;
     int newB = startColor.b + (endColor.b - startColor.b) * i / steps;

     matrix.fillScreen(matrix.Color(newR, newG, newB));
     matrix.show();
     delay(wt);
  }
}
