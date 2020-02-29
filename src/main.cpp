#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <OneButton.h>
#include <EEPROM.h>

#define SR_DATAPin 4
#define SR_CLOCKPin 6
#define SR_STORECLK_Pin 5
#define BUZZ_Pin 9
#define SW_Pin 11
#define SWLED_Pin 10
#define IR1_Pin A3
#define IR2_Pin A2
#define BEACON_Pin 7

#define MINLAPTime 2000
#define SLEEPTimeout 300000

#define MINIRSig 600
#define MINIRSigForSync 800

enum states_t
{
  SYNC,
  WAIT,
  WARMUP,
  RUN,
  SETUP,
  SLEEP,
  RESULT,
  FALSE_START
};
states_t STATE;

LiquidCrystal_PCF8574 displays[] = {
    LiquidCrystal_PCF8574(0x27),
    LiquidCrystal_PCF8574(0x26)};
OneButton startButton(SW_Pin, true);

uint32_t t0 = 0;
uint32_t te = 0;
uint8_t winner = -1;
uint32_t lapCount[2];
uint32_t lapStartTime[2];
uint32_t lastLapTime[2] = {0, 0};
uint32_t bestLap[2];
uint8_t irPin[2] = {IR1_Pin, IR2_Pin};
uint32_t lastIrValue[2];
uint32_t lastHighIrTime[2] = {0, 0};
uint32_t refreshRate = 100;
bool forceRefresh = false;
uint32_t lastClickTime = millis();
uint8_t targetLapCount = 0;
bool false_starts[2] = {false, false};

void start_click();
void start_longpress();
void printTime(LiquidCrystal_PCF8574 *display, uint32_t time)
{
  static char buffer[6];
  int t = time / 100;
  sprintf(buffer, "%02d.%d", t / 10, t % 10);
  display->print(buffer);
}

void sync()
{
  refreshRate = 100;
  digitalWrite(SWLED_Pin, LOW);
  for (int i = 0; i < 2; i++)
  {
    displays[i].clear();
    displays[i].setCursor(0, 0);
    displays[i].print("Syncing");
    displays[i].setCursor(0, 1);
    displays[i].print("Sig:");
  }
  STATE = SYNC;
}

void warmup()
{
  refreshRate = 100;
  false_starts[0] = false_starts[1] = false;
  t0 = millis() + 4999;
  for (int i = 0; i < 2; i++)
  {
    displays[i].clear();
    displays[i].setCursor(0, 0);
    displays[i].print("Get Ready...");
  }
  STATE = WARMUP;
}

void false_start()
{
  digitalWrite(SR_STORECLK_Pin, LOW);
  shiftOut(SR_DATAPin, SR_CLOCKPin, MSBFIRST, 0);
  digitalWrite(SR_STORECLK_Pin, HIGH);
  refreshRate = 500;
  for (int i = 0; i < 2; i++)
  {
    if (false_starts[i])
    {
      displays[i].clear();
      displays[i].print("Faux Depart  !");
    }
    else
    {
      displays[i].clear();
    }
  }
  STATE = FALSE_START;
}

void run()
{
  refreshRate = 100;
  digitalWrite(SWLED_Pin, LOW);
  tone(BUZZ_Pin, 2000);
  digitalWrite(SR_STORECLK_Pin, LOW);
  shiftOut(SR_DATAPin, SR_CLOCKPin, MSBFIRST, 0);
  digitalWrite(SR_STORECLK_Pin, HIGH);
  for (int i = 0; i < 2; i++)
  {
    displays[i].clear();
    displays[i].setCursor(0, 0);
    displays[i].print("LAP:"); //LAP:## Best:##.#
    displays[i].setCursor(7, 0);
    displays[i].print("Best:");
    displays[i].setCursor(0, 1);
    displays[i].print("Time:"); //Time:##.# Pos:#
    displays[i].setCursor(11, 1);
    displays[i].print("Pos:");

    lapCount[i] = 0;
    lapStartTime[i] = t0;
    bestLap[i] = UINT32_MAX;
    lastLapTime[i] = 0;
    lastIrValue[i] = 0;
  }
  STATE = RUN;
}

void result()
{
  refreshRate = 1000;
  forceRefresh = true;
  for (int i = 0; i < 2; i++)
  {
    displays[i].clear();
    if (i == winner)
    {
      displays[i].print("Winner !!!");
    }
    else
    {
      displays[i].print("Next time... ?!");
    }
  }
  STATE = RESULT;
}

