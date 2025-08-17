#include <Arduino.h>
#include <math.h>
#include "project_config.h"
#include "debug_utils.h"
#include "sensor_bno.h"
#include "sensor_adxl.h"
#include "output.h"


// Costanti generiche
const int START_BTN_PIN = 2;  // bottone per avviare il setup()

// === PARAMETRI ===
// Mappatura logica come nel tuo codice originale
#define MAP_VERTICAL   pitch
#define MAP_LATERAL    roll
#define MAP_TORSION    yaw

// PARAMETRI BIRD DOG SPECIFICI
float targetVerticale    = 90.0;     // gradi - target per braccio (ADXL)
float tolleranzaAngolo   = 10.0;     // ± gradi per braccio
float tolleranzaIniziale = 10.0;     // ± gradi dalla partenza
unsigned long tolleranzaTempo = 200;  // ms fuori target senza reset
unsigned long minHoldTime     = 500;  // ms per contare una ripetizione

// === PARAMETRI DUAL MODE SPECIFICI PER BIRD DOG ===
#if ACTIVE_SENSOR == 3
  // TARGET SEPARATI per ogni sensore
  float targetVerticale_ADXL = 90.0;   // Target per braccio (ADXL sul polso)
  float targetStabilita_BNO = 0.0;    // Target per stabilità caviglia (BNO)
  
  // TOLLERANZE SEPARATE
  float tolleranzaAngolo_ADXL = 10.0;  // ± gradi per movimento braccio
  float tolleranzaStabilita_BNO = 20.0; // ± gradi per stabilità caviglia
  
  // LOGICA BIRD DOG
  bool richiedEntrambi = true;         // true = braccio in target E caviglia stabile
  
  // MODALITÀ BIRD DOG: verifica movimento significativo
  bool modalitaBirdDog = true;         // true = attiva logica bird dog
  float sogliaMovimentoMinimo = 15.0;  // gradi - movimento minimo per considerare "attivo"
#endif

// === VARIABILI ANGOLI ===
// Valori dai singoli sensori
float roll_bno = 0, pitch_bno = 0, yaw_bno = 0;
float roll_adxl = 0, pitch_adxl = 0, yaw_adxl = 0;

// Valori finali - per output CSV (compatibilità)
float roll = 0, pitch = 0, yaw = 0; 

float movimentoVerticale = 0, rotazioneLaterale = 0, torsione = 0;
float posizioneIniziale = 0.0;

#if ACTIVE_SENSOR == 3
  float posizioneIniziale_BNO = 0.0;   // Posizione iniziale caviglia
  float posizioneIniziale_ADXL = 0.0;  // Posizione iniziale braccio
#endif

// === STATO ===
bool inTarget = false;
bool faseAndata = false;
unsigned long tempoInizioPosizione = 0;
unsigned long tempoPosizione = 0;
unsigned long tempoUscitaTarget = 0;  // RINOMINATO per chiarezza
unsigned int conteggioRipetizioni = 0;

// === STATO DUAL MODE ===
#if ACTIVE_SENSOR == 3
  bool inTargetBNO = false;   // Caviglia stabile
  bool inTargetADXL = false;  // Braccio in posizione
  
  // Statistiche movimento per bird dog
  float rangePitch_BNO = 0, rangePitch_ADXL = 0;
  float minPitch_BNO = 999, maxPitch_BNO = -999;
  float minPitch_ADXL = 999, maxPitch_ADXL = -999;
#endif

// --- Calcolo differenza angolare con wrap-around ---
static inline float diffAngolo(float a, float b) {
  float diff = fmodf((a - b + 540.0f), 360.0f) - 180.0f;
  return fabsf(diff);
}

#if ACTIVE_SENSOR == 3
// --- Aggiorna statistiche movimento ---
static void aggiornaStatisticheMovimento() {
  // Traccia range di movimento per ogni sensore
  if (pitch_bno < minPitch_BNO) minPitch_BNO = pitch_bno;
  if (pitch_bno > maxPitch_BNO) maxPitch_BNO = pitch_bno;
  if (pitch_adxl < minPitch_ADXL) minPitch_ADXL = pitch_adxl;
  if (pitch_adxl > maxPitch_ADXL) maxPitch_ADXL = pitch_adxl;
  
  rangePitch_BNO = maxPitch_BNO - minPitch_BNO;
  rangePitch_ADXL = maxPitch_ADXL - minPitch_ADXL;
}

