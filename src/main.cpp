#include <Arduino.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <OneButton.h>

#define SER_Pin 4
#define RCLK_Pin 6
#define SRCLK_Pin 5
#define BUZZ_Pin 9
#define SW_Pin 11
#define SWLED_Pin 10
#define IR1_Pin A3
#define IR2_Pin A2

#define MINLAPTime 2000
#define SLEEPTimeout 15000

enum states_t {SYNC, WAIT, WARMUP, RUN, SETUP, SLEEP};
states_t STATE;

LiquidCrystal_I2C displays[] = {
  LiquidCrystal_I2C(0x27, 2,1,0,4,5,6,7,3,POSITIVE),
  LiquidCrystal_I2C(0x26, 2,1,0,4,5,6,7,3,POSITIVE)
};
OneButton startButton(SW_Pin, true);

uint32_t t0 = 0;
uint32_t lapCount[2];
uint32_t lapStart[2];
uint32_t bestLap[2];
uint8_t irPin[] = {IR1_Pin, IR2_Pin};
uint32_t lastIrValue[2];
uint32_t lastHighIrTime[] = {0, 0};
uint32_t refreshRate = 100;
bool forceRefresh = false;
uint32_t lastClickTime = millis();

void start_click();
void start_longpress();
 void printTime(LiquidCrystal_I2C* display, uint32_t time) {
   int t = time / 100;
   display->print(t/10);
   display->print(".");
   display->print(t % 10);
 }

void sync() {
  digitalWrite(SWLED_Pin, LOW);
  for(int i=0; i<2; i++) {
    displays[i].clear();
    displays[i].setCursor(0,0);
    displays[i].print("Syncing");
    displays[i].setCursor(0,1);
    displays[i].print("Sig:");
  }
  STATE = SYNC;
}

void warmup() {
  t0 = millis() + 4999;
  for(int i=0; i<2; i++) {
    displays[i].clear();
    displays[i].setCursor(0,0);
    displays[i].print("Get Ready...");
  }
  STATE = WARMUP;
}

void run() {
  digitalWrite(SWLED_Pin, LOW);
  refreshRate = 100;
  tone(BUZZ_Pin, 2000);
  digitalWrite(SRCLK_Pin, LOW);
  shiftOut(SER_Pin, RCLK_Pin, MSBFIRST, 0);
  digitalWrite(SRCLK_Pin, HIGH);
  for(int i=0; i<2 ; i++){      
    displays[i].clear();
    displays[i].setCursor(0,0);displays[i].print("LAP:"); //LAP:## Best:##.#
    displays[i].setCursor(7,0);displays[i].print("Best:");
    displays[i].setCursor(0,1);displays[i].print("Time:"); //Time:##.# Pos:#
    displays[i].setCursor(11,1);displays[i].print("Pos:"); 

    lapCount[i] = 0;
    lapStart[i] = t0;
    bestLap[i] = UINT32_MAX;
    lastIrValue[i] = 0;    
  }
  STATE = RUN;
}

void wait() {
  digitalWrite(SWLED_Pin, HIGH);
  refreshRate = 1000;
  STATE = WAIT;
}

void config() {
  digitalWrite(SWLED_Pin, HIGH);
  refreshRate = 200;
  STATE = SETUP;
}

void sleep() {
  refreshRate = 60000;
  for(int i=0; i<2; i++) {
    displays[i].noBacklight();
    displays[i].clear();
    displays[i].print("Zzzzzzzzzzzz");
  }
  digitalWrite(SWLED_Pin, LOW);
  STATE = SLEEP;
}

void wakeup() {
  displays[0].backlight();
  displays[1].backlight();
}

void start_click() {
  lastClickTime = millis();
  switch(STATE) {
    case SYNC:
      break;
    case WAIT:
      warmup();
      break;
    case WARMUP:
      warmup();
      break;
    case SETUP:
      break;
    case RUN:
      warmup();
      break;
    case SLEEP:
      wakeup();
      wait();
      break;
  }
}

void start_longpress() {
  lastClickTime = millis();
  switch(STATE) {
    case SYNC:
      break;
    case WAIT:
      config();
      break;
    case WARMUP:
      config();
      break;
    case RUN:
      config();
      break;
    case SETUP:
      break;
    case SLEEP:
      wakeup();
      config();
      break;
  }
}

