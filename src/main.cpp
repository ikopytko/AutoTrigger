// #define SIM

#include <Arduino.h>
#include <U8g2lib.h>
#include <FireTimer.h>
#include "button.h"
#include "battery.h"

#define wrap(amt, low, high) ((amt) < (low) ? (high) : ((amt) > (high) ? (low) : (amt)))

#if defined(SIM)

#define ENCODER_CLK 3
#define ENCODER_DT 6
#define ENCODER_SW 7

#define BUTTON 11
#define BATT A3
#define VREF EXTERNAL

#else

#define OLED_DC 4
#define OLED_RESET 5

#define ENCODER_CLK 3
#define ENCODER_DT 16
#define ENCODER_SW 17

#define BUTTON 14
#define BATT A1
#define VREF INTERNAL

#endif

#define LED 10
#define OPTO1 8
#define OPTO2 9

#if defined(SIM)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0);
#else
U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI display(U8G2_R0, U8X8_PIN_NONE, OLED_DC, OLED_RESET);
#endif

enum screens
{
  Menu,
  StartCountDown,
  Shooting,
  Fnished,
  Flashlight
};

typedef struct
{
  int16_t value;
  int16_t minValue;
  int16_t maxValue;
  char const *unit;
} value;

typedef struct
{
  char const *name;
  uint16_t valueCount;
  value values[2];
} function;

#define DELAY 0
#define LENGTH 1
#define INTERV 2
#define REPEAT 3
#define TIMER 0
#define TIMES 1

#define STP_STATE 0
#define TRIG_STATE 1
#define WAIT_STATE 2
#define REPT_STATE 3
#define FINISH_STATE 4

byte currentState = STP_STATE;
uint16_t shootsFired = 0;

function functions[] = {
    {"DELAY", 1, {{15, 0, 100, "s"}}},
    {"LENGTH", 1, {{20, 0, 600, "s"}}},
    {"INTERV", 2, {{10, 0, 600, "s"}, {3, 0, 99, "x"}}},
    {"REPEAT", 2, {{30, 0, 600, "s"}, {1, 0, 10, "x"}}},
};

#define menuCount (sizeof(functions) / sizeof(function))

typedef struct
{
  int8_t item;
  boolean selected;
  uint8_t value;

} s_cursor;

s_cursor cursor;

Button btnSelect = Button(ENCODER_SW);
Button btnStart = Button(BUTTON);
Battery battery = Battery(BATT);

int batteryPercent;
int lightLevel;
screens currentScreen = Menu;
bool countdownStarted = false;

FireTimer msTimer;

void readEncoder();
void setupStates();

void setup()
{
  // Serial.begin(19200);
  analogReference(VREF);

  pinMode(A6, INPUT);

  pinMode(ENCODER_CLK, INPUT);
  pinMode(ENCODER_DT, INPUT);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), readEncoder, FALLING);

  pinMode(BATT, INPUT);
  digitalWrite(BATT, HIGH);

  pinMode(OPTO1, OUTPUT);
  pinMode(OPTO2, OUTPUT);

  pinMode(LED, OUTPUT);
  analogWrite(LED, 0);

  display.begin();
  display.clear();
  display.setFont(u8g2_font_princess_tr);
}

void triggerCamera(bool shutter)
{
  if (shutter)
  {
    digitalWrite(OPTO1, HIGH);
    digitalWrite(OPTO2, HIGH);
  }
  else
  {
    digitalWrite(OPTO1, LOW);
    digitalWrite(OPTO2, LOW);
  }
}

volatile short encoderDirection = 0;
void readEncoder()
{
  int dtValue = digitalRead(ENCODER_DT);
  if (dtValue == HIGH)
  {
    encoderDirection = -1;
  }
  if (dtValue == LOW)
  {
    encoderDirection = 1;
  }
}

short getEncoderDirection()
{
  short result;
  noInterrupts();
  result = encoderDirection;
  encoderDirection = 0;
  interrupts();
  return result;
}

