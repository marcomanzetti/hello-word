/*
 * Centralina faretti moto con MEMORIA allo spegnimento
 * ----------------------------------------------------
 * Sostituisce (o affianca) una centralina tipo "JS1 / JPAD" che
 * a ogni giro di chiave torna a "spento".
 *
 * - Alimentata SOTTO CHIAVE: a chiave spenta e' tutto spento, consumo zero.
 * - L'ultima modalita' scelta viene salvata in EEPROM (non si cancella
 *   senza corrente) e ripristinata alla riaccensione.
 * - Un solo pulsante al manubrio:
 *     pressione breve  -> modalita' successiva
 *     pressione lunga  -> spegne i faretti
 *
 * Scheda: Arduino Nano / Uno / Pro Mini (ATmega328P).
 * Vedi README.md per lo schema dei collegamenti.
 */

#include <EEPROM.h>

// ===================== CONFIGURAZIONE =====================

// Pin
const uint8_t PIN_PULSANTE = 2;   // pulsante verso GND (pull-up interno)
const uint8_t PIN_USCITA_A = 9;   // relè canale A (es. faretti bianchi / coppia 1)
const uint8_t PIN_USCITA_B = 10;  // relè canale B (es. faretti gialli / coppia 2)

// La maggior parte dei moduli relè a 5V con optoisolatore si attiva con LOW.
// Se il tuo relè si accende con HIGH (es. MOSFET o transistor) metti false.
const bool RELE_ATTIVO_BASSO = true;

// Modalita': {canale A, canale B}. La prima DEVE essere "tutto spento".
// Se hai una sola coppia di faretti lascia solo le prime due righe.
const uint8_t MODALITA[][2] = {
  {0, 0},  // 0: spenti
  {1, 0},  // 1: canale A
  {0, 1},  // 2: canale B
  {1, 1},  // 3: entrambi
};
const uint8_t NUM_MODALITA = sizeof(MODALITA) / sizeof(MODALITA[0]);

// Tempi (millisecondi)
const unsigned long RITARDO_AVVIO   = 1500; // attesa all'accensione (avviamento motore)
const unsigned long DEBOUNCE        = 30;
const unsigned long PRESSIONE_LUNGA = 800;  // oltre questo tempo = spegni
const unsigned long RITARDO_SALVA   = 2000; // salva dopo 2 s senza altre modifiche

// ===================== MEMORIA EEPROM =====================

const uint16_t INDIRIZZO_EEPROM = 0;
const uint16_t FIRMA = 0xFA70;    // per riconoscere dati validi

struct Memoria {
  uint16_t firma;
  uint8_t  modalita;
  uint8_t  controllo;             // = ~modalita, protegge da dati corrotti
};

uint8_t leggiModalitaSalvata() {
  Memoria m;
  EEPROM.get(INDIRIZZO_EEPROM, m);
  if (m.firma == FIRMA &&
      m.controllo == (uint8_t)~m.modalita &&
      m.modalita < NUM_MODALITA) {
    return m.modalita;
  }
  return 0; // prima accensione o dati non validi: spenti
}

void salvaModalita(uint8_t modalita) {
  Memoria m = { FIRMA, modalita, (uint8_t)~modalita };
  // put() usa update(): riscrive solo i byte cambiati -> la EEPROM dura di piu'
  EEPROM.put(INDIRIZZO_EEPROM, m);
}

// ===================== USCITE =====================

void scriviRele(uint8_t pin, bool acceso) {
  bool livello = RELE_ATTIVO_BASSO ? !acceso : acceso;
  digitalWrite(pin, livello ? HIGH : LOW);
}

void applicaModalita(uint8_t modalita) {
  scriviRele(PIN_USCITA_A, MODALITA[modalita][0]);
  scriviRele(PIN_USCITA_B, MODALITA[modalita][1]);
}

// ===================== STATO =====================

uint8_t modalitaCorrente = 0;
uint8_t modalitaSalvata  = 0;
unsigned long ultimoCambio = 0;

bool statoPulsante = false;       // true = premuto (stabile)
bool letturaPrecedente = false;
unsigned long tempoLettura = 0;
unsigned long inizioPressione = 0;
bool lungaGestita = false;

void cambiaModalita(uint8_t nuova) {
  if (nuova == modalitaCorrente) return;
  modalitaCorrente = nuova;
  applicaModalita(modalitaCorrente);
  ultimoCambio = millis();
}

// ===================== SETUP / LOOP =====================

void setup() {
  // Imposta il livello "spento" PRIMA di rendere i pin uscite,
  // cosi' i relè non fanno scatti all'accensione.
  scriviRele(PIN_USCITA_A, false);
  scriviRele(PIN_USCITA_B, false);
  pinMode(PIN_USCITA_A, OUTPUT);
  pinMode(PIN_USCITA_B, OUTPUT);
  pinMode(PIN_PULSANTE, INPUT_PULLUP);

  modalitaSalvata  = leggiModalitaSalvata();
  modalitaCorrente = modalitaSalvata;

  // Aspetta che passi l'avviamento (calo di tensione) prima di accendere.
  delay(RITARDO_AVVIO);
  applicaModalita(modalitaCorrente);
}

void loop() {
  unsigned long ora = millis();

  // --- Lettura pulsante con antirimbalzo ---
  bool lettura = (digitalRead(PIN_PULSANTE) == LOW);
  if (lettura != letturaPrecedente) {
    tempoLettura = ora;
    letturaPrecedente = lettura;
  }

  if ((ora - tempoLettura) > DEBOUNCE && lettura != statoPulsante) {
    statoPulsante = lettura;
    if (statoPulsante) {
      // appena premuto
      inizioPressione = ora;
      lungaGestita = false;
    } else if (!lungaGestita) {
      // rilasciato dopo pressione breve -> modalita' successiva
      cambiaModalita((modalitaCorrente + 1) % NUM_MODALITA);
    }
  }

  // Pressione lunga -> spegni (scatta mentre si tiene premuto)
  if (statoPulsante && !lungaGestita && (ora - inizioPressione) >= PRESSIONE_LUNGA) {
    lungaGestita = true;
    cambiaModalita(0);
  }

  // --- Salvataggio ritardato in EEPROM ---
  if (modalitaCorrente != modalitaSalvata && (ora - ultimoCambio) >= RITARDO_SALVA) {
    salvaModalita(modalitaCorrente);
    modalitaSalvata = modalitaCorrente;
  }
}
