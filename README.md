# Automated Thermal Switch (ATS) ❄️🚗

> 📺 **Video Showcase:** Watch the full mechanical build and system testing in the video, [DIY Automated Thermal Power Switch💫](https://www.youtube.com/watch?v=oN5PlHBWUPk). Developed by Abhirup.

## 📌 System Architecture

This project transforms a standard vehicle air conditioning button into a smart, automated climate control system. Instead of relying on standard `delay()` loops, the ESP8266 runs a custom state-machine OS designed for driver safety and zero-interruption execution.

* **Smart Actuation:** An SG90 servo mechanically toggles the flush AC switch precisely when the ambient cabin temperature crosses the user-defined threshold.
* **Gesture Override:** An integrated IR sensor allows drivers to manually trigger the AC via a hand wave, eliminating the need to look away from the road to press a screen.
* **Persistent Memory:** Internal EEPROM automatically saves target temperature settings and LCD backlight preferences, retaining them perfectly across vehicle power cycles.
* **Non-Blocking Logic:** Built entirely utilizing `millis()` timers, ensuring the DHT11 sensor readings, IR detection, and debounced button holds run concurrently without freezing the system.

## ⚙️ Hardware & Pinout

| Component | NodeMCU (ESP-12E) Pin | Internal GPIO | Power Connection |
| :--- | :--- | :--- | :--- |
| **16x2 LCD (I2C SCL)** | D1 | GPIO5 | 3V / 5V |
| **16x2 LCD (I2C SDA)** | D2 | GPIO4 | 3V / 5V |
| **Left Button (Temp -)**| D3 | GPIO0 | GND (Internal Pullup) |
| **Right Button (Temp +)**| D7 | GPIO13 | GND (Internal Pullup) |
| **Active Buzzer (+)** | D4 | GPIO2 | GND |
| **DHT11 Data Pin** | D5 | GPIO14 | 3V |
| **SG90 Servo PWM** | D6 | GPIO12 | **VIN (5V Direct)** |
| **IR Sensor (OUT)** | D0 | GPIO16 | **VIN (5V Direct)** |

## 🚀 Build Tutorial & Source Code

1. **Prepare the I2C Display:** Solder an I2C backpack to the 16x2 LCD to reduce the required data wires from six down to two (SCL and SDA). 
2. **Wire the Input Sensors:** Connect the DHT11 temperature sensor to D5. Connect the IR sensor's output to D0. Wire the two push buttons to D3 and D7, connecting their other legs directly to GND (the code utilizes `INPUT_PULLUP` to negate the need for external resistors).
3. **Route the High-Draw Components:** Connect the SG90 servo and the IR sensor power lines directly to the `VIN` pin. Power the NodeMCU using a 1A or 2.1A car USB adapter. *Warning: Powering the servo from the 3V logic pin will cause the ESP8266 to crash during actuation.*
4. **Compile the Firmware:** Install the `LiquidCrystal_I2C` and `DHT sensor library` in the Arduino IDE. Select your board as "NodeMCU 1.0 (ESP-12E Module)".
5. **Calibrate the Actuator:** Mount the servo inside the vehicle console. Adjust the `acServo.write(90)` value in the code to match the exact physical angle required to successfully depress the vehicle's specific AC button.

### C++ Firmware Code

```cpp
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Servo.h>
#include <EEPROM.h>

#define DHTPIN D5
#define DHTTYPE DHT11
#define SERVO_PIN D6
#define LEFT_BTN D3
#define RIGHT_BTN D7
#define BUZZER_PIN D4
#define IR_PIN D0

LiquidCrystal_I2C lcd(0x27, 16, 2); 
DHT dht(DHTPIN, DHTTYPE);
Servo acServo;

int targetTemp = 24;      
bool backlightOn = true;
bool acIsOn = true;       
int currentTemp = 0;

unsigned long lastDHTRead = 0;
unsigned long lastIRTrigger = 0;
unsigned long btnPressTime = 0;
bool actionDone = false;  

void setup() {
  pinMode(LEFT_BTN, INPUT_PULLUP);
  pinMode(RIGHT_BTN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(IR_PIN, INPUT); 
  digitalWrite(BUZZER_PIN, LOW);

  EEPROM.begin(512);
  byte savedTemp = EEPROM.read(0);
  byte savedBacklight = EEPROM.read(1);
  
  if (savedTemp != 255) {
    targetTemp = savedTemp;
    backlightOn = (savedBacklight == 1);
  }

  lcd.init();
  if (backlightOn) lcd.backlight();
  else lcd.noBacklight();
  dht.begin();

  lcd.setCursor(0, 0);
  lcd.print("   Automated    ");
  lcd.setCursor(0, 1);
  lcd.print(" Thermal Switch ");
  beep(100); delay(1500);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   By Abhirup   ");
  lcd.setCursor(0, 1);
  lcd.print(" System Boot... ");
  beep(100); delay(100); beep(100);
  delay(2000);
  lcd.clear();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastDHTRead >= 2000) {
    lastDHTRead = currentMillis;
    int readT = dht.readTemperature();
    if (!isnan(readT)) {
      currentTemp = readT;
      updateDisplay();
      checkTemperatureLogic();
    }
  }

  if (digitalRead(IR_PIN) == LOW && (currentMillis - lastIRTrigger > 2000)) {
    lastIRTrigger = currentMillis;
    triggerACButton(); 
  }

  bool leftState = !digitalRead(LEFT_BTN);  
  bool rightState = !digitalRead(RIGHT_BTN);

  if (leftState || rightState) {
    if (btnPressTime == 0) {
      btnPressTime = currentMillis;
      actionDone = false;
    }
    
    if (currentMillis - btnPressTime > 1000 && !actionDone) {
      if (leftState && rightState) {
        backlightOn = !backlightOn;
        if (backlightOn) lcd.backlight();
        else lcd.noBacklight();
        saveSettings();
        beepConfirm();
        actionDone = true;
      } 
      else if (leftState || rightState) {
        saveSettings();
        beepConfirm();
        actionDone = true;
      }
    }
  } 
  else {
    btnPressTime = 0;
  }
  
  static bool lastLeft = false;
  static bool lastRight = false;
  
  if (leftState && !lastLeft) {
    delay(50); 
    if (!rightState) { targetTemp--; updateDisplay(); beep(50); }
  }
  if (rightState && !lastRight) {
    delay(50); 
    if (!leftState) { targetTemp++; updateDisplay(); beep(50); }
  }
  
  lastLeft = leftState;
  lastRight = rightState;
}

void updateDisplay() {
  lcd.setCursor(0, 0);
  lcd.print("Target Temp: ");
  lcd.print(targetTemp);
  lcd.print("C  "); 

  lcd.setCursor(0, 1);
  lcd.print("Room Temp:   ");
  lcd.print(currentTemp);
  lcd.print("C  ");
}

void checkTemperatureLogic() {
  if (currentTemp <= targetTemp && acIsOn == true) {
    triggerACButton();
    acIsOn = false;
  }
  else if (currentTemp >= (targetTemp + 1) && acIsOn == false) {
    triggerACButton();
    acIsOn = true;
  }
}

void triggerACButton() {
  beep(200); 
  acServo.attach(SERVO_PIN);
  acServo.write(90);  
  delay(400);
  acServo.write(0);   
  delay(400);
  acServo.detach();