void processMenuNavigation(short rotationDir, short btnState, short btnStartState)
{
  if (btnStartState == Button::HOLD)
  {
    currentScreen = screens::StartCountDown;
    countdownStarted = false;
    return;
  }
  if (btnState == Button::CLICK)
  {
    if (cursor.selected)
    {
      cursor.value++;
      if (cursor.value >= functions[cursor.item].valueCount)
      {
        cursor.selected = false;
        cursor.value = 0;
      }
    }
    else
    {
      cursor.selected = true;
    }
  }

  if (cursor.selected)
  {
    functions[cursor.item].values[cursor.value].value += rotationDir;
    uint16_t clamped = constrain(functions[cursor.item].values[cursor.value].value,
                                 functions[cursor.item].values[cursor.value].minValue,
                                 functions[cursor.item].values[cursor.value].maxValue);
    functions[cursor.item].values[cursor.value].value = clamped;
  }
  else
  {
    cursor.item += rotationDir;
    cursor.item = wrap(cursor.item, 0, menuCount - 1);
  }
}

void drawMenu()
{
  char buffer[10];

  display.clearBuffer();
  display.setDrawColor(1);

  // menu items
  for (size_t i = 0; i < menuCount; i++)
  {
    u8g2_uint_t local_x, local_y;
    local_x = (i % 2) * 60;
    local_y = (i / 2) * 30;

    display.drawPixel(local_x, local_y);
    display.drawRFrame(local_x + 1, local_y + 1, 58, 28, 5);
    if (cursor.item == i)
    {
      display.drawRFrame(local_x, local_y, 60, 30, 5);
    }
    display.drawStr(local_x + 7, local_y + 14, functions[i].name);

    if (functions[i].valueCount > 0)
    {
      char str[5];
      double val = functions[i].values[0].value / 10.0;
      const char *unit = functions[i].values[0].unit;
      dtostrf(val, 1, 1, str);
      sprintf(buffer, "%s%s", str, unit);
      u8g2_uint_t textWidth = display.getUTF8Width(buffer);

      bool isCurrent = cursor.item == i && cursor.selected && cursor.value == 0;
      if (isCurrent)
      {
        display.drawBox(local_x + 7, local_y + 25 - 10, textWidth, 11);
        display.setDrawColor(0);
        display.drawStr(local_x + 7, local_y + 25, buffer);
        display.setDrawColor(1);
      }
      else
      {
        display.drawStr(local_x + 7, local_y + 25, buffer);
      }
    }
    if (functions[i].valueCount > 1)
    {
      uint16_t val = functions[i].values[1].value;
      const char *unit = functions[i].values[1].unit;
      sprintf(buffer, "%d%s", val, unit);
      u8g2_uint_t textWidth = display.getUTF8Width(buffer);

      bool isCurrent = cursor.item == i && cursor.selected && cursor.value == 1;
      if (isCurrent)
      {
        display.drawBox(local_x + 55 - textWidth, local_y + 25 - 10, textWidth, 11);
        display.setDrawColor(0);
        display.drawStr(local_x + 55 - textWidth, local_y + 25, buffer);
        display.setDrawColor(1);
      }
      else
      {
        display.drawStr(local_x + 55 - textWidth, local_y + 25, buffer);
      }
    }
  }

  // draw scrollbar
  u8g2_uint_t scrollbar_x = display.getWidth() - 7;
  const u8g2_uint_t scrollbar_tick_offset = 7;
  const u8g2_uint_t scrollbar_tick_size = 7;
  for (size_t i = 0; i < menuCount; i++)
  {
    u8g2_uint_t scrollbar_y = i * (scrollbar_tick_offset + scrollbar_tick_size);
    if (cursor.item == i)
    {
      display.drawRBox(scrollbar_x, scrollbar_y, scrollbar_tick_size, scrollbar_tick_size, 3);
    }
    else
    {
      display.drawRFrame(scrollbar_x, scrollbar_y, scrollbar_tick_size, scrollbar_tick_size, 3);
    }
  }

  // draw battery bar
  display.drawFrame(0, 60, 120, 4);
  display.drawBox(0, 60, map(batteryPercent, 0, 100, 0, 120), 4);
  // sprintf(buffer, "%d%%", batteryPercent);
  // display.drawStr(35, 25, buffer);

  // display.getStrWidth
  display.sendBuffer();
}

