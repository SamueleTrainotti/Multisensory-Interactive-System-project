#include "sensor_adxl.h"
#include "debug_utils.h"

// ==== Parametri semplificati ====
static const float SUPPLY_VOLTAGE = 3.3f;   // V
static const float SENSITIVITY = 0.33f;     // V/g (valore nominale ADXL337)
static const float ZERO_G_VOLTAGE = 1.65f;  // V (metà della tensione di alimentazione)

// Offset di calibrazione (calcolati durante l'inizializzazione)
static float offsetX = 0, offsetY = 0, offsetZ = 0; // in Volt

// ==== FILTRI E PROCESSING ====
// Filtro passa-basso esponenziale (EMA)
static const float ALPHA_ACCEL = 0.2f;  // Più basso = più smooth, più lento
static float filteredX = 0, filteredY = 0, filteredZ = 0;

// Filtro passa-basso per gli angoli
static const float ALPHA_ANGLES = 0.25f;
static float filteredPitch = 0, filteredRoll = 0;
static bool filtersInitialized = false;

// Filtro anti-rumore (dead zone)
static const float ACCEL_DEADZONE = 0.02f;  // g - ignora variazioni piccole

// Rilevamento movimento per adattare il filtraggio
static float prevX = 0, prevY = 0, prevZ = 0;
static bool isMoving = false;
static const float MOVEMENT_THRESHOLD = 0.1f;  // g/s

// Funzione helper per convertire lettura analogica in tensione
static inline float analogToVoltage(int analogValue) {
    return (analogValue * SUPPLY_VOLTAGE) / 1023.0f;
}

// Funzione helper per convertire tensione in accelerazione (g)
static inline float voltageToG(float voltage, float offset) {
    return (voltage - offset) / SENSITIVITY;
}

// Filtro passa-basso esponenziale
static inline float exponentialFilter(float previous, float current, float alpha) {
    return previous * (1.0f - alpha) + current * alpha;
}

// Filtro dead-zone per ridurre il rumore
static inline float deadZoneFilter(float value, float threshold) {
    if (fabs(value) < threshold) return 0.0f;
    return value;
}

// Funzione per rilevare movimento
static bool detectMovement(float x, float y, float z) {
    float deltaX = fabs(x - prevX);
    float deltaY = fabs(y - prevY); 
    float deltaZ = fabs(z - prevZ);
    
    float totalDelta = deltaX + deltaY + deltaZ;
    
    prevX = x; prevY = y; prevZ = z;
    
    return (totalDelta > MOVEMENT_THRESHOLD);
}

// Calcolo angoli con gestione singolarità
// static float calculatePitch(float x, float y, float z) {
//     // Evita divisione per zero e gestisce casi limite
//     float denominator = sqrt(y * y + z * z);
//     if (denominator < 0.01f) return 0.0f;  // Quasi verticale
    
//     float pitch = atan2(-x, denominator) * 180.0f / PI;
    
//     // Limita range
//     if (pitch > 90.0f) pitch = 90.0f;
//     if (pitch < -90.0f) pitch = -90.0f;
    
//     return pitch;
// }
float calculatePitch(float x, float y, float z) {
    return atan2(z, x) * 180.0f / PI;
}

static float calculateRoll(float x, float y, float z) {
    // Gestione singolarità per roll
    if (fabs(z) < 0.01f && fabs(y) < 0.01f) return 0.0f;
    
    float roll = atan2(y, z) * 180.0f / PI;
    
    return roll;  // Roll può andare da -180 a +180
}

// Funzione per applicare la mappatura logica degli assi
static void applyLogicalMapping(float rawX, float rawY, float rawZ,
                                float &outX, float &outY, float &outZ) {
    auto mapAxis = [&](char axis, int sign) -> float {
        switch(axis) {
            case 'X': return rawX * sign;
            case 'Y': return rawY * sign;
            case 'Z': return rawZ * sign;
            default:  return 0.0f;
        }
    };
    outX = mapAxis(LOGICAL_X_AXIS, LOGICAL_X_SIGN);
    outY = mapAxis(LOGICAL_Y_AXIS, LOGICAL_Y_SIGN);
    outZ = mapAxis(LOGICAL_Z_AXIS, LOGICAL_Z_SIGN);
}

