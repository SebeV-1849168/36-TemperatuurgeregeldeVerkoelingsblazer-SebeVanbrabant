//Libraries
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"

//Constants
#define STEP_IN1_PIN 12
#define STEP_IN2_PIN 13
#define STEP_IN3_PIN 15
#define STEP_IN4_PIN 2

#define BUTTON_PIN 32

#define POT_PIN 33

#define LED_PIN 17

#define TEMP_PIN 25

//Variables
TFT_eSPI tft = TFT_eSPI(); // Constructor for the TFT library
OneWire oneWireTempSense(TEMP_PIN); //Setup a oneWire instance to communicate with any OneWire device
DallasTemperature TempSensors(&oneWireTempSense); //Pass oneWire reference to DallasTemperature library

bool fanIsRunning = false;
int stepDelay = 2;

int tempSensorCount = 0;
float temperature = 0;
int tempSkipCount = 0;

bool manualMode = false;
bool localButtonPressed = false;

bool isWorking = false;

//Set up the input feeds
AdafruitIO_Feed *toggleFanSwitch = io.feed("toggle-fan-switch");
AdafruitIO_Feed *toggleManualModeSwitch = io.feed("toggle-manual-mode-switch");
AdafruitIO_Feed *displayTemperatureGauge = io.feed("display-temperature-gauge");
//Set up the calendar feeds
AdafruitIO_Feed *workdayStartedFeed = io.feed("workday-started-feed");
AdafruitIO_Feed *workdayEndedFeed = io.feed("workday-ended-feed");

void setup() {
  TempSensors.begin();
  
  Serial.begin(115200);
  while(!Serial);

  //Initialize TFT
  tft.init();
  tft.setRotation(1);

  //Connect to io.adafruit.com
  Serial.println("Connecting to Adafruit IO");
  printConnectingToAdafruit();
  io.connect();

  toggleFanSwitch->onMessage(handleToggleFanSwitch);
  toggleManualModeSwitch->onMessage(handleToggleManualModeSwitch);
  workdayStartedFeed->onMessage(handleWorkdayStartedFeed);
  workdayEndedFeed->onMessage(handleWorkdayEndedFeed);
  
  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(io.statusText());
  printConnectedToAdafruit();

  //Setup stepper motor
  pinMode(STEP_IN1_PIN, OUTPUT);
  pinMode(STEP_IN2_PIN, OUTPUT);
  pinMode(STEP_IN3_PIN, OUTPUT);
  pinMode(STEP_IN4_PIN, OUTPUT);

  //Reset stepper position
  digitalWrite(STEP_IN1_PIN , LOW);
  digitalWrite(STEP_IN2_PIN , LOW);
  digitalWrite(STEP_IN3_PIN , LOW);
  digitalWrite(STEP_IN4_PIN , LOW);

  //Setup button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, buttonHandler, FALLING);

  //Setup potmeter
  pinMode(POT_PIN, INPUT);

  //Setup LED
  pinMode(LED_PIN, OUTPUT);

  //Setup temperature sensor
  pinMode(TEMP_PIN, INPUT);
  tempSensorCount = TempSensors.getDeviceCount();

  toggleFanSwitch->save("OFF");
  toggleManualModeSwitch->save("OFF");

  toggleFanSwitch->get();
  toggleManualModeSwitch->get();
}

void loop() {
  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();
  
  //Get temperatures
  //Only do this once in a while because it takes time and so it doesn't overload Adafruit
  if (tempSkipCount % 100 == 0) {
    TempSensors.requestTemperatures();
    
    for (int i = 0; i < tempSensorCount; ++i) {
      temperature = TempSensors.getTempCByIndex(i); 
    }

    if ((isWorking || temperature > 25) && !manualMode) {
      fanIsRunning = true;
      toggleFanSwitch->save("ON");
    }

    displayTemperatureGauge->save(temperature);
    tempSkipCount = 0;
  }
  ++tempSkipCount;

  //Fan
  stepDelay = map(analogRead(POT_PIN), 0, 4095, 2, 10);
  
  if (fanIsRunning) {
    digitalWrite(LED_PIN, HIGH);
    
    digitalWrite(STEP_IN1_PIN, HIGH);
    digitalWrite(STEP_IN2_PIN, LOW);
    digitalWrite(STEP_IN3_PIN, LOW);
    digitalWrite(STEP_IN4_PIN, HIGH);
    delay(stepDelay);
    digitalWrite(STEP_IN1_PIN, HIGH);
    digitalWrite(STEP_IN2_PIN, HIGH);
    digitalWrite(STEP_IN3_PIN, LOW);
    digitalWrite(STEP_IN4_PIN, LOW);
    delay(stepDelay);
    digitalWrite(STEP_IN1_PIN, LOW);
    digitalWrite(STEP_IN2_PIN, HIGH);
    digitalWrite(STEP_IN3_PIN, HIGH);
    digitalWrite(STEP_IN4_PIN, LOW);
    delay(stepDelay);
    digitalWrite(STEP_IN1_PIN, LOW);
    digitalWrite(STEP_IN2_PIN, LOW);
    digitalWrite(STEP_IN3_PIN, HIGH);
    digitalWrite(STEP_IN4_PIN, HIGH);
    delay(stepDelay);
  }
  else {
    digitalWrite(LED_PIN, LOW);
  }

  //Handle button press, saving changes to Adafruit doesn't work in the interupt
  if (localButtonPressed) {
    manualMode = true;
    toggleManualModeSwitch->save("ON");
    fanIsRunning = !fanIsRunning;
    
    if (fanIsRunning) {
      toggleFanSwitch->save("ON");
    }
    else {
      toggleFanSwitch->save("OFF");
    }
    localButtonPressed = false;
  }

  printFanStatus();
}


//Called on local button click
void buttonHandler() {
  localButtonPressed = true;
}

//Print before connecting to Adafruit
void printConnectingToAdafruit() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, 4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.println("Connecting to Adafruit IO");
}

//Print when connected to Adafruit
void printConnectedToAdafruit() {
  tft.setCursor(0, 50, 4);
  tft.print("Connected to Adafruit IO!");
}

//Print the status of the fan
void printFanStatus() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, 4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  
  if (fanIsRunning) {
    tft.print("Fan is running!");
    tft.setCursor(0, 50, 4);
    tft.print("Fan delay: ");
    tft.println(stepDelay);
  }
  else {
    tft.print("Fan is not running!");
  }

  tft.setCursor(0, 100, 4);
  tft.print("Temperature: ");
  tft.println(temperature);
}

//Called when the Adafruit fan toggle is switched
void handleToggleFanSwitch(AdafruitIO_Data *data) {
  Serial.println("Toggle Fan Switch");
  Serial.println(data->toString());
  if (data->toString() == "ON") {
    fanIsRunning = true;
  }
  else {
    fanIsRunning = false;
  }
}

//Called when the Adafruit manual mode toggle is switched
void handleToggleManualModeSwitch(AdafruitIO_Data *data) {
  Serial.println("Toggle Manual Mode Switch");
  Serial.println(data->toString());
  if (data->toString() == "ON") {
    manualMode = true;
  }
  else {
    manualMode = false;
  }
}

//Called when IFTTT detects the start of work in the calendar
void handleWorkdayStartedFeed(AdafruitIO_Data *data) {
  Serial.println("Work day started");
  isWorking = true;
}

//Called when IFTTT detects the end of work in the calendar
void handleWorkdayEndedFeed(AdafruitIO_Data *data) {
  Serial.println("Work day ended");
  isWorking = false;
}
