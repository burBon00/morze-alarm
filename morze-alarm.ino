#include <SPI.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


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


#define PIN_BUZZER 11

int hour = 0;
int min = 0;
int sec = 0;
int msec = 0;

unsigned long lastTick = 0;

#define STATE_IDLE 0
#define STATE_EDIT 1
#define STATE_ALARM 2

int current_state = 0;
char current_state_name[20];


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

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  display.setTextColor(SSD1306_WHITE);
}

int DISPLAY_INTERVAL = 1000;
unsigned long display_updated = 0;
char pressed_key;

void printDigits(uint8_t digits) {
  if (digits < 10) {
    display.print('0');
  }
  display.print(digits);
}

void loop() {

  char key1 = keypad.getKey();
  if (key1 != NO_KEY) {
    Serial.print("Key: ");
    Serial.println(key1);
    pressed_key = key1;
  }

  if (pressed_key == 'C') {
    Serial.println("Buzzer ON");
    tone(PIN_BUZZER, 100);
  } else {
    //Serial.println("Buzzer OFF");
    noTone(PIN_BUZZER);
  }

  // char key = keypad.getKey();
  // if (key != NO_KEY){
  //   debouncer.update();
  //   if (debouncer.fell()){
  //     Serial.println(key);
  //   }
  // }


  if (millis() > display_updated + DISPLAY_INTERVAL) {

    RtcDateTime nowTime = Rtc.GetDateTime();

    printDateTime(nowTime);
    Serial.println();

    display.setCursor(17, 0);
    display.clearDisplay();
    display.setTextSize(2);
    printDigits(nowTime.Hour());
    display.print(":");
    printDigits(nowTime.Minute());
    display.print(":");
    printDigits(nowTime.Second());
    display.setTextSize(1);
    if (!nowTime.IsValid()) {
      Serial.println("RTC lost confidence in the DateTime!");
      display.print("<- ERROR!");
    }

    display.setCursor(0, 20);
    display.print("Pressed: ");
    display.print(pressed_key);


    display.display();

    display_updated = millis();
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
