#ifndef SENSOR_BNO_H
#define SENSOR_BNO_H
#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

struct EulerAngles { float yaw, roll, pitch; };

bool BNO_begin(uint8_t address = 0x28, int32_t id = 55);
EulerAngles BNO_readEuler();
void BNO_useExtCrystal(bool use);

#endif // SENSOR_BNO_H