bool ADXL_begin() {
    DEBUG_PRINTLN("=== Calibrazione ADXL337 ===");
    DEBUG_PRINTLN("IMPORTANTE: Mantieni la posizione iniziale! (per questo esercizio, X verso l'alto)");
    delay(3000); // Dai tempo di posizionarlo
    
    // Calibrazione: media di 100 letture a riposo
    const int SAMPLES = 100;
    long sumX = 0, sumY = 0, sumZ = 0;
    
    for (int i = 0; i < SAMPLES; i++) {
        sumX += analogRead(ADXL_PIN_X);
        sumY += analogRead(ADXL_PIN_Y);
        sumZ += analogRead(ADXL_PIN_Z);
        delay(5);
    }
    
    // Calcola tensioni medie
    float avgVoltX = analogToVoltage(sumX / SAMPLES);
    float avgVoltY = analogToVoltage(sumY / SAMPLES);
    float avgVoltZ = analogToVoltage(sumZ / SAMPLES);
    
    DEBUG_PRINT("Tensioni calibrazione - X: "); DEBUG_PRINT(avgVoltX, 3);
    DEBUG_PRINT("V, Y: "); DEBUG_PRINT(avgVoltY, 3);
    DEBUG_PRINT("V, Z: "); DEBUG_PRINT(avgVoltZ, 3); DEBUG_PRINTLN("V");
    
    // === FASE 1: Calcola accelerazioni FISICHE con offset nominali ===
    // Usa offset nominali temporanei per calcolare accelerazioni fisiche grezze
    float tempOffsetX = ZERO_G_VOLTAGE;  // 1.65V nominale
    float tempOffsetY = ZERO_G_VOLTAGE;  // 1.65V nominale  
    float tempOffsetZ = ZERO_G_VOLTAGE;  // 1.65V nominale
    
    float physicalX = voltageToG(avgVoltX, tempOffsetX);
    float physicalY = voltageToG(avgVoltY, tempOffsetY);
    float physicalZ = voltageToG(avgVoltZ, tempOffsetZ);
    
    DEBUG_PRINT("Accelerazioni fisiche grezze - X: "); DEBUG_PRINT(physicalX, 3);
    DEBUG_PRINT("g, Y: "); DEBUG_PRINT(physicalY, 3);
    DEBUG_PRINT("g, Z: "); DEBUG_PRINT(physicalZ, 3); DEBUG_PRINTLN("g");
    
    // === FASE 2: Applica mapping logico per ottenere valori target ===
    float logicalX, logicalY, logicalZ;
    applyLogicalMapping(physicalX, physicalY, physicalZ, logicalX, logicalY, logicalZ);
    
    DEBUG_PRINT("Accelerazioni logiche grezze - X: "); DEBUG_PRINT(logicalX, 3);
    DEBUG_PRINT("g, Y: "); DEBUG_PRINT(logicalY, 3);
    DEBUG_PRINT("g, Z: "); DEBUG_PRINT(logicalZ, 3); DEBUG_PRINTLN("g");
    
    // === FASE 3: Calcola offset finali per ottenere valori logici target ===
    // Target nella posizione iniziale: X_logico = +1g, Y_logico = 0g, Z_logico = 0g
    
    // Per ottenere i valori logici target, dobbiamo calcolare quali valori fisici servono
    // Invertiamo il mapping: se X_logico = Z_fisico * LOGICAL_X_SIGN, allora Z_fisico = X_logico / LOGICAL_X_SIGN
    
    float targetPhysicalX, targetPhysicalY, targetPhysicalZ;
    
    // Mapping inverso per trovare i valori fisici target
    auto getPhysicalFromLogical = [&](float targetLogX, float targetLogY, float targetLogZ) {
        // Reset dei valori fisici target
        targetPhysicalX = 0; targetPhysicalY = 0; targetPhysicalZ = 0;
        
        // X logico viene da quale asse fisico?
        if (LOGICAL_X_AXIS == 'X') targetPhysicalX += targetLogX * LOGICAL_X_SIGN;
        else if (LOGICAL_X_AXIS == 'Y') targetPhysicalY += targetLogX * LOGICAL_X_SIGN;
        else if (LOGICAL_X_AXIS == 'Z') targetPhysicalZ += targetLogX * LOGICAL_X_SIGN;
        
        // Y logico viene da quale asse fisico?
        if (LOGICAL_Y_AXIS == 'X') targetPhysicalX += targetLogY * LOGICAL_Y_SIGN;
        else if (LOGICAL_Y_AXIS == 'Y') targetPhysicalY += targetLogY * LOGICAL_Y_SIGN;
        else if (LOGICAL_Y_AXIS == 'Z') targetPhysicalZ += targetLogY * LOGICAL_Y_SIGN;
        
        // Z logico viene da quale asse fisico?
        if (LOGICAL_Z_AXIS == 'X') targetPhysicalX += targetLogZ * LOGICAL_Z_SIGN;
        else if (LOGICAL_Z_AXIS == 'Y') targetPhysicalY += targetLogZ * LOGICAL_Z_SIGN;  
        else if (LOGICAL_Z_AXIS == 'Z') targetPhysicalZ += targetLogZ * LOGICAL_Z_SIGN;
    };
    
    // Calcola i valori fisici necessari per ottenere: X_logico=+1g, Y_logico=0g, Z_logico=0g
    getPhysicalFromLogical(1.0f, 0.0f, 0.0f);
    
    DEBUG_PRINT("Valori fisici target - X: "); DEBUG_PRINT(targetPhysicalX, 3);
    DEBUG_PRINT("g, Y: "); DEBUG_PRINT(targetPhysicalY, 3);
    DEBUG_PRINT("g, Z: "); DEBUG_PRINT(targetPhysicalZ, 3); DEBUG_PRINTLN("g");
    
    // Calcola gli offset finali: offset = tensione_attuale - (target_g * sensitivity)
    offsetX = avgVoltX - (targetPhysicalX * SENSITIVITY);
    offsetY = avgVoltY - (targetPhysicalY * SENSITIVITY);
    offsetZ = avgVoltZ - (targetPhysicalZ * SENSITIVITY);
    
    DEBUG_PRINT("Offset finali - X: "); DEBUG_PRINT(offsetX, 3);
    DEBUG_PRINT("V, Y: "); DEBUG_PRINT(offsetY, 3);
    DEBUG_PRINT("V, Z: "); DEBUG_PRINT(offsetZ, 3); DEBUG_PRINTLN("V");
    
    // === FASE 4: Verifica finale ===
    // Test: calcola le accelerazioni con i nuovi offset
    float testPhysX = voltageToG(avgVoltX, offsetX);
    float testPhysY = voltageToG(avgVoltY, offsetY);
    float testPhysZ = voltageToG(avgVoltZ, offsetZ);
    
    DEBUG_PRINT("Test accelerazioni fisiche - X: "); DEBUG_PRINT(testPhysX, 3);
    DEBUG_PRINT("g, Y: "); DEBUG_PRINT(testPhysY, 3);
    DEBUG_PRINT("g, Z: "); DEBUG_PRINT(testPhysZ, 3); DEBUG_PRINTLN("g");
    
    // Applica mapping per vedere i valori logici finali
    float finalLogicalX, finalLogicalY, finalLogicalZ;
    applyLogicalMapping(testPhysX, testPhysY, testPhysZ, finalLogicalX, finalLogicalY, finalLogicalZ);
    
    DEBUG_PRINT("Test accelerazioni LOGICHE - X: "); DEBUG_PRINT(finalLogicalX, 3);
    DEBUG_PRINT("g, Y: "); DEBUG_PRINT(finalLogicalY, 3);
    DEBUG_PRINT("g, Z: "); DEBUG_PRINT(finalLogicalZ, 3); DEBUG_PRINTLN("g");
    DEBUG_PRINTLN("X logico dovrebbe essere ~1.0g, Y e Z logici dovrebbero essere ~0.0g");
    DEBUG_PRINTLN("========================");
    
    return true;
}

