#include <LiquidCrystal.h>
#include <Wire.h>
#include <DS3231.h>
#include <IRremote.h>

DS3231 clock;
RTCDateTime dt;

int tempPin = 0;
int buzzer = 2;
int has_ring = 0;
int receiver = 4;

IRrecv irrecv(receiver);
uint32_t last_decodedRawData = 0;

LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

void translateIR() 
{
  if (irrecv.decodedIRData.flags)
  {
    irrecv.decodedIRData.decodedRawData = last_decodedRawData;
  } else
  {
    Serial.println(irrecv.decodedIRData.decodedRawData, HEX);
  }
  last_decodedRawData = irrecv.decodedIRData.decodedRawData;
}

void setup()
{
  lcd.begin(16, 2);
  clock.begin();
  clock.setDateTime(__DATE__, __TIME__);
  pinMode(buzzer, OUTPUT);
  irrecv.enableIRIn();
  Serial.begin(9600);
}


void loop()
{
  dt = clock.getDateTime();
  int tempReading = analogRead(tempPin);
  int sound_duration = 500;
  
  double tempK = log(10000.0 * ((1024.0 / tempReading - 1)));
  tempK = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * tempK * tempK )) * tempK );     
  float tempC = tempK - 273.15;            
  float tempF = (tempC * 9.0)/ 5.0 + 32.0; 

  if (irrecv.decode()) // have we received an IR signal?
  {
    translateIR();
    irrecv.resume(); // receive the next value
  }
  
  if (dt.minute % 2 == 0 && has_ring == 0 && irrecv.decodedIRData.decodedRawData != 0xB847FF00) {
    for (int i = 0; i < 20; i++){
      
      if (irrecv.decode()) {
        translateIR();
        irrecv.resume();
        
        if (irrecv.decodedIRData.decodedRawData == 0xB847FF00) {
          digitalWrite(buzzer, LOW);
          break; 
        }
      }
      
      digitalWrite(buzzer, HIGH);
      delay(sound_duration);
      digitalWrite(buzzer, LOW);
      delay(sound_duration);
    }
    has_ring = 1;
  }

  if (dt.minute % 2 == 1) {
    has_ring = 0;
  }
  
  
  lcd.setCursor(0, 0);
  lcd.print("Temp         C  ");
  lcd.setCursor(6, 0);
  lcd.print(tempC);
  lcd.setCursor(0, 1);
  lcd.print(dt.hour);
  lcd.print(":");
  lcd.print(dt.minute);
  lcd.print(":");
  lcd.print(dt.second);
  lcd.print(" ");
  lcd.print(dt.day);
  lcd.print("-");
  lcd.print(dt.month);
  //delay(50);
}