void executeCountdown()
{
  if (!countdownStarted)
  {
    countdownStarted = true;
    msTimer.update((ulong)(functions[DELAY].values[TIMER].value * 100));
  }
  else
  {
    if (msTimer.fire(false))
    {
      countdownStarted = false;
      currentScreen = screens::Shooting;
      currentState = STP_STATE;
      shootsFired = 0;
    }
  }
}

void processCountdownNavigation(short btnStartState)
{
  if (btnStartState == Button::CLICK)
  {
    countdownStarted = false;
    currentScreen = screens::Menu;
  }
}

void drawCountdown()
{
  char buffer[10];

  display.clearBuffer();
  display.setDrawColor(1);
  display.setFontMode(0);

  display.drawRFrame(10, 10, display.getWidth() - 10 * 2, display.getHeight() - 10 * 2, 7);
  u8g2_uint_t textWidth = display.getUTF8Width("START");
  int text_x = display.getWidth() / 2 - textWidth / 2;
  display.setDrawColor(0);
  display.drawBox(text_x - 1, 5, textWidth + 2, 10); // erase frame behind title
  display.setDrawColor(1);
  display.drawStr(text_x, 15, "START");

  double tmp = (functions[DELAY].values[TIMER].value - msTimer.timeDiff / 100) / 10.0;
  dtostrf(tmp, 1, 1, buffer);
  textWidth = display.getUTF8Width(buffer);
  text_x = display.getWidth() / 2 - textWidth / 2;
  display.drawStr(text_x, 37, buffer);

  display.sendBuffer();
}

uint16_t shootsRemaining = 3;
uint16_t repeatsRemaining = 3;

void setupState()
{
  shootsRemaining = functions[INTERV].values[TIMES].value;
  repeatsRemaining = functions[REPEAT].values[TIMES].value;
}

void executeShooting()
{
  switch (currentState)
  {
  case STP_STATE:
    shootsRemaining = functions[INTERV].values[TIMES].value;
    repeatsRemaining = functions[REPEAT].values[TIMES].value;
    msTimer.update(functions[LENGTH].values[TIMER].value * 100);
    currentState = TRIG_STATE;
    shootsFired++;
    triggerCamera(true);
    break;
  case TRIG_STATE:
    if (msTimer.fire())
    {
      triggerCamera(false);
      if (shootsRemaining > 1)
      {
        shootsRemaining--;
        msTimer.update(functions[INTERV].values[TIMER].value * 100);
        currentState = WAIT_STATE;
      }
      else if (repeatsRemaining > 1)
      {
        repeatsRemaining--;
        msTimer.update(functions[REPEAT].values[TIMER].value * 100);
        currentState = REPT_STATE;
      }
      else
      {
        currentState = FINISH_STATE;
      }
    }
    break;
  case WAIT_STATE:
    if (msTimer.fire())
    {
      msTimer.update(functions[LENGTH].values[TIMER].value * 100);
      currentState = TRIG_STATE;
      shootsFired++;
      triggerCamera(true);
    }
    break;
  case REPT_STATE:
    if (msTimer.fire())
    {
      shootsRemaining = functions[INTERV].values[TIMES].value;
      msTimer.update(functions[LENGTH].values[TIMER].value * 100);
      currentState = TRIG_STATE;
      shootsFired++;
      triggerCamera(true);
    }
    break;
  }
}