// --- Verifica stabilità caviglia (BNO) ---
static bool verificaStabilitaCaviglia() {
  float diffDaIniziale = diffAngolo(pitch_bno, posizioneIniziale_BNO);
  return (diffDaIniziale <= tolleranzaStabilita_BNO);
}

// --- Verifica posizione braccio (ADXL) ---
static bool verificaPosizioneBraccio() {
  float diffTarget = diffAngolo(pitch_adxl, targetVerticale_ADXL);
  return (diffTarget <= tolleranzaAngolo_ADXL);
}
#endif

void setup() {
  // abilitazione dell'pull-up interno
  pinMode(START_BTN_PIN, INPUT_PULLUP);
  delay(100);

  // Resta in attesa che il bottone sia premuto per continuare
  // Se INPUT_PULLUP è abilitato, la lettura è invertita. 
  // press -> LOW
  // idle -> HIGH
  while(digitalRead(START_BTN_PIN) == HIGH) {
    delay(100);
  }
  DEBUG_PRINTLN("START: bottone premuto");

  Serial.begin(115200);
  delay(200);
  // invia messaggio per confermare l'avvio
  Serial.println("INIT_START");

  bool bnoOk = false, adxlOk = false;

#if ACTIVE_SENSOR == 1
  if (!BNO_begin()) {
    DEBUG_PRINTLN("Errore: sensore BNO055 non trovato");
    while (1) { delay(1000); }
  }
  bnoOk = true;
#elif ACTIVE_SENSOR == 2
  if (!ADXL_begin()) {
    DEBUG_PRINTLN("Errore: inizializzazione ADXL337 fallita");
    while (1) { delay(1000); }
  }
  adxlOk = true;
#elif ACTIVE_SENSOR == 3
  // Dual mode: inizializza entrambi
  DEBUG_PRINTLN("=== BIRD DOG MODE: ADXL(braccio) + BNO(caviglia) ===");
  
  bnoOk = BNO_begin();
  if (!bnoOk) {
    DEBUG_PRINTLN("WARNING: BNO055 non trovato, continuo solo con ADXL337");
  }
  
  adxlOk = ADXL_begin();
  if (!adxlOk) {
    DEBUG_PRINTLN("WARNING: ADXL337 fallito, continuo solo con BNO055");
  }
  
  if (!bnoOk && !adxlOk) {
    DEBUG_PRINTLN("ERRORE: Nessun sensore disponibile!");
    while (1) { delay(1000); }
  }
  
  DEBUG_PRINT("Sensori - BNO055 (caviglia): ");
  DEBUG_PRINT(bnoOk ? "OK" : "NO");
  DEBUG_PRINT(", ADXL337 (braccio): ");
  DEBUG_PRINTLN(adxlOk ? "OK" : "NO");
  
  DEBUG_PRINT("Target - ADXL (braccio): "); DEBUG_PRINT(targetVerticale_ADXL, 1); DEBUG_PRINT("° (±");
  DEBUG_PRINT(tolleranzaAngolo_ADXL, 1); DEBUG_PRINTLN("°)");
  
  DEBUG_PRINT("Stabilità - BNO (caviglia): ±"); DEBUG_PRINT(tolleranzaStabilita_BNO, 1); 
  DEBUG_PRINTLN("° dalla posizione iniziale");
  
  if (richiedEntrambi) {
    DEBUG_PRINTLN("Modalità BIRD DOG: Braccio in target E caviglia stabile");
  }
#else
  #error "ACTIVE_SENSOR deve essere 1 (BNO055), 2 (ADXL337) o 3 (DUAL)"
#endif

  // Legge posizioni iniziali
  delay(1000); // Più tempo per stabilizzarsi
  
  DEBUG_PRINTLN("Calibrazione posizioni iniziali...");
  
#if ACTIVE_SENSOR == 1
  EulerAngles e = BNO_readEuler();
  yaw = e.yaw; roll = e.roll; pitch = e.pitch;
  posizioneIniziale = pitch;
#elif ACTIVE_SENSOR == 2
  EulerAngles e = ADXL_readEuler();
  yaw = e.yaw; roll = e.roll; pitch = e.pitch;
  posizioneIniziale = pitch;
#elif ACTIVE_SENSOR == 3
  // Dual mode: calibra entrambi SEPARATAMENTE
  if (bnoOk) {
    EulerAngles bno = BNO_readEuler();
    yaw_bno = bno.yaw; roll_bno = bno.roll; pitch_bno = bno.pitch;
    posizioneIniziale_BNO = pitch_bno;
    DEBUG_PRINT("Posizione iniziale BNO (caviglia): "); DEBUG_PRINTLN(posizioneIniziale_BNO, 2);
  }
  
  if (adxlOk) {
    EulerAngles adxl = ADXL_readEuler();
    yaw_adxl = adxl.yaw; roll_adxl = adxl.roll; pitch_adxl = adxl.pitch;
    posizioneIniziale_ADXL = pitch_adxl;
    DEBUG_PRINT("Posizione iniziale ADXL (braccio): "); DEBUG_PRINTLN(posizioneIniziale_ADXL, 2);
  }
  
  // Per compatibilità con il resto del codice, usa il sensore primario (ADXL per bird dog)
  if (adxlOk) {
    yaw = yaw_adxl; roll = roll_adxl; pitch = pitch_adxl;
    posizioneIniziale = posizioneIniziale_ADXL;
  } else if (bnoOk) {
    yaw = yaw_bno; roll = roll_bno; pitch = pitch_bno;
    posizioneIniziale = posizioneIniziale_BNO;
  }
  
  // Inizializza statistiche
  minPitch_BNO = maxPitch_BNO = pitch_bno;
  minPitch_ADXL = maxPitch_ADXL = pitch_adxl;
#endif

  DEBUG_PRINTLN("Calibrazione completata. Inizio esercizio...");
  delay(1000);
  // Invia messaggio di completamento
  Serial.println("INIT_COMPLETE");

  // Header CSV per Processing
  printCsvHeader();
}