void wait()
{
  digitalWrite(SWLED_Pin, HIGH);
  refreshRate = 1000;
  forceRefresh = true;
  STATE = WAIT;
}

void config()
{
  digitalWrite(SWLED_Pin, HIGH);
  refreshRate = 200;
  displays[1].clear();
  displays[0].clear();
  displays[1].print("Race Lap :");
  STATE = SETUP;
}

void sleep()
{
  refreshRate = 5000;
  for (int i = 0; i < 2; i++)
  {
    displays[i].setBacklight(0);
    displays[i].clear();
    displays[i].print("Zzzzzzzzzzzz");
  }
  digitalWrite(SWLED_Pin, LOW);
  STATE = SLEEP;
}

void wakeup()
{
  displays[0].setBacklight(255);
  displays[1].setBacklight(255);
  wait();
}

void start_click()
{
  lastClickTime = millis();
  switch (STATE)
  {
  case SYNC:
    break;
  case WAIT:
  case WARMUP:
    warmup();
    break;
  case FALSE_START:
    wait();
    break;
  case SETUP:
    targetLapCount++;
    if (targetLapCount > 20)
      targetLapCount = 0;
    break;
  case RUN:
    warmup();
    break;
  case RESULT:
    displays[0].setBacklight(255);
    displays[1].setBacklight(255);
    warmup();
    break;
  case SLEEP:
    wakeup();
    break;
  }
}

void start_longpress()
{
  lastClickTime = millis();
  switch (STATE)
  {
  case SYNC:
    break;
  case WAIT:
  case WARMUP:
  case FALSE_START:
  case RESULT:
  case RUN:
    config();
    break;
  case SETUP:
    EEPROM.update(0, targetLapCount);
    wait();
    break;
  case SLEEP:
    wakeup();
    config();
    break;
  }
}

void setup()
{
  pinMode(SR_DATAPin, OUTPUT);
  pinMode(SR_CLOCKPin, OUTPUT);
  pinMode(SR_STORECLK_Pin, OUTPUT);
  pinMode(BUZZ_Pin, OUTPUT);
  pinMode(SWLED_Pin, OUTPUT);
  pinMode(IR1_Pin, INPUT);
  pinMode(IR2_Pin, INPUT);
  pinMode(BEACON_Pin, OUTPUT);
  digitalWrite(SWLED_Pin, HIGH);
  targetLapCount = EEPROM.read(0);

  for (int i = 0; i < 2; i++)
  {
    displays[i].begin(16, 2);
    displays[i].setBacklight(128);
  }

  digitalWrite(SR_STORECLK_Pin, LOW);
  shiftOut(SR_DATAPin, SR_CLOCKPin, MSBFIRST, 255);
  digitalWrite(SR_STORECLK_Pin, HIGH);
  delay(500);
  digitalWrite(SR_STORECLK_Pin, LOW);
  shiftOut(SR_DATAPin, SR_CLOCKPin, MSBFIRST, 0);
  digitalWrite(SR_STORECLK_Pin, HIGH);

  startButton.attachClick(start_click);
  startButton.attachLongPressStart(start_longpress);
  sync();
}

