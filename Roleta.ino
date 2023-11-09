#include <Adafruit_Sensor.h>
#include "Adafruit_TSL2591.h"
#include <Wire.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include "Firebase_Arduino_WiFiNINA.h"
//definicja zmiennych
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define FB_HOST "roleta-67c9d-default-rtdb.europe-west1.firebasedatabase.app" //ulr bazy danych
#define FB_SECRET "Rr7b80vqi6CObtDdLcBs8XN0dj5djXxgf30kP3cA"  //secret do uwierzytelnienia bazy
//konifguracja pinow
#define DIR_PIN 3
#define STEP_PIN 2
#define EN_PIN 4
#define MS1_PIN 5
#define MS2_PIN 6
#define AUTOMODE_PERIOD 5*60*1000L
unsigned long interval_time = 0L ;
FirebaseData fbdata;
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);


int MaxSteps, CalMin, CalMax, Mode, Lvl, oldBlindLvl, BlindLvl, OneStep = 0;


void setup()
{
  //ustalenie stanow
  pinMode(STEP_PIN,OUTPUT);
  pinMode(DIR_PIN,OUTPUT);
  pinMode(EN_PIN,OUTPUT);
  pinMode(MS1_PIN,OUTPUT);
  pinMode(MS2_PIN,OUTPUT);

  digitalWrite(EN_PIN, HIGH);
  digitalWrite(MS1_PIN, HIGH);
  digitalWrite(MS2_PIN, HIGH);

  Serial.begin(9600);

  //połączenie z WIFi
  int WiFiStatus = WL_IDLE_STATUS;
  while (WiFiStatus != WL_CONNECTED) 
  {
    Serial.print("*");
    WiFiStatus = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(3000);
  }
  Serial.print("Polaczono z: ");
  Serial.println(WIFI_SSID);

  //Inicjacja połączenia z bazą danych czasu rzeczywistego
  Firebase.begin(FB_HOST, FB_SECRET, WIFI_SSID, WIFI_PASSWORD);

  //Konfiguracja czujnika
  LightSensorConf();
  //Ustawienie trybu na 0 przy uruchomieniu
  Firebase.setInt(fbdata, "/Control/Mode", 0);

}

