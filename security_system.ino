#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo doorServo;

// --- Pin Definitions ---
const int magneticSwitchPin = 2;  // Magnetic switch (between D2 and GND)
const int buzzerPin         = 3;  // Active Buzzer (+ to D3, - to GND)
const int pirPin            = 4;  // PIR Sensor
const int servoPin          = 5;  // Servo Motor
const int exteriorLightPin  = A0; // Exterior motion LED

// --- Keypad Setup ---
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {13, 12, 11, 10}; 
byte colPins[COLS] = {9, 8, 7, 6}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- Passcodes & States ---
String doorPasscode = "1234";
String secPasscode  = "9999";
String enteredCode  = "";
bool securityArmed  = true;

// --- Buzzer Beeping Timer Variables ---
unsigned long lastBeepTime = 0;
bool buzzerState = LOW;
const unsigned long BEEP_INTERVAL_MS = 200; // Speed of beeping (200ms ON, 200ms OFF)

// --- Improved PIR Hold Logic ---
bool lastPirState = LOW;
unsigned long pirWentLowTime = 0;
bool keepLightOn = false;
const unsigned long BLIND_GAP_BRIDGE_MS = 5000;

// Non-blocking timers for Door and LCD messages
bool doorIsOpen = false;
unsigned long doorOpenTime = 0;

bool showingTempMsg = false;
unsigned long tempMsgEndTime = 0;
String tempMsgL1 = "";
String tempMsgL2 = "";

String lastL1 = "";
String lastL2 = "";

void setup() {
  Serial.begin(9600);
  pinMode(magneticSwitchPin, INPUT_PULLUP);
  pinMode(pirPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(exteriorLightPin, OUTPUT);
  
  doorServo.attach(servoPin);
  doorServo.write(0);
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Security sys 1.0");
  lcd.setCursor(0, 1);
  lcd.print("System Ready");
  delay(1500);
  lcd.clear();
}

void loop() {
  // 1. Magnetic Switch & Beeping Active Buzzer
  bool windowOpen = (digitalRead(magneticSwitchPin) == HIGH);
  if (securityArmed && windowOpen) {
    // Toggle buzzer ON and OFF every 200ms without blocking the keypad
    if (millis() - lastBeepTime >= BEEP_INTERVAL_MS) {
      lastBeepTime = millis();
      buzzerState = !buzzerState;
      digitalWrite(buzzerPin, buzzerState);
    }
  } else {
    buzzerState = LOW;
    digitalWrite(buzzerPin, LOW); // Ensure buzzer is completely OFF when safe
  }

  // 2. PIR Motion Sensor -> Exterior Light
  bool currentPir = digitalRead(pirPin);

  if (currentPir != lastPirState) {
    Serial.print("PIR PIN changed to: ");
    Serial.println(currentPir == HIGH ? "HIGH (Motion)" : "LOW (Sensor Resetting)");
    if (currentPir == LOW) {
      pirWentLowTime = millis();
    }
    lastPirState = currentPir;
  }

  if (currentPir == HIGH) {
    keepLightOn = true;
  } else if (keepLightOn && (millis() - pirWentLowTime >= BLIND_GAP_BRIDGE_MS)) {
    keepLightOn = false;
  }

  if (securityArmed && keepLightOn) {
    digitalWrite(exteriorLightPin, HIGH);
  } else {
    digitalWrite(exteriorLightPin, LOW);
  }

  // 3. Non-Blocking Door Auto-Close Timer (3 seconds)
  if (doorIsOpen && (millis() - doorOpenTime >= 3000)) {
    doorServo.write(0);
    doorIsOpen = false;
    showTempMessage("Door Locked", "", 1000);
  }

  // 4. Keypad Logic
  char key = keypad.getKey();
  if (key) {
    if (key == '*') {
      enteredCode = "";
      showTempMessage("Code Cleared", "", 1000);
    } 
    else if (key == '#') {
      if (enteredCode == doorPasscode) {
        doorServo.write(90);
        doorIsOpen = true;
        doorOpenTime = millis();
        showTempMessage("Door Unlocked", "Opening...", 3000);
      } 
      else if (enteredCode == secPasscode) {
        securityArmed = !securityArmed;
        showTempMessage("Security Mode:", securityArmed ? "on" : "off", 1500);
      } 
      else {
        showTempMessage("Wrong Code!", "Try Again", 1500);
      }
      enteredCode = "";
    } 
    else {
      showingTempMsg = false;
      enteredCode += key;
    }
  }

  // 5. Live LCD Update
  if (showingTempMsg && millis() >= tempMsgEndTime) {
    showingTempMsg = false;
  }

  String l1 = "";
  String l2 = "";

  if (securityArmed && windowOpen) {
    l1 = "ALARM ACTIVE!";
    l2 = "Window Opened";
  } else if (showingTempMsg) {
    l1 = tempMsgL1;
    l2 = tempMsgL2;
  } else {
    l1 = securityArmed ? "Security: on" : "Security: off";
    if (enteredCode.length() > 0) {
      for (unsigned int i = 0; i < enteredCode.length() && i < 16; i++) {
        l2 += "*";
      }
    } else {
      l2 = doorIsOpen ? "Door: Open" : "Door: Locked";
    }
  }

  updateLCDNoFlicker(l1, l2);
}

void showTempMessage(String l1, String l2, unsigned long durationMs) {
  tempMsgL1 = l1;
  tempMsgL2 = l2;
  showingTempMsg = true;
  tempMsgEndTime = millis() + durationMs;
}

void updateLCDNoFlicker(String l1, String l2) {
  while (l1.length() < 16) l1 += " ";
  while (l2.length() < 16) l2 += " ";
  l1 = l1.substring(0, 16);
  l2 = l2.substring(0, 16);

  if (l1 != lastL1 || l2 != lastL2) {
    lcd.setCursor(0, 0);
    lcd.print(l1);
    lcd.setCursor(0, 1);
    lcd.print(l2);
    lastL1 = l1;
    lastL2 = l2;
  }
}