//
// Created by Joel Neumann on 14.07.25.
//

#include "LSM6DS3.h"
#include <Arduino.h>

//Create a instance of class LSM6DS3
LSM6DS3 myIMU(I2C_MODE, 0x6A);  //I2C device address 0x6A

#define CONVERT_G_TO_MS2 9.80665f
#define FREQUENCY_HZ 50
#define INTERVAL_MS (1000 / (FREQUENCY_HZ + 1))

static unsigned long last_interval_ms = 0;


void setup() {
    Serial.begin(115200);
    while (!Serial)
        ;

    if (myIMU.begin() != 0) {
        Serial.println("Device error");
    } else {
        Serial.println("Device OK!");
    }
}



void loop() {
    if (millis() > last_interval_ms + INTERVAL_MS) {
        last_interval_ms = millis();
        Serial.print(myIMU.readFloatAccelX() * CONVERT_G_TO_MS2, 4);
        Serial.print('\t');
        Serial.print(myIMU.readFloatAccelY() * CONVERT_G_TO_MS2, 4);
        Serial.print('\t');
        Serial.print(myIMU.readFloatAccelZ() * CONVERT_G_TO_MS2, 4);
        Serial.print('\t');
        Serial.print(myIMU.readFloatGyroX(), 4);
        Serial.print('\t');
        Serial.print(myIMU.readFloatGyroY(), 4);
        Serial.print('\t');
        Serial.print(myIMU.readFloatGyroZ(), 4);
        Serial.println();
    }
}