void loop() {
  // === LETTURA SENSORI ===
  bool bnoDisponibile = false, adxlDisponibile = false;
  
#if ACTIVE_SENSOR == 1
  EulerAngles e = BNO_readEuler();
  yaw = e.yaw; roll = e.roll; pitch = e.pitch;
#elif ACTIVE_SENSOR == 2
  EulerAngles e = ADXL_readEuler();
  yaw = e.yaw; roll = e.roll; pitch = e.pitch;
#elif ACTIVE_SENSOR == 3
  // Dual mode: leggi da entrambi SEPARATAMENTE
  EulerAngles bno = BNO_readEuler();
  if (!isnan(bno.pitch) && !isnan(bno.roll)) {
    yaw_bno = bno.yaw; roll_bno = bno.roll; pitch_bno = bno.pitch;
    bnoDisponibile = true;
  }
  
  EulerAngles adxl = ADXL_readEuler();
  if (!isnan(adxl.pitch) && !isnan(adxl.roll)) {
    roll_adxl = adxl.roll; pitch_adxl = adxl.pitch; yaw_adxl = adxl.yaw;
    adxlDisponibile = true;
  }
  
  // Aggiorna statistiche movimento
  if (bnoDisponibile && adxlDisponibile) {
    aggiornaStatisticheMovimento();
  }
  
  // Seleziona valori per output CSV (usa ADXL come primario per bird dog)
  if (adxlDisponibile) {
    yaw = yaw_adxl; roll = roll_adxl; pitch = pitch_adxl;
  } else if (bnoDisponibile) {
    yaw = yaw_bno; roll = roll_bno; pitch = pitch_bno;
  }
#endif

  // Aggiorna variabili di output per compatibilità
  movimentoVerticale = MAP_VERTICAL;
  rotazioneLaterale  = MAP_LATERAL;
  torsione           = MAP_TORSION;

  // === CALCOLO TARGET - BIRD DOG LOGIC ===
#if ACTIVE_SENSOR == 3
  if (bnoDisponibile && adxlDisponibile) {
    // BIRD DOG: logica specifica
    inTargetBNO = verificaStabilitaCaviglia();    // Caviglia stabile
    inTargetADXL = verificaPosizioneBraccio();    // Braccio in target
    
    // Combinazione target: entrambi devono essere OK per bird dog
    inTarget = (inTargetBNO && inTargetADXL);
    
    // DEBUG specifico per bird dog
    static unsigned long lastDebugTime = 0;
    if (millis() - lastDebugTime > 1000) { // Debug ogni secondo
      DEBUG_PRINT("BIRD DOG - Braccio(ADXL): ");
      DEBUG_PRINT(pitch_adxl, 1); DEBUG_PRINT("° -> ");
      DEBUG_PRINT(inTargetADXL ? "TARGET" : "fuori");
      DEBUG_PRINT(" | Caviglia(BNO): ");
      DEBUG_PRINT(pitch_bno, 1); DEBUG_PRINT("° -> ");
      DEBUG_PRINT(inTargetBNO ? "STABILE" : "instabile");
      DEBUG_PRINT(" | Combinato: ");
      DEBUG_PRINTLN(inTarget ? "OK" : "NO");
      lastDebugTime = millis();
    }
  } else {
    // Fallback a singolo sensore
    float diffTarget = diffAngolo(movimentoVerticale, targetVerticale);
    inTarget = (diffTarget <= tolleranzaAngolo);
  }
#else
  // Modalità singolo sensore
  float diffTarget = diffAngolo(movimentoVerticale, targetVerticale);
  inTarget = (diffTarget <= tolleranzaAngolo);
#endif

  // Calcola se siamo vicini alla posizione iniziale (per il ritorno)
  float diffStart = diffAngolo(movimentoVerticale, posizioneIniziale);
  bool attualeInStart = (diffStart <= tolleranzaIniziale);

  // === GESTIONE TOLLERANZA TEMPORALE (CORREZIONE LOGICA) ===
  if (inTarget) {
    // Siamo in target
    if (tempoInizioPosizione == 0) {
      // Prima volta in target
      tempoInizioPosizione = millis();
    }
    tempoPosizione = millis() - tempoInizioPosizione;
    tempoUscitaTarget = 0; // Reset timer uscita
  } else {
    // Siamo fuori target
    if (tempoInizioPosizione > 0) {
      // Eravamo in target, ora siamo usciti
      if (tempoUscitaTarget == 0) {
        tempoUscitaTarget = millis(); // Inizia timer tolleranza
      }
      
      // Verifica se siamo fuori target da troppo tempo
      if (millis() - tempoUscitaTarget > tolleranzaTempo) {
        // Reset completo - siamo davvero usciti
        tempoInizioPosizione = 0;
        tempoPosizione = 0;
        tempoUscitaTarget = 0;
      }
      // Altrimenti manteniamo lo stato precedente (tolleranza)
    }
  }

  // Aggiorna inTarget considerando la tolleranza temporale
  bool inTargetEffettivo = (tempoInizioPosizione > 0); // Siamo in target o in tolleranza

  // === CONTEGGIO RIPETIZIONI (CORREZIONE LOGICA) ===
  if (inTargetEffettivo && !faseAndata && tempoPosizione >= minHoldTime) {
    faseAndata = true; // Fase di andata raggiunta e mantenuta
    DEBUG_PRINTLN("FASE ANDATA completata - target raggiunto e mantenuto");
  }
  
  if (faseAndata && attualeInStart) {
    // Ripetizione completata: andata + ritorno
    conteggioRipetizioni++;
    faseAndata = false;
    
    // Reset timers per la prossima ripetizione
    tempoInizioPosizione = 0;
    tempoPosizione = 0;
    tempoUscitaTarget = 0;
    
    DEBUG_PRINT("RIPETIZIONE #"); DEBUG_PRINT(conteggioRipetizioni);
    DEBUG_PRINTLN(" COMPLETATA!");
    
#if ACTIVE_SENSOR == 3
    DEBUG_PRINT("Dettagli - ADXL: "); DEBUG_PRINT(pitch_adxl, 1);
    DEBUG_PRINT("°, BNO: "); DEBUG_PRINT(pitch_bno, 1); DEBUG_PRINTLN("°");
#endif
  }

  // === OUTPUT CSV ===
  // Usa inTargetEffettivo per l'output
  printCsvRow(movimentoVerticale, rotazioneLaterale, torsione,
              inTargetEffettivo, tempoPosizione, conteggioRipetizioni);

  delay(50); // ~20 Hz
}