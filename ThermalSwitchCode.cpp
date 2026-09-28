#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Servo.h>
#include <EEPROM.h>

// --- PIN DEFINITIONS ---
#define DHTPIN D5
#define DHTTYPE DHT11
#define SERVO_PIN D6
#define LEFT_BTN D3
#define RIGHT_BTN D7
#define BUZZER_PIN D4
#define IR_PIN D0

// --- OBJECT INITIALIZATION ---
LiquidCrystal_I2C lcd(0x27, 16, 2); // Change 0x27 to 0x3F if LCD doesn't show text
DHT dht(DHTPIN, DHTTYPE);
Servo acServo;

// --- SYSTEM VARIABLES ---
int targetTemp = 24;      
bool backlightOn = true;
bool acIsOn = true;       // Assumes AC is ON when car starts
int currentTemp = 0;

// --- TIMERS FOR LOGIC (Non-Blocking) ---
unsigned long lastDHTRead = 0;
unsigned long lastIRTrigger = 0;
unsigned long btnPressTime = 0;
bool leftHeld = false;
bool rightHeld = false;
bool bothHeld = false;
bool actionDone = false;  // Prevents multiple triggers on one hold

void setup() {
  // Initialize Pins
  pinMode(LEFT_BTN, INPUT_PULLUP);
  pinMode(RIGHT_BTN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(IR_PIN, INPUT); // IR modules usually have built-in pullups
  
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize Memory (EEPROM)
  EEPROM.begin(512);
  byte savedTemp = EEPROM.read(0);
  byte savedBacklight = EEPROM.read(1);
  
  // If memory isn't blank (255), load saved settings
  if (savedTemp != 255) {
    targetTemp = savedTemp;
    backlightOn = (savedBacklight == 1);
  }

  // Initialize Display
  lcd.init();
  if (backlightOn) lcd.backlight();
  else lcd.noBacklight();

  // Initialize Sensor
  dht.begin();

  // --- BOOT SEQUENCE (The OS Startup) ---
  // Screen 1
  lcd.setCursor(0, 0);
  lcd.print("   Automated    ");
  lcd.setCursor(0, 1);
  lcd.print(" Thermal Switch ");
  beep(100);
  delay(1500);

  // Screen 2
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   By Abhirup   ");
  lcd.setCursor(0, 1);
  lcd.print(" For Dipta Sir  ");
  beep(100); delay(100); beep(100);
  delay(2000);
  
  lcd.clear();
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. READ TEMPERATURE (Every 2 seconds)
  if (currentMillis - lastDHTRead >= 2000) {
    lastDHTRead = currentMillis;
    int readT = dht.readTemperature();
    if (!isnan(readT)) {
      currentTemp = readT;
      updateDisplay();
      checkTemperatureLogic();
    }
  }

  // 2. CHECK IR SENSOR (Manual Override)
  // IR sensors usually output LOW when an object is detected
  if (digitalRead(IR_PIN) == LOW && (currentMillis - lastIRTrigger > 2000)) {
    lastIRTrigger = currentMillis;
    triggerACButton(); // Manually press button
  }

  // 3. CHECK BUTTONS (Hold and Click Logic)
  bool leftState = !digitalRead(LEFT_BTN);  // Inverted because of PULLUP
  bool rightState = !digitalRead(RIGHT_BTN);

  if (leftState || rightState) {
    if (btnPressTime == 0) {
      btnPressTime = currentMillis;
      actionDone = false;
    }
    
    // Check for holds
    if (currentMillis - btnPressTime > 1000 && !actionDone) {
      if (leftState && rightState) {
        // HOLD BOTH: Toggle Backlight
        backlightOn = !backlightOn;
        if (backlightOn) lcd.backlight();
        else lcd.noBacklight();
        saveSettings();
        beepConfirm();
        actionDone = true;
      } 
      else if (leftState || rightState) {
        // HOLD ONE: Save Target Temp
        saveSettings();
        beepConfirm();
        actionDone = true;
      }
    }
  } 
  else {
    // Buttons Released
    if (btnPressTime > 0 && !actionDone) {
      // It was a short click
      if (leftState == false && rightState == false) {
        unsigned long pressDuration = currentMillis - btnPressTime;
        if (pressDuration > 50 && pressDuration < 1000) { 
          // Previous state check required to see which was pressed
          // Simplified: We rely on the moment right before release, 
          // but better logic is checking which was active right before release.
        }
      }
    }
    // Re-written Short Click Logic for accuracy
    btnPressTime = 0;
  }
  
  // Dedicated Short Click Check
  static bool lastLeft = false;
  static bool lastRight = false;
  
  if (leftState && !lastLeft) {
    delay(50); // debounce
    if (!rightState) { targetTemp--; updateDisplay(); beep(50); }
  }
  if (rightState && !lastRight) {
    delay(50); // debounce
    if (!leftState) { targetTemp++; updateDisplay(); beep(50); }
  }
  
  lastLeft = leftState;
  lastRight = rightState;
}

// --- HELPER FUNCTIONS ---

void updateDisplay() {
  lcd.setCursor(0, 0);
  lcd.print("Target Temp: ");
  lcd.print(targetTemp);
  lcd.print("C  "); // Extra spaces to clear old artifacts

  lcd.setCursor(0, 1);
  lcd.print("Room Temp:   ");
  lcd.print(currentTemp);
  lcd.print("C  ");
}

void checkTemperatureLogic() {
  // If room gets too cold, turn AC OFF
  if (currentTemp <= targetTemp && acIsOn == true) {
    triggerACButton();
    acIsOn = false;
  }
  // If room gets hot, turn AC ON (added +1 degree buffer so it doesn't constantly toggle)
  else if (currentTemp >= (targetTemp + 1) && acIsOn == false) {
    triggerACButton();
    acIsOn = true;
  }
}

void triggerACButton() {
  beep(200); // Warning beep before movement
  acServo.attach(SERVO_PIN);
  acServo.write(90);  // The "Push" angle (Adjust this physically)
  delay(400);
  acServo.write(0);   // The "Rest" angle (Adjust this physically)
  delay(400);
  acServo.detach();   // Power down servo to keep it silent
}

void saveSettings() {
  EEPROM.write(0, targetTemp);
  EEPROM.write(1, backlightOn ? 1 : 0);
  EEPROM.commit();
}

void beep(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

void beepConfirm() {
  beep(100); delay(100); beep(100); delay(100); beep(300);
}