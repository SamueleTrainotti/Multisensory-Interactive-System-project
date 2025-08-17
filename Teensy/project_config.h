// AGGIUNTO NUOVO HEADER
#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

// ====== DEBUG CONTROL ======
#ifndef DEBUG_MODE
  #define DEBUG_MODE 0  // 0 = solo CSV pulito, 1 = debug completo
#endif

// ====== SELECT ACTIVE SENSOR ======
#ifndef ACTIVE_SENSOR
  #define ACTIVE_SENSOR 3   // 1 = BNO055, 2 = ADXL337, 3 = DUAL
#endif

#endif // PROJECT_CONFIG_H