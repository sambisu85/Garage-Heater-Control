#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>


#define sensorPin 2
#define limitSwitchPin 11
#define buttonPin1 8
#define buttonPin2 9
#define relayPin 5
#define ledPin 12

DHT dht(sensorPin, DHT11);
LiquidCrystal_I2C lcd(0x27, 16, 2);

//float temperatureC = 0;
float temperatureF = 0;
unsigned long lastReading = 0;
int sensorInterval = 2000;
float setPoint = 55;
float hysteresis = 5; // degrees of hysteresis around setpoint
bool heaterStatus = false; // false = off, true = on
bool limitSwitchStatus = LOW; // LOW = door open, HIGH = door closed
bool lastLimitSwitchStatus = HIGH;
bool buttonState1 = HIGH; // HIGH = NOT PRESSED
bool lastButtonState1 = HIGH;
bool buttonState2 = HIGH;
bool lastButtonState2 = HIGH;
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers
unsigned long lastMillis = 0;
unsigned long pageTimeout = 5000;
int screenPage = 0; // 0 is main screen, 1 is menu, 2 is door open splash, 3 is hysteresis edit page
int lastScreenPage = 0;
// Set the menu options
String menuOptions[] = {"Back", "Htr Enbl/Disbl", "Edit Hyst", "Fan Enbl/Disbl", "LS Enbl/Disbl", " "};
// Set the number of menu options
//int numMenuOptions = sizeof(menuOptions) / sizeof(menuOptions[0]);
// Set the starting index for the menu
int menuIndex = 0;
bool limitSwitchEnabled = true;
bool heaterEnabled = true;
bool fanEnabled = true;
int heaterMinTimeOn = 300000;  // minimum time heater will run when cycled on
int heaterMinTimeOff = 300000; // minimum time heater will remain off when cycled off
int heaterTimeOn = 0;  // used to track time heater has been on
int heaterTimeOff = 0; // used to track time heater has been off


void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.clear();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("      Briv");
  lcd.setCursor(0,1);
  lcd.print("  Technologies");
  delay(2000);
  lcd.clear();
  configureDisplay();
  lcd.print("Off");
  dht.begin();

  pinMode(sensorPin, INPUT);
  pinMode(limitSwitchPin, INPUT_PULLUP);
  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(relayPin, OUTPUT);
}

void loop() {

  // makes sure heater is off if disabled
  if (heaterEnabled == false)
  {disableHeat();}
  
  // door limit switch functions
  if (limitSwitchEnabled == true) 
  {limitSwitch();}
  
  // Get sensor reading at defined interval
  if (millis() - lastReading > sensorInterval) 
  {
    readSensor();
    
    // only print to screen if on main screen
    if (screenPage == 0)
    {
      lcd.setCursor(4,0);
      lcd.print(temperatureF,1);
      lcd.print(" F ");
      lcd.setCursor(4,1);
      lcd.print(setPoint,1);
      lcd.print(" F ");
    }

    // ON CONTROL
    if (temperatureF < (setPoint - hysteresis/2)     // temperature below setpoint minus half of hysteresis
    && heaterStatus == false                         // heater is off
    && heaterEnabled == true                         // heater is enabled
    && millis() - heaterTimeOff > heaterMinTimeOff)  // heater has been off for at least the minimum off time
    {
      enableHeat();
    }
    // OFF CONTROL
    else if (temperatureF > (setPoint + hysteresis/2) // temperature is above setpoint plus half of hysteresis
    && heaterStatus == true                           // heater is on
    && millis() - heaterTimeOn > heaterMinTimeOn)     // heater has been on for at least the minimum on time 
    {
      disableHeat();
    }
  }

  // MAIN SCREEN BUTTON FUNCTIONS
  if (screenPage == 0)
  {
    buttonState1 = digitalRead(buttonPin1);
    buttonState2 = digitalRead(buttonPin2);
    if (buttonState1 == LOW && buttonState2 == LOW)  // Both buttons pressed - accesses menu
    {
      delay(debounceDelay);
      if (buttonState1 == LOW && buttonState2 == LOW)
      {
        menu();
      }
    }
    else if (buttonState1 == LOW && buttonState2 == HIGH) // UP button only
    {
      delay(debounceDelay);
      if (buttonState1 == LOW && buttonState2 == HIGH)  
      {
        setPoint = setPoint + 1;
        lcd.setCursor(4,1);
        lcd.print(setPoint);
        lcd.print(" F ");
        delay(500);
      }
    }
    else if (buttonState1 == HIGH && buttonState2 == LOW)  // DOWN button only
    {
      delay(debounceDelay);
      if (buttonState2 == LOW) 
      {
        setPoint = setPoint - 1;
        lcd.setCursor(4,1);
        lcd.print(setPoint);
        lcd.print(" F ");
        delay(500);
      }
    }
  }
  else if (screenPage == 1)
  {
    menu();
  }
  else if (screenPage ==3)  // edit hysteresis page
  {
    if (millis() - lastMillis > pageTimeout)
    {
      configureDisplay();
    }    
   else 
   {
     buttonState1 = digitalRead(buttonPin1);
      buttonState2 = digitalRead(buttonPin2);
      if (buttonState1 == LOW && buttonState2 == HIGH) // UP button only
      {
        delay(debounceDelay);
        if (buttonState1 == LOW && buttonState2 == HIGH)  
        {
          hysteresis = hysteresis + 0.5;
          lcd.setCursor(0, 1);
          lcd.print(hysteresis,1);
          lcd.print (" degrees");
          delay(500);
          lastMillis = millis();
        }
      }
      else if (buttonState1 == HIGH && buttonState2 == LOW) // DOWN button only
      {
        delay(debounceDelay);
        if (buttonState1 == HIGH && buttonState2 == LOW)  
        {
          hysteresis = hysteresis - 0.5;
          lcd.setCursor(0, 1);
          lcd.print(hysteresis,1);
          lcd.print (" degrees");
          delay(500);
          lastMillis = millis();
        }
      }
    }
  }
}

