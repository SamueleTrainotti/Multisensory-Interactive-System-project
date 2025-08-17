#ifndef SENSOR_ADXL_H
#define SENSOR_ADXL_H
#include <Arduino.h>
#include <math.h>
#include "sensor_bno.h" // for EulerAngles struct reuse

// --- Config logica/asse: puoi cambiare qui per ruotare/invertire ---
#define LOGICAL_X_AXIS 'Z'
#define LOGICAL_X_SIGN  -1
#define LOGICAL_Y_AXIS 'Y'
#define LOGICAL_Y_SIGN  1
#define LOGICAL_Z_AXIS 'X'
#define LOGICAL_Z_SIGN  -1

// Pin di collegamento (Teensy 3.6: A0/A1/A2)
#ifndef ADXL_PIN_X
  #define ADXL_PIN_X A0
#endif
#ifndef ADXL_PIN_Y
  #define ADXL_PIN_Y A1
#endif
#ifndef ADXL_PIN_Z
  #define ADXL_PIN_Z A2
#endif

bool ADXL_begin();
EulerAngles ADXL_readEuler(); // yaw = NAN (non disponibile)
void ADXL_printRawValues();
void ADXL_resetFilters();

#endif // SENSOR_ADXL_H
