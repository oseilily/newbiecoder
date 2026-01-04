#include <SPI.h>
#include <max6675.h>


// --- Pins ---
const int soPin = 12;   // MAX6675 SO
const int csPin = 10;   // MAX6675 CS
const int sckPin = 13;  // MAX6675 SCK


const int stepPin = 9;  // A4988 STEP
const int dirPin  = 8;  // A4988 DIR


MAX6675 thermocouple(sckPin, csPin, soPin);


// --- Adjustable settings ---
const int cutoffTemp = 80;   // °C - motor turns ON above this
const int hysteresis = 5;    // °C - motor turns OFF below (cutoff - hysteresis)


// --- Motor speed (pulse delay in microseconds) ---
int slowSpeed   = 4000; // gentle stir (~125 steps/sec)
int mediumSpeed = 2000; // moderate (~250 steps/sec)
int fastSpeed   = 1000; // strong (~500 steps/sec)
int maxSpeed    = 500;  // very strong (~1000 steps/sec)


// --- Temp averaging ---
const int numReadings = 10;
float tempReadings[numReadings];
int readIndex = 0;
float total = 0;
float averageTemp = 0;


// Timing
unsigned long lastTempRead = 0;
const unsigned long tempInterval = 1000; // 1s per temp read


// Motor stepping
volatile long currentPulseDelay = 0; // 0 = motor off
unsigned long lastStepUs = 0;


// State tracking for hysteresis
bool motorEnabled = false;


void setup() {
  Serial.begin(9600);


  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);


  digitalWrite(dirPin, HIGH); // Set motor direction


  // Init smoothing buffer
  for (int i = 0; i < numReadings; i++) {
    tempReadings[i] = 25; // assume room temp start
    total += tempReadings[i];
  }
}


void loop() {
  // 1) Every 1s, read thermocouple and update average
  if (millis() - lastTempRead >= tempInterval) {
    lastTempRead = millis();


    float t = thermocouple.readCelsius();
    if (!isnan(t)) {
      total -= tempReadings[readIndex];
      tempReadings[readIndex] = t;
      total += t;
      readIndex = (readIndex + 1) % numReadings;


      averageTemp = total / numReadings;


      Serial.print("Avg Temp (10s): ");
      Serial.print(averageTemp);
      Serial.println(" °C");


      // 2) Apply hysteresis logic
      if (!motorEnabled && averageTemp >= cutoffTemp) {
        motorEnabled = true;  // Turn ON above cutoff
        Serial.println("Motor ENABLED");
      } 
      else if (motorEnabled && averageTemp <= (cutoffTemp - hysteresis)) {
        motorEnabled = false; // Turn OFF below cutoff-hysteresis
        Serial.println("Motor DISABLED");
      }


      // 3) Decide motor speed only if enabled
      if (motorEnabled) {
        if (averageTemp < 100) {
          currentPulseDelay = slowSpeed; // gentle stir
        } 
        else if (averageTemp < 130) {
          currentPulseDelay = mediumSpeed; // moderate stir
        } 
        else if (averageTemp < 160) {
          currentPulseDelay = fastSpeed; // vigorous stir
        } 
        else {
          currentPulseDelay = maxSpeed; // maximum stir
        }
      } else {
        currentPulseDelay = 0; // OFF
      }
    } else {
      Serial.println("Thermocouple error, keeping last state.");
    }
  }


  // 4) Step motor continuously at chosen speed
  if (motorEnabled && currentPulseDelay > 0) {
    unsigned long nowUs = micros();
    if (nowUs - lastStepUs >= (unsigned long)currentPulseDelay) {
      digitalWrite(stepPin, HIGH);
      delayMicroseconds(3);
      digitalWrite(stepPin, LOW);
      lastStepUs = nowUs;
    }
  }
  else {
    // Motor idle
    digitalWrite(stepPin, LOW);
  }
}