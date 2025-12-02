#include <LiquidCrystal.h>
#include <Wire.h>
#include <DS3231.h>
#include <IRremote.h>

DS3231 clock;
RTCDateTime dt;

int tempPin = 0;
int buzzer = 2;
int receiver = 4;

struct Alarm {
  int hour;
  int minute;
  bool enabled;
  bool has_rung;  // Track si l'alarme a déjà sonné cette minute
};

Alarm my_alarm;
IRrecv irrecv(receiver);
uint32_t last_decodedRawData = 0;

LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// Variables pour les modes et snooze
int edit_mode = 0;  // 0 = normal, 1 = edit hour, 2 = edit minute
int snooze_time = 0;  // Pour tracker le snooze (en minutes)

// Codes hex de ta télécommande
uint32_t BUTTON_ON = 3208707840;      // Bouton ON/Activation
uint32_t BUTTON_OFF = 3091726080;     // Bouton OFF/Désactivation
uint32_t BUTTON_SET = 4061003520;     // Bouton SET/OK
uint32_t BUTTON_PLUS = 4127850240;    // Bouton + (augmenter)
uint32_t BUTTON_MINUS = 4161273600;   // Bouton - (diminuer)
uint32_t BUTTON_OK = 3860463360;      // Bouton OK (confirmer)
uint32_t BUTTON_STOP_ALARM = 3125149440;  // Bouton pour arrêter l'alarme sans snooze


void setup()
{
  lcd.begin(16, 2);
  clock.begin();
  clock.setDateTime(__DATE__, __TIME__);
  pinMode(buzzer, OUTPUT);
  irrecv.enableIRIn();
  
  // Initialise l'alarme
  my_alarm.hour = 21;
  my_alarm.minute = 0;
  my_alarm.enabled = false;
  my_alarm.has_rung = false;
  
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

  // Gère le snooze : réinitialise après les 10 minutes écoulées
  if (snooze_time > 0 && dt.minute >= snooze_time) {
    snooze_time = 0;
  }

  // Vérifie si l'alarme doit sonner
  bool should_ring = my_alarm.enabled && 
                     dt.hour == my_alarm.hour && 
                     dt.minute == my_alarm.minute && 
                     !my_alarm.has_rung &&
                     snooze_time == 0;  // Ne sonne pas si en snooze
  
  if (should_ring) {
    my_alarm.has_rung = true;  // Marque qu'elle a sonné
    
    // La sonnerie!
    for (int i = 0; i < 50; i++) {
      digitalWrite(buzzer, HIGH);
      delay(sound_duration);
      digitalWrite(buzzer, LOW);
      delay(sound_duration);
      
      // Écoute la télécommande pendant la sonnerie
      if (irrecv.decode()) {
        Serial.println(irrecv.decodedIRData.decodedRawData);
        
        if (irrecv.decodedIRData.decodedRawData == BUTTON_STOP_ALARM) {
          // Arrête complètement l'alarme (ne change pas l'heure)
          digitalWrite(buzzer, LOW);
          irrecv.resume();
          break;
        } 
        else {
          // N'importe quel autre bouton = snooze 10 minutes
          int new_minute = (dt.minute + 10) % 60;
          int new_hour = dt.hour;
          
          // Si les minutes dépassent 59, on ajoute 1 à l'heure
          if ((dt.minute + 10) >= 60) {
            new_hour = (dt.hour + 1) % 24;  // % 24 pour gérer 23h -> 0h
          }
          
          my_alarm.minute = new_minute;
          my_alarm.hour = new_hour;
          snooze_time = new_minute;
          
          digitalWrite(buzzer, LOW);
          irrecv.resume();
          break;
        }
      }
    }
  }
  
  // Réinitialise has_rung quand on change de minute
  if (dt.minute != my_alarm.minute || dt.hour != my_alarm.hour) {
    my_alarm.has_rung = false;
  }
  
  // Gère la télécommande
  if (irrecv.decode()) {
      Serial.println(irrecv.decodedIRData.decodedRawData);
      
      if (edit_mode == 0) {  // Mode normal
        if (irrecv.decodedIRData.decodedRawData == BUTTON_ON) {
          my_alarm.enabled = true;
        } else if (irrecv.decodedIRData.decodedRawData == BUTTON_OFF) {
          my_alarm.enabled = false;
        } else if (irrecv.decodedIRData.decodedRawData == BUTTON_SET) {
          edit_mode = 1;  // Passe en mode édition de l'heure
        }
      } 
      else if (edit_mode == 1) {  // Mode édition heure
        if (irrecv.decodedIRData.decodedRawData == BUTTON_PLUS) {  // Bouton +
          my_alarm.hour++;
          if (my_alarm.hour > 23) my_alarm.hour = 0;
        } else if (irrecv.decodedIRData.decodedRawData == BUTTON_MINUS) {  // Bouton -
          my_alarm.hour--;
          if (my_alarm.hour < 0) my_alarm.hour = 23;
        } else if (irrecv.decodedIRData.decodedRawData == BUTTON_OK) {  // Bouton OK
          edit_mode = 2;  // Passe en mode édition des minutes
        }
      }
      else if (edit_mode == 2) {  // Mode édition minutes
        if (irrecv.decodedIRData.decodedRawData == BUTTON_PLUS) {  // Bouton +
          my_alarm.minute += 1;
          if (my_alarm.minute > 59) my_alarm.minute = 0;
        } else if (irrecv.decodedIRData.decodedRawData == BUTTON_MINUS) {  // Bouton -
          my_alarm.minute -= 1;
          if (my_alarm.minute < 0) my_alarm.minute = 59;
        } else if (irrecv.decodedIRData.decodedRawData == BUTTON_OK) {  // Bouton OK
          edit_mode = 0;  // Retour au mode normal
        }
      }
      
      irrecv.resume();
  }
  
  // Affichage selon le mode
  if (edit_mode == 0) {  // Mode normal : affiche temp et heure actuelle
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T:      C");
    lcd.setCursor(2, 0);
    lcd.print(tempC);
    lcd.setCursor(0, 1);
    lcd.print("H:");
    lcd.print(dt.hour);
    lcd.print(":");
    if (dt.minute < 10) lcd.print("0");
    lcd.print(dt.minute);
    
    if (my_alarm.enabled) {  // Affiche l'alarme seulement si elle est activée
      lcd.print(" A:");
      lcd.print(my_alarm.hour);
      lcd.print(":");
      if (my_alarm.minute < 10) lcd.print("0");
      lcd.print(my_alarm.minute);
    }
  } 
  else if (edit_mode == 1) {  // Mode édition heure
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Set alarm hour");
    lcd.setCursor(0, 1);
    lcd.print(my_alarm.hour);
    lcd.print(":");
    if (my_alarm.minute < 10) lcd.print("0");
    lcd.print(my_alarm.minute);
  }
  else if (edit_mode == 2) {  // Mode édition minutes
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Set alarm min");
    lcd.setCursor(0, 1);
    lcd.print(my_alarm.hour);
    lcd.print(":");
    if (my_alarm.minute < 10) lcd.print("0");
    lcd.print(my_alarm.minute);
  }
  
  delay(500);
}