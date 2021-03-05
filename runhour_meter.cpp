#include <uEEPROMLib.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_ADS1015.h>
#include "RTClib.h"
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

File myStorage;

Adafruit_ADS1115 ads;
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 rtc;

uEEPROMLib eeprom(0x57);

char daysOfTheWeek[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

unsigned int eeAddress1 = 5;
unsigned int eeAddress2 = 6;
unsigned int eeAddress3 = 7;
unsigned int eeAddress4 = 8;

bool recordState = false;

const float FACTOR = 100;
const float multiplier = 0.0625F;

// declare a type for the various states of our machine
enum t_weldingState : uint8_t {WELDING_IDLE, WELDING_IN_USE};

// define a variable to keep track of current state and initialize as IDLE
t_weldingState weldingState  = WELDING_IDLE;

// variables to keep track of start and end time + duration
uint32_t weldingStartTime, weldingEndTime;
uint32_t weldingDuration;

uint32_t _totalrunhour;
uint32_t _runhour;
uint32_t TotalRunHour;
uint32_t RunHour;

unsigned long startRecord;
unsigned long stopRecord;
const uint64_t recordTime = 60000;

const int chipSelect = 4;

int powerInput = A1;
int powerState = 0;

//float amps;

//..

bool isNewWeldingTimeReady()
{
  DateTime now = rtc.now();
  //  lcd.setCursor(0, 0);
  //  lcd.print(now.timestamp(DateTime::TIMESTAMP_TIME));
  //  lcd.setCursor(9, 0);
  //  lcd.print(daysOfTheWeek[now.dayOfTheWeek()]);

  bool durationIsReady = false;
  float voltage;
  float current;
  float sum = 0;
  long time = millis();
  int counter = 0;

  while (millis() - time < 500)
  {
    voltage = ads.readADC_Differential_0_1() * multiplier;
    current = voltage * FACTOR;
    current /= 1000.0;
    sum += sq(current);
    counter = counter + 1;
  }

  current = sqrt(sum / counter);

  switch (weldingState) {
    case WELDING_IDLE: // we are waiting for the start of the process

      if (current >= 4.00) {
        // welding started, record time and make note of state change
        weldingStartTime = now.unixtime();
        //        Serial.println(weldingStartTime);
        weldingState = WELDING_IN_USE;
      }
      break;

    case WELDING_IN_USE: // we are waiting for the end of the process

      float voltage;
      float current;
      float sum = 0;
      long time = millis();
      int counter = 0;

      while (millis() - time < 500)
      {
        voltage = ads.readADC_Differential_0_1() * multiplier;
        current = voltage * FACTOR;
        current /= 1000.0;
        sum += sq(current);
        counter = counter + 1;
      }

      current = sqrt(sum / counter);

      if (current <= 4.00) {
        // welding ended, record time and make note of state change
        weldingEndTime = now.unixtime();
        //        Serial.println(weldingEndTime);
        weldingDuration = weldingEndTime - weldingStartTime;
        durationIsReady = true;
        weldingState = WELDING_IDLE;
      }
      break;
  }
  return durationIsReady;
  return current;
}

void writeHourData()
{
  myStorage = SD.open("totRhr.txt", FILE_WRITE);

  if (myStorage)
  {
    myStorage.println(_totalrunhour);
    //    Serial.println("Hour data written to SD");
    myStorage.close();
  }
  delay(20);
  myStorage = SD.open("R_hour.txt", FILE_WRITE);

  if (myStorage)
  {
    myStorage.println(_runhour);
    //    Serial.println("Hour data written to SD");
    myStorage.close();
  }
  delay(20);
}

void setup()
{
  Serial.begin(9600);
  ads.setGain(GAIN_TWO);

#ifndef ESP8266
  while (!Serial); // wait for serial port to connect. Needed for native USB
#endif

  pinMode(chipSelect, OUTPUT);

  if (! rtc.begin()) {
    //    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    //    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  if (!SD.begin(4))
  {
    //    Serial.println("initialization failed!");
    while (1);
  }
  else
  {
    //    Serial.println("initialization done.");
  }

  //  eeprom.eeprom_read(eeAddress1, &_totalrunhour);
  //  eeprom.eeprom_read(eeAddress2, &_runhour);
  eeprom.eeprom_read(eeAddress3, &TotalRunHour);
  eeprom.eeprom_read(eeAddress4, &RunHour);
  //  Serial.println("Total runhour: " + String(_totalrunhour));
  //  Serial.println("runhour: " + String(_runhour));
  //  Serial.println("Totalrunsecond: " + String(TotalRunHour));
  Serial.println("runsecond: " + String(RunHour));

  pinMode(powerInput, INPUT);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Tranos OEE meter");
  lcd.setCursor(0, 1);
  lcd.print("Version 1.0.");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("C.C");
  lcd.setCursor(0, 0);
  delay(2000);
  lcd.clear();
  //  lcd.setCursor(0, 0);
  //  lcd.print("T.R_Hr:" + String(_runhour));
  //  lcd.setCursor(0, 1);
  //  lcd.print("R_Hr:" + String(_totalrunhour));
}

void loop()
{
  powerState = digitalRead(powerInput);
  startRecord = millis();

  // check the welding machine state on a regular basis to keep track of usage
  if (isNewWeldingTimeReady()) {
    lcd.clear();
    if (RunHour / 3600 >= 8)
    {
      RunHour = 0;
    }
    RunHour += weldingDuration;
    TotalRunHour += weldingDuration;
  }
  _runhour = RunHour / 3600;
  _totalrunhour = _runhour;
//  Serial.println(_runhour);

  lcd.setCursor(0, 0);
  lcd.print("T.R_Hr:" + String(_runhour));
  lcd.setCursor(0, 1);
  lcd.print("R_Hr:" + String(_totalrunhour));

  if (startRecord - stopRecord >= recordTime)               //Write to EEPROM and SD card hourly
  {
    eeprom.eeprom_write(eeAddress1, _totalrunhour);
    eeprom.eeprom_write(eeAddress2, _runhour);
    eeprom.eeprom_write(eeAddress3, TotalRunHour);
    eeprom.eeprom_write(eeAddress4, RunHour);
    writeHourData();
    Serial.println("Data written to eeprom");
    stopRecord = startRecord;

  }

  //  if (powerState == LOW)                                  // Power fail detection
  //  {
  //    //    Serial.println("Data written at power outage");
  //    eeprom.eeprom_write(eeAddress1, _totalrunhour);
  //    eeprom.eeprom_write(eeAddress2, _runhour);
  //    eeprom.eeprom_write(eeAddress3, TotalRunHour);
  //    eeprom.eeprom_write(eeAddress4, RunHour);
  //  }
}