void readSensor() {
  // Get a reading from the temperature sensor:
  //temperatureC = dht.readTemperature();
  temperatureF = dht.readTemperature(true);
  lastReading = millis();
}

void enableHeat() {
  if (heaterEnabled == true)
  {
    heaterStatus = true;
    heaterTimeOn = millis(); // set TimeOn to current time
    digitalWrite(relayPin, HIGH);
    digitalWrite(ledPin, HIGH);
    if (screenPage == 0) // only print when on main screen
    {
      lcd.setCursor(13,0);
      lcd.print(" On");
    }
  }
}

void disableHeat() {
  heaterStatus = false;
  heaterTimeOff = millis(); // set TimeOff to current time
  digitalWrite(relayPin, LOW);
  digitalWrite(ledPin, LOW);
  if (screenPage == 0 && heaterEnabled == true) // only print when on main screen
  {
    lcd.setCursor(13,0);
    lcd.print("Off");
  }
  else if (screenPage == 0 && heaterEnabled == false)
  {
    lcd.setCursor(13,0);
    lcd.print("Dis");
  }
}

void configureDisplay() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("AV: ");
  lcd.setCursor(0,1);
  lcd.print("SP: ");
  screenPage = 0;
  lastScreenPage = 0;
  if (heaterStatus == false && heaterEnabled == true) 
  {
    lcd.setCursor(13,0);
    lcd.print("Off");
  } 
  else if (heaterStatus == true && heaterEnabled == true)
  {
    lcd.setCursor(13,0);
    lcd.print(" On");
  }
  else if (heaterEnabled == false)
  {
    lcd.setCursor(13,0);
    lcd.print("Dis");
  }
}

void limitSwitch() {
  limitSwitchStatus = digitalRead(limitSwitchPin);
  if (limitSwitchStatus != lastLimitSwitchStatus)
  {
    delay(500);
    limitSwitchStatus = digitalRead(limitSwitchPin);
    if (limitSwitchStatus == LOW)
    {
      disableHeat();
      heaterEnabled = false;
      lastLimitSwitchStatus = LOW;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("DOOR OPEN");
      lcd.setCursor(0,1);
      lcd.print("HEATER DISABLED");
      screenPage = 2;
    }
    else if (limitSwitchStatus == HIGH)
    {
      heaterEnabled = true;
      lastLimitSwitchStatus = HIGH;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("DOOR CLOSED");
      lcd.setCursor(0,1);
      lcd.print("HEATER ENABLED");
      delay(2000);
      configureDisplay();
    }
  }
}

void menu() {
  // ENTER MENU
  screenPage = 1;
  if (lastScreenPage != 1)
  {
    menuIndex = 0;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(menuOptions[menuIndex]);
    lcd.print(" <");
    lcd.setCursor(0,1);
    lcd.print(menuOptions[menuIndex + 1]);
    lastScreenPage = 1;
  }
  delay(100);
  buttonState1 = digitalRead(buttonPin1);
  buttonState2 = digitalRead(buttonPin2);

  if (buttonState1 == LOW && buttonState2 == HIGH) // UP button only
  {
    delay(debounceDelay);
    if (buttonState1 == LOW && buttonState2 == HIGH)  
    {
      // Back option
      if (menuIndex == 0)
      {configureDisplay();}
      
      // Heater enable/disable option
      else if (menuIndex == 1)
      {
        if (heaterEnabled == true)
        {
          heaterEnabled = false;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("HEATER DISABLED");
          delay(1000);
          configureDisplay();          
        }
        else
        {          
          heaterEnabled = true;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("HEATER ENABLED");
          delay(1000);
          configureDisplay();}
      }

      // Edit hysteresis option
      else if (menuIndex == 2)
      {
        screenPage = 3;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Hysteresis value:");
        lcd.setCursor(0, 1);
        lcd.print(hysteresis,1);
        lcd.print(" degrees");
        delay(500);
        lastMillis = millis();
      }

      // Fan enable/disable
      else if (menuIndex == 3)
      {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Nothing here yet");
        delay(1000);
        configureDisplay();
      }

      // Limit switch enable/disable
      else if (menuIndex == 4)
      {
        if (limitSwitchEnabled == true)
        {
          limitSwitchEnabled = false;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("LS DISABLED");
          delay(1000);
          configureDisplay();          
        }
        else
        {          
          limitSwitchEnabled = true;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("LS ENABLED");
          delay(1000);
          configureDisplay();
          }
      }
      
    }
  }
  else if (buttonState1 == HIGH && buttonState2 == LOW) // DOWN button only
  {
    delay(debounceDelay);
    if (buttonState1 == HIGH && buttonState2 == LOW)
    {
      // Menu scrolling
      if (menuIndex <= 3)
      {menuIndex = menuIndex + 1;}
      else if (menuIndex == 4)
      {menuIndex = 0;}
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(menuOptions[menuIndex]);
      lcd.print(" <");
      lcd.setCursor(0,1);
      lcd.print(menuOptions[menuIndex + 1]);
      Serial.println(menuIndex);
    }
  }
}