void loop() 
{
  //Jeśli wykonuje loop arduino właczone
  Firebase.setInt(fbdata, "/Control/ArduinoOn", 1);
  
  //Kalibracja
  if (Firebase.getInt(fbdata, "/Control/CalMin")) 
  {
    if (fbdata.dataType() == "int") 
    {
      CalMin = fbdata.intData();
      if(CalMin == 1)
      { 
        digitalWrite(DIR_PIN, HIGH);
        int steps = 100;
        BlindStart(steps);
        Serial.println("Kalibracja w dol"); 
        MaxSteps+=100;
        Firebase.setInt(fbdata, "/Control/Cal", MaxSteps);     
      }
    }
  } 
  
  if (Firebase.getInt(fbdata, "/Control/CalMax")) 
  {
    if (fbdata.dataType() == "int") 
    {
      CalMax = fbdata.intData();
      if(CalMax == 1)
      {
        digitalWrite(DIR_PIN, LOW);
        int steps = 100;
        BlindStart(steps);
        Serial.println("Kalibracja w gore");   
      } 
    }
  }

  //Sprawdzanie w jakim trybie sterowania znajduje sie roleta
  if (Firebase.getInt(fbdata, "/Control/Mode")) 
  {
    if (fbdata.dataType() == "int") 
    {
      Mode = fbdata.intData();

      //Mode 1 Tryb ręczny. Zmienna Motor jako dodatkowa zmienna warunkowa uruchomienia silnik
      Firebase.getInt(fbdata, "/Control/Motor");
      int Motor = fbdata.intData();

      if((Mode == 1) && (Motor == 1))
      { 
        Firebase.getInt(fbdata, "/Control/Cal");
        OneStep = fbdata.intData()/100;
        Firebase.getInt(fbdata, "/Control/BlindLvl");
        BlindLvl = fbdata.intData();
        Firebase.getInt(fbdata, "/Control/oldBlindLvl");
        oldBlindLvl = fbdata.intData();
         
        if(BlindLvl > oldBlindLvl)
        {
          Lvl = (BlindLvl - oldBlindLvl)*OneStep;
          digitalWrite(DIR_PIN, HIGH);
          BlindStart(Lvl);
        }
        else
        {
          Lvl = (oldBlindLvl - BlindLvl)*OneStep;
          digitalWrite(DIR_PIN, LOW);
          BlindStart(Lvl);
        }
        delay(2000);
        Firebase.setInt(fbdata, "/Control/oldBlindLvl", BlindLvl);
        Firebase.setInt(fbdata, "/Control/Motor", 0);
      }

      //Mode 2 Tryb automatyczny. Sprawdza co 5 min natężenie i steruje roletą
      int LightLvl, LightRef, Light = 0;
      if(millis() - interval_time >= AUTOMODE_PERIOD)
      { 
        interval_time+=AUTOMODE_PERIOD ;
        if (Mode == 2)
        {
          
          LightRef = 0;
          Serial.println("Dziala tryb auto"); 
          Firebase.getString(fbdata, "/Control/SensivityLight");
          String sensivity = fbdata.stringData();
          if(sensivity ==  "Low") LightRef = 1500;
          else if(sensivity ==  "Medium") LightRef = 2000;
          else if(sensivity ==  "Low") LightRef = 2500;
          else Firebase.setInt(fbdata, "/Control/Mode", 0);          
          Serial.println(LightRef); 
          Light = ReadLux();
          Serial.println(Light);
          int Denominator = LightRef/100;

          if(LightRef < Light)
          {
            LightLvl = (Light-LightRef)/Denominator;
            if(LightLvl > 100) LightLvl = 100;
            Serial.println(LightLvl);
            Firebase.getInt(fbdata, "/Control/BlindLvl");
            BlindLvl = fbdata.intData();
            delay(500);
            if(LightLvl > BlindLvl)
            {
              Firebase.setInt(fbdata, "/Control/oldBlindLvl", LightLvl);
              Firebase.setInt(fbdata, "/Control/BlindLvl", LightLvl);
              Firebase.getInt(fbdata, "/Control/Cal");
              OneStep = fbdata.intData()/100;
              digitalWrite(DIR_PIN, HIGH);
              BlindStart((LightLvl-BlindLvl)*OneStep);
            }
            else
            {
              Firebase.setInt(fbdata, "/Control/oldBlindLvl", LightLvl);
              Firebase.setInt(fbdata, "/Control/BlindLvl", LightLvl);
              Firebase.getInt(fbdata, "/Control/Cal");
              OneStep = fbdata.intData()/100;
              digitalWrite(DIR_PIN, LOW);
              BlindStart((BlindLvl-LightLvl)*OneStep);
            }
          }
          else
          {
            Firebase.getInt(fbdata, "/Control/BlindLvl");
            BlindLvl = fbdata.intData();
            Firebase.getInt(fbdata, "/Control/Cal");
            OneStep = fbdata.intData()/100;
            digitalWrite(DIR_PIN, LOW);
            BlindStart(BlindLvl*OneStep);
            Firebase.setInt(fbdata, "/Control/oldBlindLvl", 0);
            Firebase.setInt(fbdata, "/Control/BlindLvl", 0);
          }
        }
      }
    }

  }
  //Odczytywanie wartości natężenia światła i wysyłanie do bazy danych
  Firebase.setInt(fbdata, "/Control/Lux", ReadLux());
  delay(100);

}
//Funkcja generująca impulsy prostokątne sterujące roletą
void BlindStart(int val)
{
  digitalWrite(EN_PIN, LOW);
  while(val>0)
  {
    delayMicroseconds(1000);
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(1000);
    digitalWrite(STEP_PIN, LOW);
    val--;
  }
  digitalWrite(EN_PIN, HIGH);
}
//Konfiguracja czujnika
void LightSensorConf(void)
{
  tsl.setGain(TSL2591_GAIN_LOW);
  tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
}
//Oczytywanie wartości natężenia Lux
int ReadLux(void)
{
  uint32_t fullread = tsl.getFullLuminosity();
  uint16_t ir, full;
  ir = fullread >> 16;
  full = fullread & 0xFFFF;
  int lux = tsl.calculateLux(full, ir);
  if(isnan(lux)) lux = 0;  
  return lux;
}