void loop()
{
  int IrSig[2] = {0, 0};
  static bool newBestLap[] = {false, false};
  static bool newPosition[] = {true, true};
  static bool newLap[] = {true, true};
  static uint8_t positions[] = {1, 2};
  int warmupPhase;

  digitalWrite(BEACON_Pin, HIGH);
  startButton.tick();

  if (lastClickTime + SLEEPTimeout < millis() && lapStartTime[0] + SLEEPTimeout < millis() && lapStartTime[1] + SLEEPTimeout < millis())
    sleep();

  //Read ir signal
  for (int i = 0; i < 2; i++)
  {
    IrSig[i] = analogRead(irPin[i]);
    if (IrSig[i] > MINIRSig)
      lastHighIrTime[i] = millis();
    if (lastHighIrTime[i] + 5000 < millis())
      sync();
  }

  switch (STATE)
  {
  case SYNC:
    static uint32_t syncLastIrNOk[2] = {millis(), millis()};
    for (int i = 0; i < 2; i++)
    {
      if (IrSig[i] < MINIRSigForSync)
        syncLastIrNOk[i] = millis();
    }
    if (syncLastIrNOk[0] + 2000 < millis() && syncLastIrNOk[1] + 2000 < millis())
      wait();
    break;
  case WAIT:
    break;
  case WARMUP:
    refreshRate = 200;
    warmupPhase = (t0 - millis()) / 500; // Goes from 9 to 0
    if (warmupPhase % 2 == 1)
    {
      tone(BUZZ_Pin, 100);
    }
    else
    {
      noTone(BUZZ_Pin);
    }
    digitalWrite(SR_STORECLK_Pin, LOW);
    shiftOut(SR_DATAPin, SR_CLOCKPin, MSBFIRST, 65 - pow(2, 5 - floor(warmupPhase / 2)));
    digitalWrite(SR_STORECLK_Pin, HIGH);

    //False start detection
    for (int i = 0; i < 2; i++)
      if (IrSig[i] < MINIRSig)
        false_starts[i] = true;
    if (false_starts[0] || false_starts[1])
      false_start();

    if (millis() >= t0)
      run();
    break;
  case RUN:
    if (millis() > t0 + 1000)
      noTone(BUZZ_Pin);
    /*Lap Detection*/
    for (int i = 0; i < 2; i++)
    {
      if (millis() > lapStartTime[i] + MINLAPTime && IrSig[i] < MINIRSig && lastIrValue[i] > MINIRSig)
      {
        /* Car detected */
        unsigned long t = millis();
        lapCount[i]++;
        lastLapTime[i] = t - lapStartTime[i];
        newLap[i] = true;
        if (bestLap[i] != 0 && lastLapTime[i] < bestLap[i])
        {
          bestLap[i] = lastLapTime[i];
          newBestLap[i] = true;
        }

        if (lapCount[i] > lapCount[1 - i])
        {
          if (positions[i] != 1)
          {
            positions[i] = 1;
            positions[1 - i] = 2;
            newPosition[0] = newPosition[1] = true;
          }
        }

        lapStartTime[i] = t;
      }
      lastIrValue[i] = IrSig[i];
      if (targetLapCount > 0 && lapCount[i] >= targetLapCount)
      {
        te = millis();
        winner = i;
        result();
      }
    }
    break;
  case FALSE_START:
    if (t0 + 1000 > millis())
      tone(BUZZ_Pin, 100);
    else
      noTone(BUZZ_Pin);

    if (t0 + 5000 < millis())
      wait();
    break;
  case RESULT:
  case SETUP:
  case SLEEP:
    break;
  }

  /*Display*/
  digitalWrite(BEACON_Pin, LOW);
  static unsigned long lastPrint = millis();
  if (forceRefresh || millis() > (lastPrint + refreshRate))
  {
    lastPrint = millis();
    switch (STATE)
    {
    case SYNC:
      for (int i = 0; i < 2; i++)
      {
        displays[i].setCursor(5, 1);
        displays[i].print(IrSig[i]);
        displays[i].print("   ");
      }
      break;
    case WAIT:
      for (int i = 0; i < 2; i++)
      {
        displays[i].clear();
        displays[i].print("Press start");
      }
      break;
    case WARMUP:
      break;
    case FALSE_START:
      break;
    case RUN:
      for (int i = 0; i < 2; i++)
      {
        if (newLap[i])
        {
          displays[i].setCursor(4, 0);
          displays[i].print(targetLapCount == 0 ? lapCount[i] : targetLapCount - lapCount[i]);
          newLap[i] = false;
        }
        if (newBestLap[i])
        {
          displays[i].setCursor(12, 0);
          printTime(&displays[i], bestLap[i]);
          newBestLap[i] = false;
        }
        displays[i].setCursor(5, 1);
        unsigned long lap = lastLapTime[i] > 0 && lapStartTime[i] + MINLAPTime > millis() ? lastLapTime[i] : millis() - lapStartTime[i];
        printTime(&displays[i], lap);
        if (newPosition[i])
        {
          displays[i].setCursor(15, 1);
          displays[i].print(positions[i]);
          newPosition[i] = false;
        }
      }
      break;
    case RESULT:
      if ((millis() - te) % 1000 < 500)
        displays[winner].setBacklight(255);
      else
        displays[winner].setBacklight(0);
      break;
    case SETUP:
      displays[1].setCursor(10, 0);
      displays[1].print("   ");
      displays[1].setCursor(10, 0);
      displays[1].print(targetLapCount);
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