EulerAngles ADXL_readEuler() {
    // === ACQUISIZIONE DATI ===
    int rawX = analogRead(ADXL_PIN_X);
    int rawY = analogRead(ADXL_PIN_Y);
    int rawZ = analogRead(ADXL_PIN_Z);
    
    // Converti in tensioni
    float voltX = analogToVoltage(rawX);
    float voltY = analogToVoltage(rawY);
    float voltZ = analogToVoltage(rawZ);
    
    // Converti in accelerazioni (g) usando gli offset calibrati
    float accelX = voltageToG(voltX, offsetX);
    float accelY = voltageToG(voltY, offsetY);
    float accelZ = voltageToG(voltZ, offsetZ);
    
    // Applica la mappatura logica (ruota/scala gli assi secondo definizioni)
    float logicalX, logicalY, logicalZ;
    applyLogicalMapping(accelX, accelY, accelZ, logicalX, logicalY, logicalZ);

    // Usa i valori logici al posto di accelX/Y/Z
    accelX = logicalX;
    accelY = logicalY;
    accelZ = logicalZ;
    DEBUG_PRINT("DEBUG - LogicalX: "); DEBUG_PRINT(accelX, 3);
    DEBUG_PRINT(", LogicalZ: "); DEBUG_PRINT(accelZ, 3);
    float angle = atan2(accelZ, accelX) * 180.0f / PI;
    DEBUG_PRINT(", Angle: "); DEBUG_PRINTLN(angle, 2);

    // === PROCESSING AVANZATO ===
    
    // 1. Rilevamento movimento per filtraggio adattivo
    isMoving = detectMovement(accelX, accelY, accelZ);
    float dynamicAlpha = isMoving ? (ALPHA_ACCEL * 2.0f) : ALPHA_ACCEL;
    
    // 2. Filtro dead-zone per ridurre rumore quando fermo
    if (!isMoving) {
        accelX = deadZoneFilter(accelX, ACCEL_DEADZONE);
        accelY = deadZoneFilter(accelY, ACCEL_DEADZONE);
        accelZ = deadZoneFilter(accelZ, ACCEL_DEADZONE);
    }
    
    // 3. Filtro passa-basso esponenziale
    if (!filtersInitialized) {
        // Prima lettura: inizializza i filtri
        filteredX = accelX;
        filteredY = accelY;
        filteredZ = accelZ;
        filtersInitialized = true;
    } else {
        filteredX = exponentialFilter(filteredX, accelX, dynamicAlpha);
        filteredY = exponentialFilter(filteredY, accelY, dynamicAlpha);
        filteredZ = exponentialFilter(filteredZ, accelZ, dynamicAlpha);
    }
    
    // 4. Normalizzazione del vettore gravità (opzionale, per maggiore precisione)
    float magnitude = sqrt(filteredX*filteredX + filteredY*filteredY + filteredZ*filteredZ);
    if (magnitude > 0.5f && magnitude < 1.5f) {  // Solo se ragionevole
        filteredX /= magnitude;
        filteredY /= magnitude; 
        filteredZ /= magnitude;
    }
    
    // === CALCOLO ANGOLI ===
    float pitch = calculatePitch(filteredX, filteredY, filteredZ);
    float roll = calculateRoll(filteredX, filteredY, filteredZ);
    
    // 5. Filtro passa-basso anche sugli angoli per maggiore stabilità
    filteredPitch = exponentialFilter(filteredPitch, pitch, ALPHA_ANGLES);
    filteredRoll = exponentialFilter(filteredRoll, roll, ALPHA_ANGLES);
    
    // === OUTPUT ===
    EulerAngles result;
    result.pitch = filteredPitch;
    result.roll = filteredRoll;
    result.yaw = NAN;  // ADXL337 non può misurare yaw
    
    return result;
}