void setup() {  
  pinMode(SER_Pin, OUTPUT);
  pinMode(RCLK_Pin, OUTPUT);
  pinMode(SRCLK_Pin, OUTPUT);
  pinMode(BUZZ_Pin, OUTPUT);
  pinMode(SWLED_Pin, OUTPUT);
  pinMode(IR1_Pin, INPUT);
  pinMode(IR2_Pin, INPUT);
  digitalWrite(SWLED_Pin, HIGH);

  Serial.begin(57600);

  displays[0].begin(16, 2);
  displays[1].begin(16, 2);

  digitalWrite(SRCLK_Pin, LOW);
  shiftOut(SER_Pin, RCLK_Pin, MSBFIRST, 0);
  digitalWrite(SRCLK_Pin, HIGH);

  startButton.attachClick(start_click); 
  startButton.attachLongPressStart(start_longpress);
  sync();
 }

void loop() {
  static uint32_t syncLastIrOk[] = {millis(), millis()};
  static int syncIrSigStrength[] =  {0, 0};
  static int warmupPhase = 0;

  startButton.tick();

  if(lastClickTime + SLEEPTimeout < millis() && lapStart[0] + SLEEPTimeout < millis() && lapStart[1] + SLEEPTimeout < millis())
    sleep();

  switch(STATE)
  {
    case SYNC:
      for(int i=0; i<2; i++) {
        syncIrSigStrength[i] = analogRead(irPin[i]);
        if(syncIrSigStrength[i] < 900) syncLastIrOk[i] = millis();
      }
      if(syncLastIrOk[0] + 2000 < millis() && syncLastIrOk[1] + 2000 < millis())
        wait();
      break;
    case WAIT:
      break;
    case WARMUP:
      refreshRate = 200;
        warmupPhase = (t0 - millis())/500;
        if(warmupPhase % 2 == 1) {
          tone(BUZZ_Pin, 100);
        }
        else {
          noTone(BUZZ_Pin);
        }
        digitalWrite(SRCLK_Pin, LOW);
        shiftOut(SER_Pin, RCLK_Pin, MSBFIRST, 63-pow(2, 5-warmupPhase/2)+1);
        digitalWrite(SRCLK_Pin, HIGH);

        if(millis() > t0) {
          run();
        }
      break;
    case RUN:
      if(millis() > t0+1000) {
        noTone(BUZZ_Pin);
      }
      /*Lap Detection*/
      for(int i=0 ; i<2 ; i++)
      {
          int ir = analogRead(irPin[i]);
          if(ir > 800)
            lastHighIrTime[i] = millis();
          if(millis() > lapStart[i]+MINLAPTime &&  ir < 800 && lastIrValue[i] > 800) {
            /* Car detected */
            unsigned long t = millis();
            lapCount[i]++;
            if(bestLap[i] != 0 && t-lapStart[i] < bestLap[i])
              bestLap[i] = t-lapStart[i];
            lapStart[i] = t;
          }
          lastIrValue[i] = ir;
          if(millis() > t0+5000 && lastHighIrTime[i] + 5000 < millis())
            sync();
      }
      break;
    case SETUP:
      break;
    case SLEEP:
      break;
  }

  /*Display*/
  static unsigned long lastPrint = millis();
  if(forceRefresh || millis() > (lastPrint + refreshRate)) {    
    lastPrint = millis();
    switch(STATE) {
      case SYNC:
        for(int i=0; i<2; i++) {
          displays[i].setCursor(5,1);
          displays[i].print(syncIrSigStrength[i]);
          displays[i].print("   ");
        }
        break;
      case WAIT:
        for(int i=0 ; i<2; i++) {
          displays[i].clear();
          displays[i].print("Press start");
        }
        break;
      case WARMUP:
        for(int i=0 ; i<2; i++)
        {
          displays[i].setCursor(0,1);
          int phase = (t0-millis())/1000;
          for(int j=0; j <= phase; j++)
            displays[i].print("#");
        }
        break;
      case RUN:
        /*Calculate pole position*/
        int polePosition;
        if(lapCount[0] > lapCount[1])
          polePosition = 0;
        else if(lapCount[0] < lapCount[1])
          polePosition = 1;
        else
          polePosition = lapStart[0] < lapStart[1]?0:1;

        for(int i = 0 ; i<2 ; i++) {
          unsigned long lap = millis() - lapStart[i];
          displays[i].setCursor(4, 0);
          displays[i].print(lapCount[i]);
          displays[i].setCursor(12, 0); 
          printTime(&displays[i], bestLap[i]);
          displays[i].setCursor(5, 1); 
          printTime(&displays[i], lap);
          displays[i].setCursor(15, 1); 
          displays[i].print(i==polePosition?1:2);      
        }
        break;
      case SETUP:
        displays[1].clear();
        displays[0].clear();
        displays[0].print("Setup");
        break;
      case SLEEP:
        break;
    }
    forceRefresh = false;
  }
}