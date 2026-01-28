#include <SPI.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Keypad.h>

#include <ClearDS1302.h>

int RTCrstPin = A3;
int RTCclkPin = A1;
int RTCdatPin = A2;
ClearDS1302 RTC1 = ClearDS1302(RTCdatPin, RTCrstPin, RTCclkPin);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


const byte ROWS = 4; // Four rows
const byte COLS = 4; // Four columns
char keys[ROWS][COLS] = {
  {'A','3','2','1'},
  {'B','6','5','4'},
  {'C','9','8','7'},
  {'D','#','0','*'}
};
byte rowPins[ROWS] = {2, 3, 4, 5}; // Connect to the row pinouts of the keypad
byte colPins[COLS] = {6, 7, 8, 9}; // Connect to the column pinouts of the keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);


#define PIN_BUZZER 11

int hour = 0;
int min = 0;
int sec = 0;
int msec = 0;

unsigned long lastTick=0;

#define STATE_IDLE 0
#define STATE_EDIT 1
#define STATE_ALARM 2

int current_state = 0;
char current_state_name[20];

void setup()
{

  Serial.begin(9600);
  
  Serial.println("Starting...");


  pinMode(PIN_BUZZER, 11);

 // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.display();

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print(hour);
  display.print(":");
  display.print(min);
  display.print(":");
  display.print(sec);
  display.print(".");
  display.print(msec);

  display.display();

  //RTC1.set.time.SetAll(int second, int minute, int hour, int day, int date, int month, int year, bool ClockRegister)
  RTC1.set.time.SetAll(0, 30, 12, 2, 26, 7, 2025, false);

  delay(10);
}

  int DISPLAY_INTERVAL = 100;
  unsigned long display_updated = 0;

char pressed_key;

char time_str[100];
void loop()
{

char key1 = keypad.getKey();
  if (key1 != NO_KEY) {
    Serial.print("Key: ");
    Serial.println(key1);
    pressed_key = key1;
  }

  if(pressed_key == 'C')
  {
    Serial.println("Buzzer ON");
    tone(PIN_BUZZER, 100);
  }
  else
  {
   Serial.println("Buzzer OFF");
   noTone(PIN_BUZZER);
  }

  // char key = keypad.getKey();
  // if (key != NO_KEY){
  //   debouncer.update();
  //   if (debouncer.fell()){
  //     Serial.println(key);
  //   }
  // }

  unsigned long now = millis();

   if (now - lastTick >= 10)
  {
    lastTick += 10;   // ВАЖНО: не now, а +1
    msec+=10;
  }

  if (msec >= 100)
  {
    msec = 0;
    sec++;
  }

  if (sec >= 60)
  {
    sec = 0;
    min++;
  }

  if (min >= 60)
  {
    min = 0;
    hour++;
  }

  if (hour >= 24)
  {
    hour = 0;
  }
  


  if(millis() > display_updated + DISPLAY_INTERVAL){

    display.setCursor(0,0);
    display.clearDisplay();
    display.print(hour);
    display.print(":");
    display.print(min);
    display.print(":");
    display.print(sec);
    display.print(".");
    display.print(msec);

    display.setCursor(0,20);
    display.print("Pressed: ");
    display.print(pressed_key);


    display.display();

    display_updated = millis();

  byte * timestr1 = RTC1.get.time.GetAll();

    Serial.println((char *) timestr1);
  }

}
