#include <SPI.h>
#include <time.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <EEPROM.h>
#include <elapsedMillis.h>
#include <Keypad.h>

#include <RtcDS1302.h>


// CONNECTIONS:
// DS1302 CLK/SCLK --> 5
// DS1302 DAT/IO --> 4
// DS1302 RST/CE --> 2
// DS1302 VCC --> 3.3v - 5v
// DS1302 GND --> GND
ThreeWire myWire(A2, A1, A3);  // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);


const byte ROWS = 4;  // Four rows
const byte COLS = 4;  // Four columns
char keys[ROWS][COLS] = {
  { 'A', '3', '2', '1' },
  { 'B', '6', '5', '4' },
  { 'C', '9', '8', '7' },
  { 'D', '#', '0', '*' }
};
byte rowPins[ROWS] = { 2, 3, 4, 5 };  // Connect to the row pinouts of the keypad
byte colPins[COLS] = { 6, 7, 8, 9 };  // Connect to the column pinouts of the keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
char key;

#define PIN_BUZZER 11

int hour = 0;
int min = 0;
int sec = 0;
int msec = 0;

unsigned long lastTick = 0;

#define STATE_IDLE 0
#define STATE_EDIT_CLOCK 1
#define STATE_EDIT_ALARM 2
#define STATE_ALARM 3

char edit_buffer[3] = { '0', '0', '\0' };

int edit_buffer_count = 0;
int edit_alarm_position = 0;

int current_state = 0;
char current_state_name[20];
bool state_on_enter = true;

bool buzzerON = false;


/************************************/
struct alarm_obj {
  int alarmHour;
  int alarmMinute;
  bool alarmEnabled;
};

struct alarm_obj myAlarm;

#define ALARM_EEPROM_ADDR 0
#define OTHER_DATA_ADDR (ALARM_EEPROM_ADDR + sizeof(alarm_obj))

// Write alarm to EEPROM
void saveAlarmToEEPROM() {
  EEPROM.put(ALARM_EEPROM_ADDR, myAlarm);
  Serial.println("Alarm saved to EEPROM");
}

// Read alarm from EEPROM
void loadAlarmFromEEPROM() {
  EEPROM.get(ALARM_EEPROM_ADDR, myAlarm);

  // Validate data - if garbage, use defaults
  if (myAlarm.alarmHour < 0 || myAlarm.alarmHour > 23 || myAlarm.alarmMinute < 0 || myAlarm.alarmMinute > 59) {
    // Invalid data, use defaults
    myAlarm.alarmHour = 7;
    myAlarm.alarmMinute = 30;
    myAlarm.alarmEnabled = false;
    saveAlarmToEEPROM();  // Save valid defaults
  }

  delay(10);

  Serial.println("Alarm loaded from EEPROM");
  Serial.print("Hour: ");
  Serial.print(myAlarm.alarmHour);
  Serial.print(", Minute: ");
  Serial.print(myAlarm.alarmMinute);
  Serial.print(", Enabled: ");
  Serial.println(myAlarm.alarmEnabled);
}

/****************************************/


void setDate() {
  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);

  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);
  Serial.println();

  if (!Rtc.IsDateTimeValid()) {
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing

    Serial.println("RTC lost confidence in the DateTime!");
    Rtc.SetDateTime(compiled);
  }

  if (Rtc.GetIsWriteProtected()) {
    Serial.println("RTC was write protected, enabling writing now");
    Rtc.SetIsWriteProtected(false);
  }

  if (!Rtc.GetIsRunning()) {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
  } else if (now > compiled) {
    Serial.println("RTC is newer than compile time. (this is expected)");
  } else if (now == compiled) {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }
}


void setup() {

  Serial.begin(9600);

  Serial.println("Starting...");

  setDate();
  pinMode(PIN_BUZZER, 11);
  u8g2.begin();

  loadAlarmFromEEPROM();
}

int DISPLAY_INTERVAL = 100;
unsigned long display_updated = 0;
char pressed_key;

void printDigits(int x, int y, uint8_t digits) {
  char buffer[3];
  sprintf(buffer, "%02d", digits);
  u8g2.drawStr(x, y, buffer);
}

