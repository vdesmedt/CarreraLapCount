#include <Arduino.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <OneButton.h>
#include <EEPROM.h>

#define SER_Pin 4
#define RCLK_Pin 6
#define SRCLK_Pin 5
#define BUZZ_Pin 9
#define SW_Pin 11
#define SWLED_Pin 10
#define IR1_Pin A3
#define IR2_Pin A2
#define BEACON_Pin 7 

#define MINLAPTime 2000
#define SLEEPTimeout 300000

enum states_t {SYNC, WAIT, WARMUP, RUN, SETUP, SLEEP, RESULT};
states_t STATE;

LiquidCrystal_I2C displays[] = {
  LiquidCrystal_I2C(0x27, 2,1,0,4,5,6,7,3,POSITIVE),
  LiquidCrystal_I2C(0x26, 2,1,0,4,5,6,7,3,POSITIVE)
};
OneButton startButton(SW_Pin, true);

uint32_t t0 = 0;
uint32_t te = 0;
uint8_t winner = -1;
uint32_t lapCount[2];
uint32_t lapStart[2];
uint32_t bestLap[2];
uint8_t irPin[] = {IR1_Pin, IR2_Pin};
uint32_t lastIrValue[2];
uint32_t lastHighIrTime[] = {0, 0};
uint32_t refreshRate = 100;
bool forceRefresh = false;
uint32_t lastClickTime = millis();
uint8_t raceLapCount = 0;

void start_click();
void start_longpress();
void printTime(LiquidCrystal_I2C* display, uint32_t time) {
  static char buffer[6];
  int t = time / 100;
  sprintf(buffer, "%02d\.%d", t/10, t%10);
  display->print(buffer);
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

void result() {
  for(int i=0; i<2; i++) {
    displays[i].clear();
    if(i == winner) {
      displays[i].print("Winner !!!");
    }
    else {
      displays[i].print("Next time... ?!");
    }
  }
  STATE = RESULT;
}

void wait() {
  digitalWrite(SWLED_Pin, HIGH);
  refreshRate = 1000;
  STATE = WAIT;
}

void config() {
  digitalWrite(SWLED_Pin, HIGH);
  refreshRate = 200;
  displays[1].clear();
  displays[0].clear();
  displays[1].print("Race Lap :");
  STATE = SETUP;
}

void sleep() {
  refreshRate = 5000;
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
      raceLapCount++;
      if(raceLapCount > 20)
        raceLapCount = 0;
      break;
    case RUN:
      warmup();
      break;
    case RESULT:
      displays[0].backlight();
      displays[1].backlight();
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
    case RESULT:
      config();
      break;
    case SETUP:
      EEPROM.update(0, raceLapCount);
      wait();
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
  pinMode(BEACON_Pin, OUTPUT);
  digitalWrite(SWLED_Pin, HIGH);
  raceLapCount = EEPROM.read(0);

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
  static bool newBestLap[] {false, false};
  static bool newPosition[] = {true, true};
  static bool newLap[] = {true, true};
  static uint8_t positions[] {1,2};

  digitalWrite(BEACON_Pin, HIGH);
  startButton.tick();

  if(lastClickTime + SLEEPTimeout < millis() && lapStart[0] + SLEEPTimeout < millis() && lapStart[1] + SLEEPTimeout < millis())
    sleep();

  switch(STATE)
  {
    case SYNC:
      for(int i=0; i<2; i++) {
        syncIrSigStrength[i] = analogRead(irPin[i]);
        if(syncIrSigStrength[i] < 800) syncLastIrOk[i] = millis();
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
            newLap[i] = true;
            if(bestLap[i] != 0 && t-lapStart[i] < bestLap[i]) {
              bestLap[i] = t-lapStart[i];
              newBestLap[i] = true;
            }

            if(lapCount[i] > lapCount[1-i]) {
              if(positions[i] !=1) {
                positions[i]=1;
                positions[1-i] = 2;
                newPosition[0] = newPosition[1] = true;  
              }
            }

            lapStart[i] = t;
          }
          lastIrValue[i] = ir;
          if(millis() > t0+5000 && lastHighIrTime[i] + 5000 < millis())
            sync();
          if(raceLapCount > 0 && lapCount[i] >= raceLapCount) {
            te = millis();
            winner = i;
            result();
          }
      }
      break;
    case RESULT:
      break;
    case SETUP:
      break;
    case SLEEP:
      break;
  }

  /*Display*/
  digitalWrite(BEACON_Pin, LOW);
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
        break;
      case RUN:
        for(int i = 0 ; i<2 ; i++) {
          unsigned long lap = millis() - lapStart[i];
          if(newLap[i]) {
            displays[i].setCursor(4, 0);
            displays[i].print(raceLapCount==0?lapCount[i]:raceLapCount-lapCount[i]);
            newLap[i] = false;
          }
          if(newBestLap[i]) {
            displays[i].setCursor(12, 0); 
            printTime(&displays[i], bestLap[i]);
            newBestLap[i] = false;
          }
          displays[i].setCursor(5, 1); 
          printTime(&displays[i], lap);
          if(newPosition[i]) {
            displays[i].setCursor(15, 1); 
            displays[i].print(positions[i]);      
            newPosition[i] = false;
          }
        }
        break;
      case RESULT:
        if((millis() - te) % 1000 < 500)
          displays[winner].backlight();
        else
          displays[winner].noBacklight();
        break;
      case SETUP:
        displays[1].setCursor(10,0);
        displays[1].print("   ");
        displays[1].setCursor(10,0);
        displays[1].print(raceLapCount);
        break;
      case SLEEP:
        digitalWrite(SWLED_Pin, HIGH);
        delay(100);
        digitalWrite(SWLED_Pin, LOW);
        break;
    }
    forceRefresh = false;
  }
}