// Funzione di test per verificare i valori grezzi
void ADXL_printRawValues() {
    int rawX = analogRead(ADXL_PIN_X);
    int rawY = analogRead(ADXL_PIN_Y);
    int rawZ = analogRead(ADXL_PIN_Z);
    
    float voltX = analogToVoltage(rawX);
    float voltY = analogToVoltage(rawY);
    float voltZ = analogToVoltage(rawZ);
    
    float accelX = voltageToG(voltX, offsetX);
    float accelY = voltageToG(voltY, offsetY);
    float accelZ = voltageToG(voltZ, offsetZ);

    float logicalX, logicalY, logicalZ;
    applyLogicalMapping(accelX, accelY, accelZ, logicalX, logicalY, logicalZ);
    accelX = logicalX; accelY = logicalY; accelZ = logicalZ;
    
    DEBUG_PRINT("Raw ADC: X="); DEBUG_PRINT(rawX);
    DEBUG_PRINT(" Y="); DEBUG_PRINT(rawY);
    DEBUG_PRINT(" Z="); DEBUG_PRINT(rawZ);
    DEBUG_PRINT(" | Volt: X="); DEBUG_PRINT(voltX, 3);
    DEBUG_PRINT(" Y="); DEBUG_PRINT(voltY, 3);
    DEBUG_PRINT(" Z="); DEBUG_PRINT(voltZ, 3);
    DEBUG_PRINT(" | Accel(g): X="); DEBUG_PRINT(accelX, 3);
    DEBUG_PRINT(" Y="); DEBUG_PRINT(accelY, 3);
    DEBUG_PRINT(" Z="); DEBUG_PRINT(accelZ, 3);
    DEBUG_PRINT(" | Filtered: X="); DEBUG_PRINT(filteredX, 3);
    DEBUG_PRINT(" Y="); DEBUG_PRINT(filteredY, 3);
    DEBUG_PRINT(" Z="); DEBUG_PRINT(filteredZ, 3);
    DEBUG_PRINT(" | Moving: "); DEBUG_PRINT(isMoving ? "YES" : "NO");
    DEBUG_PRINTLN();
}

// Funzione per resettare i filtri (utile per ri-calibrazione)
void ADXL_resetFilters() {
    filtersInitialized = false;
    filteredX = filteredY = filteredZ = 0;
    filteredPitch = filteredRoll = 0;
    prevX = prevY = prevZ = 0;
    isMoving = false;
}