/****************************/
bool alarmWasTriggered = false;

void alarmOFF() {
  buzzerON = false;
  noTone(PIN_BUZZER);
}
/****************************/

void state_idle_enter() {
  Serial.println("state_idle_enter");
  alarmOFF();
  alarmWasTriggered = false;
}

void state_idle_run() {

  RtcDateTime nowTime = Rtc.GetDateTime();

  if (myAlarm.alarmEnabled && !alarmWasTriggered && nowTime.Hour() == myAlarm.alarmHour && nowTime.Minute() == myAlarm.alarmMinute && nowTime.Second() == 0) {

    alarmWasTriggered = true;
    switch_state(STATE_ALARM);
    return;
  }

  switch (key) {
    case 'B':
      switch_state(STATE_EDIT_ALARM);
      break;
    case 'D':
      myAlarm.alarmEnabled = !myAlarm.alarmEnabled;
      saveAlarmToEEPROM();
      break;
    default:
      break;
  }
}

void state_edit_alarm_enter() {
  Serial.println("state_edit_alarm_enter");
  alarmOFF();

  edit_buffer[0] = '0';
  edit_buffer[1] = '0';
  edit_buffer_count = 0;
  edit_alarm_position = 1;
}

void state_edit_alarm_run() {

  switch (key) {
    case 'B':
      edit_alarm_position++;
      edit_alarm_position = edit_alarm_position % 3;

      if (edit_alarm_position == 0) {
        saveAlarmToEEPROM();
        switch_state(STATE_IDLE);
      }
      break;
    case 'D':
      myAlarm.alarmEnabled = !myAlarm.alarmEnabled;
      saveAlarmToEEPROM();
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      bool invalid_input = false;
      if (edit_buffer_count >= 2) {
        edit_buffer_count = 0;
        edit_buffer[0] = '0';
        edit_buffer[1] = '0';
      }

      char buf0 = edit_buffer[0];
      edit_buffer[0] = edit_buffer[1];
      edit_buffer[1] = key;
      edit_buffer_count++;

      int i;
      sscanf(edit_buffer, "%d", &i);

      if (i < 0
          || (edit_alarm_position == 1 && i > 23)
          || (edit_alarm_position == 2 && i > 59)) {
        invalid_input = true;
      }

      if (invalid_input) {
        Serial.print("Invalid input: ");
        Serial.println(edit_buffer);

        edit_buffer[1] = edit_buffer[0];
        edit_buffer[0] = buf0;
        break;
      }

      if (edit_alarm_position == 1) {
        myAlarm.alarmHour = i;
      } else if (edit_alarm_position == 2) {
        myAlarm.alarmMinute = i;
      }

      Serial.print("edit_alarm_position: ");
      Serial.print(edit_alarm_position);
      Serial.print("->");
      Serial.println(edit_buffer);

      break;
    default:
      break;
  }
}

const int BUZZER_INTERVAL = 500;
const int MAX_ALARM_TIME = 10 * 1000;  // 1 minute
elapsedMillis buzzerTimer;
elapsedMillis alarmTimer;

void state_alarm_enter() {
  alarmTimer = 0;
  buzzerTimer = 0;
  buzzerON = true;

  Serial.print("state_alarm_enter Alarm timer: ");
  Serial.println(alarmTimer);
}

void state_alarm_run() {

  if (alarmTimer > MAX_ALARM_TIME) {
    Serial.print("Alarm timer: ");
    Serial.println(alarmTimer);
    switch_state(STATE_IDLE);
    return;
  }

  if (buzzerON) {
    tone(PIN_BUZZER, 100);
  } else {
    noTone(PIN_BUZZER);
  }

  if (buzzerTimer > BUZZER_INTERVAL) {
    // Serial.print("Alarm timer: ");
    // Serial.println(alarmTimer);
    buzzerON = !buzzerON;
    buzzerTimer = 0;
  }
}

void switch_state(int state) {
  if (state != current_state) {
    Serial.print("Switch state ");
    Serial.print(current_state);
    Serial.print("->");
    Serial.println(state);

    current_state = state;
    state_on_enter = true;
  }

  Serial.print("switch_state state_on_enter: ");
  Serial.println(state_on_enter);
}

