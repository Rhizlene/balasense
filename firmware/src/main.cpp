/*
 * Test ICM-20948 - Capteur IMU 9 axes
 * Projet BalaSense - Projet personnel
 * 
 * Mesure accélération, rotation et orientation
 * Pins I2C : GPIO 25 (SDA) et GPIO 26 (SCL)
 * 
 * Auteur : Rhiz
 * Date : Janvier 2026
 */

#include <Arduino.h>
#include <Wire.h>
#include "ICM_20948.h"

#define I2C_SDA 25
#define I2C_SCL 26

ICM_20948_I2C imu;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Test ICM-20948 - IMU 9 axes ===");
  Serial.println("=== Projet BalaSense ===\n");
  
  Wire.begin(I2C_SDA, I2C_SCL);
  
  Serial.print("Initialisation ICM-20948...");
  
  bool initialized = false;
  
  // Tentative avec adresse 0x68
  imu.begin(Wire, 0);
  
  if (imu.status != ICM_20948_Stat_Ok) {
    Serial.println(" ÉCHEC !");
    Serial.println("\nVérifiez le câblage :");
    Serial.println("- VIN → 3.3V");
    Serial.println("- GND → GND");
    Serial.println("- SDA → GPIO 25");
    Serial.println("- SCL → GPIO 26");
    Serial.println("\nAssurez-vous que les pins sont bien enfoncés !");
    while (1);
  }
  
  Serial.println(" OK !\n");
  
  Serial.println("=== Configuration ===");
  Serial.println("Accéléromètre : ±16g");
  Serial.println("Gyroscope : ±2000 dps");
  Serial.println("Magnétomètre : activé\n");
  
  delay(2000);
}

void loop() {
  // Lecture des données
  if (imu.dataReady()) {
    imu.getAGMT();  // Accéléromètre + Gyro + Magnétomètre + Température
    
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    // ACCÉLÉROMÈTRE (G-force)
    Serial.println("ACCÉLÉROMÈTRE (G-force) :");
    Serial.print("  X: ");
    Serial.print(imu.accX() / 1000.0, 2);
    Serial.print(" g  |  Y: ");
    Serial.print(imu.accY() / 1000.0, 2);
    Serial.print(" g  |  Z: ");
    Serial.print(imu.accZ() / 1000.0, 2);
    Serial.println(" g");
    
    // Magnitude totale (force G totale)
    float totalG = sqrt(
      pow(imu.accX() / 1000.0, 2) +
      pow(imu.accY() / 1000.0, 2) +
      pow(imu.accZ() / 1000.0, 2)
    );
    Serial.print("  Total G-force: ");
    Serial.print(totalG, 2);
    Serial.println(" g");
    
    // GYROSCOPE (rotation)
    Serial.println("\nGYROSCOPE (rotation) :");
    Serial.print("  X: ");
    Serial.print(imu.gyrX(), 1);
    Serial.print(" °/s  |  Y: ");
    Serial.print(imu.gyrY(), 1);
    Serial.print(" °/s  |  Z: ");
    Serial.print(imu.gyrZ(), 1);
    Serial.println(" °/s");
    
    // MAGNÉTOMÈTRE (orientation)
    Serial.println("\nMAGNÉTOMÈTRE (champ magnétique) :");
    Serial.print("  X: ");
    Serial.print(imu.magX(), 1);
    Serial.print(" µT  |  Y: ");
    Serial.print(imu.magY(), 1);
    Serial.print(" µT  |  Z: ");
    Serial.print(imu.magZ(), 1);
    Serial.println(" µT");
    
    // TEMPÉRATURE
    Serial.println("\nTEMPÉRATURE :");
    Serial.print("  ");
    Serial.print(imu.temp(), 1);
    Serial.println(" °C");
    
    // ANALYSE MOUVEMENT
    Serial.println("\nANALYSE :");
    
    // Détection mouvement brusque
    if (totalG > 1.5) {
      Serial.println("  MOUVEMENT BRUSQUE détecté !");
    } else if (totalG < 0.5) {
      Serial.println("  Chute libre / Capteur non stable");
    } else {
      Serial.println("  Capteur stable (gravité terrestre ~1g)");
    }
    
    // Détection rotation
    float totalRotation = sqrt(
      pow(imu.gyrX(), 2) +
      pow(imu.gyrY(), 2) +
      pow(imu.gyrZ(), 2)
    );
    
    if (totalRotation > 50) {
      Serial.println("  ROTATION RAPIDE détectée !");
    } else if (totalRotation > 10) {
      Serial.println("  Rotation modérée");
    } else {
      Serial.println("  Pas de rotation significative");
    }
    
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
  } else {
    Serial.println("En attente de données...");
  }
  
  delay(500);  // Mesure toutes les 500ms (2Hz)
}