#include <SPI.h>
#include <Ethernet.h>
#include <WebSocketClient.h>
#include "Adafruit_WS2801.h"
#include <aJSON.h>
#include <avr/wdt.h>


byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0x9C, 0xEA };
byte ip[] = { 192, 168, 0, 108 };  

int dataPin  = 2;    // Yellow wire on Adafruit Pixels
int clockPin = 3;    // Green wire on Adafruit Pixels
Adafruit_WS2801 strip = Adafruit_WS2801(49 /* number of LEDs */, dataPin, clockPin);

long previousMillis = 0;
int oldj = 0;
int isAnimating = 0;
unsigned long interval = 50;
int brightness = 255;

char server[] = "server.com";
char path[] = "/";
int port = 8080;
WebSocketClient client;

String currentState = "";
int solidEvent = 1;
int queryEvent = 0;
int rainbowEvent = 2;
int colorWipeEvent = 3;
int rainbowCycleEvent = 6;

int colorIndex = 0;
int maxColors = 6;

void setup() {
  Serial.begin(9600);
  wdt_enable(WDTO_2S);
  strip.begin();
  strip.show();
}

void loop() {
  wdt_reset();
  if(!client.connected()) {
    setColor(0, 0, 0);
    strip.setPixelColor(0, Color(255, 0, 0));
    strip.show();
    connectToServer();
  }
  client.monitor();
  
  unsigned long currentMillis = millis();
    
  if(currentMillis - previousMillis > 300000) { 
    previousMillis = currentMillis; 
    client.send("ping");
  }
    
  if(isAnimating != 0) {
    
    if(currentMillis - previousMillis > interval) { 
      previousMillis = currentMillis; 
      if(isAnimating == 1) {
        //Rainbow
        oldj++;
        if(oldj >= (brightness+1)) oldj = 0;
        rainbow(oldj);
      } else if(isAnimating == 2) {
        //Color Wipe
        if(oldj >= strip.numPixels()) {
          oldj = 0;
          colorIndex++;
          if(colorIndex >= maxColors) {
            colorIndex = 0;
          }
        }
        strip.setPixelColor(oldj, animColors(colorIndex));
        strip.show();
        oldj++;
      } else if(isAnimating == 3) {
        //Rainbow Cycle
        oldj++;
        if(oldj >= 256*5) oldj = 0;
        rainbowCycle(oldj);
      }
    }
  }
}

void connectToServer() {
  Serial.println("Connecting...");
  
  Ethernet.begin(mac, ip);
  if(client.connect(server,path,port)) {
    Serial.println("Connected"); 
    setColor(0, 0, 0);
    client.setDataArrivedDelegate(dataArrived);
    //dataArrived(client, currentState);
  } else {
    Serial.println("Connection Failed");
    strip.setPixelColor(0, Color(255, 0, 0));
    strip.show();
  }
}

void dataArrived(WebSocketClient client, String data) {
  if(data.startsWith("currentState")) {
    return;
  }
  //Serial.println("Data Arrived: " + data);
  char *c = data.buffer;
  if(data.length() == 0) {
    setColor(0, 0, 0);
  }
  aJsonObject *root = aJson.parse(c);
  aJsonObject *event = aJson.getObjectItem(root, "event");
  //Serial.println(event->valuestring);
  oldj = 0;
  if(event->valueint == solidEvent) {
    Serial.println("Received Solid Event");
    isAnimating = 0;
    currentState = data;
    aJsonObject *colors = aJson.getObjectItem(root, "color");
    int r = aJson.getArrayItem(colors, 0)->valueint;
    int g = aJson.getArrayItem(colors, 1)->valueint;
    int b = aJson.getArrayItem(colors, 2)->valueint;
    setColor(r, g, b);
  } else if(event->valueint == queryEvent) {
    Serial.println("Received Query Event");
    client.send("currentState: " + currentState);
  } else if(event->valueint == rainbowEvent) {
    currentState = data;
    interval = aJson.getObjectItem(root, "speed")->valueint;
    brightness = aJson.getObjectItem(root, "brightness")->valueint;
    isAnimating = 1;
  } else if(event->valueint == colorWipeEvent) {
    currentState = data;
    interval = aJson.getObjectItem(root, "speed")->valueint;
    brightness = aJson.getObjectItem(root, "brightness")->valueint;
    isAnimating = 2;
    setColor(0,0,0);
  } else if(event->valueint == rainbowCycleEvent) {
    currentState = data;
    interval = aJson.getObjectItem(root, "speed")->valueint;
    brightness = aJson.getObjectItem(root, "brightness")->valueint;
    isAnimating = 3;
  }
  aJson.deleteItem(root);
}

void setColor(int r, int g, int b) {
  for (int i=0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, Color(r, g, b));
  }
  strip.show();
}

uint32_t animColors(int i) {
  //animColors[] = {Color(brightness,0,0), Color(brightness,brightness,0), Color(0,brightness,0), 
  //Color(0,brightness,brightness), Color(0,0,brightness), Color(brightness,0,brightness)};
  uint32_t retVal;
  switch(i) {
     case 0:
       retVal = Color(brightness,0,0);
       break;
     case 1:
       retVal = Color(brightness,brightness,0);
       break;
     case 2:
       retVal = Color(0,brightness,0);
       break;
     case 3:
       retVal = Color(0,brightness,brightness);
       break;
     case 4:
       retVal = Color(0,0,brightness);
       break;
     case 5:
       retVal = Color(brightness,0,brightness);
       break;
     default:
       retVal = Color(brightness,0,0);
       break;
  }
  return retVal;
}

void rainbow(int j) {
   int i;
    for (i=0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel( (j) % brightness));
    }  
    strip.show();   // write all the pixels out
}

void rainbowCycle(int j) {
  int i;
  
  //for (j=0; j < 256 * 5; j++) {     // 5 cycles of all 25 colors in the wheel
    for (i=0; i < strip.numPixels(); i++) {
      // tricky math! we use each pixel as a fraction of the full 96-color wheel
      // (thats the i / strip.numPixels() part)
      // Then add in j which makes the colors go around per pixel
      // the % 96 is to make the wheel cycle around
      strip.setPixelColor(i, Wheel( ((i * (brightness+1) / strip.numPixels()) + j) % (brightness+1)) );
    }  
    strip.show();   // write all the pixels out
    //delay(wait);
  //}
}

uint32_t Color(byte r, byte g, byte b)
{
  uint32_t c;
  c = r;
  c <<= 8;
  c |= g;
  c <<= 8;
  c |= b;
  return c;
}

uint32_t Wheel(byte WheelPos)
{
  if (WheelPos < (brightness/3)) {
   return Color(WheelPos * 3, brightness - WheelPos * 3, 0);
  } else if (WheelPos < (brightness/1.5)) {
   WheelPos -= (brightness/3);
   return Color(brightness - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= (brightness/1.5); 
   return Color(0, WheelPos * 3, brightness - WheelPos * 3);
  }
}

String splitString(String s, char parser,int index){
  String rs='\0';
  int parserIndex = index;
  int parserCnt=0;
  int rFromIndex=0, rToIndex=-1;

  while(index>=parserCnt){
    rFromIndex = rToIndex+1;
    rToIndex = s.indexOf(parser,rFromIndex);

    if(index == parserCnt){
      if(rToIndex == 0 || rToIndex == -1){
        return '\0';
      }
      return s.substring(rFromIndex,rToIndex);
    }
    else{
      parserCnt++;
    }

  }
  return rs;
}
