/*
 * Test Multi-Capteurs I2C - BalaSense
 * MAX30102 + MLX90614 + ICM-20948 simultanément
 * 
 * Bus I2C partagé : GPIO 25 (SDA), GPIO 26 (SCL)
 * Projet personnel - Rhiz - 2026
 */

#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <Adafruit_MLX90614.h>
#include "ICM_20948.h"

#define I2C_SDA 25
#define I2C_SCL 26

// Instances capteurs
MAX30105 heartSensor;
Adafruit_MLX90614 tempSensor = Adafruit_MLX90614();
ICM_20948_I2C imuSensor;

// Variables MAX30102
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("   BALASENSE - TEST MULTI-CAPTEURS");
  Serial.println("   3 capteurs I2C sur breadboard");
  Serial.println("========================================\n");
  
  Wire.begin(I2C_SDA, I2C_SCL);
  
  // ========== INITIALISATION MAX30102 ==========
  Serial.print("MAX30102 (0x57)... ");
  if (!heartSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("ECHEC");
  } else {
    Serial.println("OK");
    heartSensor.setup(30, 4, 2, 100, 411, 4096);
    heartSensor.setPulseAmplitudeRed(0x0A);
    heartSensor.setPulseAmplitudeIR(0x30);
  }
  
  // ========== INITIALISATION MLX90614 ==========
  Serial.print("MLX90614 (0x5A)... ");
  if (!tempSensor.begin()) {
    Serial.println("ECHEC");
  } else {
    Serial.println("OK");
  }
  
  // ========== INITIALISATION ICM-20948 ==========
  Serial.print("ICM-20948 (0x68)... ");
  imuSensor.begin(Wire, 0);
  if (imuSensor.status != ICM_20948_Stat_Ok) {
    Serial.println("ECHEC");
  } else {
    Serial.println("OK");
  }
  
  Serial.println("\n========================================");
  Serial.println("Instructions :");
  Serial.println("- Posez votre doigt sur MAX30102");
  Serial.println("- Pointez MLX90614 vers votre front");
  Serial.println("- Bougez le breadboard pour tester IMU");
  Serial.println("========================================\n");
  
  delay(3000);
}

void loop() {
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println("LECTURE MULTI-CAPTEURS");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  
  // ========== MAX30102 - RYTHME CARDIAQUE ==========
  Serial.println("--- MAX30102 (Rythme cardiaque) ---");
  long irValue = heartSensor.getIR();
  
  if (irValue >= 260000) {
    Serial.println("SATURATION ! Appuyez moins fort");
  } else if (irValue < 50000) {
    Serial.print("Pas de contact (IR: ");
    Serial.print(irValue);
    Serial.println(")");
    Serial.println("Posez LEGEREMENT votre doigt sur le capteur");
    beatAvg = 0;
  } else {
    // Contact OK, détection battement
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
    }
    
    Serial.print("Signal IR: ");
    Serial.print(irValue);
    
    if (beatAvg > 0) {
      Serial.print(" | BPM moyen: ");
      Serial.print(beatAvg);
      Serial.print(" bpm");
    }
    
    // Qualité signal
    if (irValue > 100000) {
      Serial.println(" | EXCELLENT");
    } else if (irValue > 70000) {
      Serial.println(" | BON");
    } else {
      Serial.println(" | FAIBLE");
    }
  }
  
  // ========== MLX90614 - TEMPERATURE ==========
  Serial.println("\n--- MLX90614 (Temperature IR) ---");
  float ambientTemp = tempSensor.readAmbientTempC();
  float objectTemp = tempSensor.readObjectTempC();
  
  Serial.print("Ambiante: ");
  Serial.print(ambientTemp, 1);
  Serial.println(" C");
  
  Serial.print("Objet: ");
  Serial.print(objectTemp, 1);
  Serial.print(" C");
  
  if (objectTemp > 34.0 && objectTemp < 38.0) {
    Serial.println(" [NORMAL]");
  } else if (objectTemp >= 38.0) {
    Serial.println(" [FIEVRE]");
  } else if (objectTemp < 25.0) {
    Serial.println(" [Pointez vers votre main/front]");
  } else {
    Serial.println();
  }
  
  // ========== ICM-20948 - IMU 9 AXES ==========
  Serial.println("\n--- ICM-20948 (Accelerometre + Gyroscope) ---");
  if (imuSensor.dataReady()) {
    imuSensor.getAGMT();
    
    // Accéléromètre
    float totalG = sqrt(
      pow(imuSensor.accX() / 1000.0, 2) +
      pow(imuSensor.accY() / 1000.0, 2) +
      pow(imuSensor.accZ() / 1000.0, 2)
    );
    
    Serial.print("G-force: ");
    Serial.print(totalG, 2);
    Serial.print(" g");
    
    if (totalG > 1.5) {
      Serial.print(" [MOUVEMENT BRUSQUE]");
    } else if (totalG < 0.5) {
      Serial.print(" [CHUTE LIBRE]");
    } else {
      Serial.print(" [STABLE]");
    }
    Serial.println();
    
    // Gyroscope
    float totalRotation = sqrt(
      pow(imuSensor.gyrX(), 2) +
      pow(imuSensor.gyrY(), 2) +
      pow(imuSensor.gyrZ(), 2)
    );
    
    Serial.print("Rotation: ");
    Serial.print(totalRotation, 0);
    Serial.print(" deg/s");
    
    if (totalRotation > 50) {
      Serial.println(" [ROTATION RAPIDE]");
    } else if (totalRotation > 10) {
      Serial.println(" [ROTATION MODEREE]");
    } else {
      Serial.println(" [PAS DE ROTATION]");
    }
    
    // Magnétomètre
    Serial.print("Magnetometre: X=");
    Serial.print(imuSensor.magX(), 0);
    Serial.print(" Y=");
    Serial.print(imuSensor.magY(), 0);
    Serial.print(" Z=");
    Serial.print(imuSensor.magZ(), 0);
    Serial.println(" uT");
    
    // Température capteur
    Serial.print("Temp capteur: ");
    Serial.print(imuSensor.temp(), 1);
    Serial.println(" C");
  } else {
    Serial.println("En attente de donnees...");
  }
  
  Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  
  delay(2000); 
}