void run_state() {
  if (state_on_enter) {
    Serial.print("run_state NEW: ");
    Serial.println(state_on_enter);
  }
  switch (current_state) {
    case STATE_EDIT_ALARM:
      if (state_on_enter) {
        state_edit_alarm_enter();
        state_on_enter = false;
        break;
      }

      state_edit_alarm_run();
      break;
    case STATE_ALARM:
      if (state_on_enter) {
        state_alarm_enter();
        state_on_enter = false;
        break;
      }

      state_alarm_run();
      break;
    default:
      // state IDLE
      if (state_on_enter) {
        state_idle_enter();
        state_on_enter = false;
        break;
      }

      state_idle_run();
      break;
  }
}

void loop() {

  key = keypad.getKey();
  if (key != NO_KEY) {
    Serial.print("Key: ");
    Serial.println(key);
    pressed_key = key;
  }

  if (key == 'A') {
    switch_state(STATE_IDLE);
  } else if (key == 'C') {
    switch_state(STATE_ALARM);
  }

  run_state();

  // if (pressed_key == 'C') {
  //   Serial.println("Buzzer ON");
  //   tone(PIN_BUZZER, 100);
  // } else {
  //   //Serial.println("Buzzer OFF");
  //   noTone(PIN_BUZZER);
  // }

  if (millis() > display_updated + DISPLAY_INTERVAL) {

    draw_display();

    display_updated = millis();
  }

  delay(5);
}


void draw_display() {

  u8g2.firstPage();
  do {
    switch (current_state)
    {
        case STATE_ALARM:
          displayAlarm();
          break;
        case STATE_EDIT_ALARM:
          drawClock();
          drawAlarmClock(true);
          break;
        default:
          drawClock();
          drawAlarmClock(false);
      }
  } while (u8g2.nextPage());
}

void displayAlarm()
{
  u8g2.setFont(u8g2_font_logisoso24_tn);

  int y = 30;
  int x = 10;
  u8g2.drawStr(x, y, "12345");
}

void drawClock() {
  RtcDateTime nowTime = Rtc.GetDateTime();
  // printDateTime(nowTime);
  // Serial.println();

  u8g2.setFont(u8g2_font_logisoso24_tn);

  int y = 30;
  int x = 10;
  printDigits(x, y, nowTime.Hour());
  x += 30;
  u8g2.drawStr(x, y, ":");
  x += 8;
  printDigits(x, y, nowTime.Minute());
  x += 30;
  u8g2.drawStr(x, y, ":");
  x += 8;
  printDigits(x, y, nowTime.Second());
}

void drawAlarmClock(bool always_show) {

  int x = 10;
  int y = 60;
  u8g2.setFont(u8g2_font_streamline_interface_essential_alert_t);
  if (myAlarm.alarmEnabled) {
    u8g2.drawGlyph(x, y, '1');  // Icon alarm enabled

  } else {
    u8g2.drawGlyph(x, y, '0');  // Icon alarm disabled
  }

  if (myAlarm.alarmEnabled || always_show) {
    u8g2.setFont(u8g2_font_logisoso16_tn);
    y = 58;
    x += 25;
    printDigits(x, y, myAlarm.alarmHour);


    if (edit_alarm_position == 1) {
      u8g2.setDrawColor(2);                 // Set color to XOR mode
      u8g2.drawBox(x - 1, y - 18, 22, 20);  // Invert a 128x64 area
      u8g2.setDrawColor(1);                 // Reset to default color
    }

    x += 20;
    u8g2.drawStr(x, y, ":");
    x += 7;
    printDigits(x, y, myAlarm.alarmMinute);
    if (edit_alarm_position == 2) {
      u8g2.setDrawColor(2);                 // Set color to XOR mode
      u8g2.drawBox(x - 1, y - 18, 22, 20);  // Invert a 128x64 area
      u8g2.setDrawColor(1);                 // Reset to default color
    }
  }
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt) {
  char datestring[26];

  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             dt.Month(),
             dt.Day(),
             dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second());
  Serial.print(datestring);
}
