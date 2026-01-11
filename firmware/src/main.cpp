/*
 * Test MAX30102 - Capteur rythme cardiaque
 * Projet BalaSense - Version optimisée
 */

#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

#define I2C_SDA 25
#define I2C_SCL 26

MAX30105 particleSensor;

const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;

float beatsPerMinute;
int beatAvg;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Test MAX30102 - BalaSense ===\n");
  
  Wire.begin(I2C_SDA, I2C_SCL);
  
  Serial.print("Initialisation MAX30102...");
  
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println(" ÉCHEC !");
    Serial.println("Vérifiez que les pins sont bien enfoncés !");
    while (1);
  }
  
  Serial.println(" OK ! ✅\n");
  
  // Configuration optimisée
  byte ledBrightness = 30;   // Réduit à 30 (était 60)
  byte sampleAverage = 4;
  byte ledMode = 2;
  byte sampleRate = 100;
  int pulseWidth = 411;
  int adcRange = 4096;
  
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeIR(0x30);  // Réduit à 0x30 (était 0x50)
  
  Serial.println("=== Instructions ===");
  Serial.println("1. Maintenez les pins ESP32 bien enfoncés");
  Serial.println("2. Posez DÉLICATEMENT votre doigt sur le capteur");
  Serial.println("3. Pression légère (pas trop fort !)");
  Serial.println("4. Ne bougez plus pendant 15 secondes");
  Serial.println();
  
  delay(3000);
}

void loop() {
  long irValue = particleSensor.getIR();
  
  // Détection saturation
  if (irValue >= 260000) {
    Serial.println("⚠️ SATURATION ! Appuyez moins fort sur le capteur");
    delay(1000);
    return;
  }
  
  // Pas de contact
  if (irValue < 50000) {
    Serial.print("Pas de contact (IR: ");
    Serial.print(irValue);
    Serial.println(") - Posez votre doigt");
    delay(500);
    return;
  }
  
  // Contact OK !
  if (checkForBeat(irValue) == true) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    
    beatsPerMinute = 60 / (delta / 1000.0);
    
    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;
      
      beatAvg = 0;
      for (byte x = 0; x < RATE_SIZE; x++) {
        beatAvg += rates[x];
      }
      beatAvg /= RATE_SIZE;
    }
    
    Serial.print("💓 BATTEMENT ! | IR=");
    Serial.print(irValue);
    Serial.print(" | BPM=");
    Serial.print(beatsPerMinute, 1);
    Serial.print(" | Moy=");
    Serial.print(beatAvg);
    Serial.println(" BPM validé");
  }
  
  // Affichage périodique
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000) {
    Serial.print("Signal: IR=");
    Serial.print(irValue);
    
    if (beatAvg > 0) {
      Serial.print(" | BPM moyen: ");
      Serial.print(beatAvg);
    }
    
    // Indicateur qualité signal
    if (irValue > 100000) {
      Serial.print(" | 🟢 Signal EXCELLENT");
    } else if (irValue > 70000) {
      Serial.print(" | 🟡 Signal BON");
    } else {
      Serial.print(" | 🟠 Signal FAIBLE");
    }
    
    Serial.println();
    lastPrint = millis();
  }
}