void drawShooting()
{
  char buffer[10];

  display.clearBuffer();
  display.setDrawColor(1);
  display.setFontMode(0);

  if (currentState == FINISH_STATE)
  {
    display.drawRFrame(10, 10, display.getWidth() - 10 * 2, display.getHeight() - 10 * 2, 7);
    u8g2_uint_t textWidth = display.getUTF8Width("DONE");
    u8g2_uint_t text_x = display.getWidth() / 2 - textWidth / 2;
    display.drawStr(text_x, 35, "DONE");
    display.sendBuffer();
    return;
  }

  display.drawStr(10, 30, "TRIG");
  display.drawStr(47, 30, "WAIT");
  display.drawStr(84, 30, "REPT");

  const uint16_t cellWidth = 35;
  uint16_t currentTimer;
  uint16_t cellOffset;
  switch (currentState)
  {
  case TRIG_STATE:
    cellOffset = cellWidth * 0;
    currentTimer = functions[LENGTH].values[TIMER].value * 100;
    display.drawHLine(10, 31, 35);
    break;
  case WAIT_STATE:
    cellOffset = cellWidth * 1;
    currentTimer = functions[INTERV].values[TIMER].value * 100;
    display.drawHLine(47, 31, 35);
    break;
  case REPT_STATE:
    cellOffset = cellWidth * 2;
    currentTimer = functions[REPEAT].values[TIMER].value * 100;
    display.drawHLine(84, 31, 35);
    break;
  }

  short progres = map(currentTimer - msTimer.timeDiff + 1, currentTimer, 0, 0, cellWidth);

  display.drawFrame(10, 0, cellWidth * 3, 13);
  display.drawVLine(10 + cellWidth * 1, 0, 13);
  display.drawVLine(10 + cellWidth * 2, 0, 13);
  display.drawBox(10 + cellOffset, 0, progres, 13);

  short totalNumber = functions[INTERV].values[TIMES].value * functions[REPEAT].values[TIMES].value;
  progres = map(shootsFired, 0, totalNumber, 0, cellWidth * 3);

  display.drawFrame(10, 40, cellWidth * 3, 13);
  display.drawBox(10, 40, progres, 13);

  char buffNum[4];
  itoa(totalNumber, buffNum, 10);
  itoa(shootsFired, buffer, 10);
  strcat(buffer, " / ");
  strcat(buffer, buffNum);
  u8g2_uint_t textWidth = display.getUTF8Width(buffer);
  u8g2_uint_t text_x = display.getWidth() / 2 - textWidth / 2;
  display.drawStr(text_x, 64, buffer);

  display.sendBuffer();
}

void processShootingNavigation(short btnStartState)
{
  if (btnStartState == Button::HOLD)
  {
    currentScreen = screens::Menu;
      triggerCamera(false);
  }

  if (currentState == FINISH_STATE && btnStartState == Button::CLICK)
  {
    currentScreen = screens::Menu;
      triggerCamera(false);
  }
}

void loop()
{
  batteryPercent = battery.checkBattery();

  short temp = getEncoderDirection();
  short btnState = btnSelect.checkButton();
  short btnStartState = btnStart.checkButton();

  //if (btnStartState != Button::NONE) {
  //  Serial.println(btnStart);
  //}

  // pos = constrain(pos + temp, 0, 16);
  // analogWrite(LED, min(pos * 16, 255));

  switch (currentScreen)
  {
  case screens::Menu:
    processMenuNavigation(temp, btnState, btnStartState);
    drawMenu();
    break;
  case screens::StartCountDown:
    executeCountdown();
    processCountdownNavigation(btnStartState);
    drawCountdown();
    break;
  case screens::Shooting:
    executeShooting();
    processShootingNavigation(btnStartState);
    drawShooting();
    break;
  default:
    break;
  }
}

// EEPROM:
// #define kEepromSetpoint ((uint16_t *)0)
// #define kEepromHysteresis ((uint16_t *)2)
// int getSetPoint() { return (int)eeprom_read_word(kEepromSetpoint); }
// int getHysteresis() { return (int)eeprom_read_word(kEepromHysteresis); }
// void setSetPoint(int v) { eeprom_write_word(kEepromSetpoint, (uint16_t)v); }
// void setHysteresis(int v) { eeprom_write_word(kEepromHysteresis, (uint16_t)v); }