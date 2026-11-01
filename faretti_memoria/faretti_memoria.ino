/*
 * Centralina faretti moto con MEMORIA allo spegnimento
 * ----------------------------------------------------
 * Sostituisce una centralina tipo "JS1 / JPAD" che a ogni giro di chiave
 * torna a "spento", mantenendo le stesse funzioni:
 *   - accensione / spegnimento
 *   - luminosita' regolabile (PWM)
 *   - strobo
 *   - (opzionale) due colori: bianco / giallo / entrambi
 *
 * Tutto (acceso/spento, modalita', luminosita', colore) viene salvato in
 * EEPROM e ripristinato a ogni accensione della moto.
 *
 * UN SOLO PULSANTE al manubrio:
 *   1 click             -> accende / spegne
 *   2 click             -> fisso <-> strobo
 *   3 click             -> colore: bianco -> giallo -> entrambi (se 2 canali)
 *   tieni premuto       -> luminosita' a rampa (rilascia quando ti piace;
 *                          alla pressione successiva la rampa va al contrario)
 *
 * Scheda: Arduino Nano / Uno / Pro Mini 5V (ATmega328P).
 * Uscite: MOSFET P-channel lato positivo (vedi schema.svg e README.md).
 */

#include <EEPROM.h>

// ===================== CONFIGURAZIONE =====================

// Pin
const uint8_t PIN_PULSANTE = 2;   // pulsante verso GND (pull-up interno)
const uint8_t PIN_CANALE_A = 9;   // PWM -> faretti bianchi (o unico colore)
const uint8_t PIN_CANALE_B = 10;  // PWM -> faretti gialli
const uint8_t PIN_SPIA     = 5;   // PWM -> LED spia (facoltativo)

// 1 = faretti di un solo colore, 2 = faretti bianco + giallo (3 fili)
const uint8_t NUM_CANALI = 2;

// Luminosita' (livello "percepito"; l'uscita PWM e' corretta in modo che
// la rampa sembri uniforme all'occhio)
const uint8_t LUM_MIN = 40;           // circa 2,5% di potenza
const uint8_t LUM_MAX = 255;          // 100%
const unsigned long RAMPA_MS = 2500;  // tempo per passare da minimo a massimo

// Strobo: durate in ms alternate ACCESO, SPENTO, ACCESO, SPENTO...
const uint16_t SEQUENZA_STROBO[] = { 40, 60, 40, 60, 40, 500 };

// Se false, all'accensione della moto lo strobo riparte come luce fissa
// (la luminosita' e il colore restano comunque memorizzati).
const bool RIPRISTINA_STROBO = true;

// Tempi (ms)
const unsigned long RITARDO_AVVIO   = 1500; // attesa all'accensione (avviamento)
const unsigned long DISSOLVENZA     = 300;  // accensione morbida
const unsigned long DEBOUNCE        = 30;
const unsigned long FINESTRA_CLICK  = 350;  // tempo massimo tra due click
const unsigned long PRESSIONE_LUNGA = 500;  // oltre = regolazione luminosita'
const unsigned long RITARDO_SALVA   = 2000; // salva dopo 2 s senza modifiche

// ===================== STATO E MEMORIA =====================

enum : uint8_t { FISSO = 0, STROBO = 1 };
enum : uint8_t { BIANCO = 0, GIALLO = 1, ENTRAMBI = 2 };

struct Stato {
  uint8_t acceso;
  uint8_t modalita;
  uint8_t luminosita;
  uint8_t colore;
};

struct Memoria {
  uint16_t firma;
  Stato    stato;
  uint8_t  controllo;
};

const uint16_t INDIRIZZO_EEPROM = 0;
const uint16_t FIRMA = 0xFA71;

Stato stato   = { 0, FISSO, LUM_MAX, BIANCO };
Stato salvato = stato;

uint8_t calcolaControllo(const Stato &s) {
  return 0xA5 ^ s.acceso ^ (s.modalita << 1) ^ s.luminosita ^ (s.colore << 3);
}

bool statoValido(const Stato &s) {
  return s.acceso <= 1 && s.modalita <= STROBO &&
         s.luminosita >= LUM_MIN && s.colore <= ENTRAMBI;
}

void caricaStato() {
  Memoria m;
  EEPROM.get(INDIRIZZO_EEPROM, m);
  if (m.firma == FIRMA && m.controllo == calcolaControllo(m.stato) &&
      statoValido(m.stato)) {
    stato = m.stato;
  }
  if (NUM_CANALI < 2) stato.colore = BIANCO;
  salvato = stato;
}

void salvaStato() {
  Memoria m = { FIRMA, stato, calcolaControllo(stato) };
  EEPROM.put(INDIRIZZO_EEPROM, m);  // riscrive solo i byte cambiati
  salvato = stato;
}

bool statoCambiato() {
  return stato.acceso != salvato.acceso || stato.modalita != salvato.modalita ||
         stato.luminosita != salvato.luminosita || stato.colore != salvato.colore;
}

// ===================== USCITE =====================

unsigned long ultimoCambio = 0;
unsigned long inizioDissolvenza = 0;
unsigned long inizioStrobo = 0;

void segnaModifica(unsigned long ora) { ultimoCambio = ora; }

void avviaLuce(unsigned long ora) {
  inizioDissolvenza = ora;
  inizioStrobo = ora;
}

// Correzione "gamma" semplice: l'occhio percepisce la luce in modo non lineare
uint8_t pwmDaLivello(uint8_t livello) {
  return (uint16_t)livello * livello / 255;
}

bool faseStroboAccesa(unsigned long ora) {
  const uint8_t n = sizeof(SEQUENZA_STROBO) / sizeof(SEQUENZA_STROBO[0]);
  unsigned long totale = 0;
  for (uint8_t i = 0; i < n; i++) totale += SEQUENZA_STROBO[i];
  unsigned long t = (ora - inizioStrobo) % totale;
  for (uint8_t i = 0; i < n; i++) {
    if (t < SEQUENZA_STROBO[i]) return (i % 2) == 0;
    t -= SEQUENZA_STROBO[i];
  }
  return false;
}

void aggiornaUscite(unsigned long ora) {
  uint8_t pwm = 0;
  if (stato.acceso) {
    pwm = pwmDaLivello(stato.luminosita);
    unsigned long trascorso = ora - inizioDissolvenza;
    if (trascorso < DISSOLVENZA) pwm = (uint32_t)pwm * trascorso / DISSOLVENZA;
    if (stato.modalita == STROBO && !faseStroboAccesa(ora)) pwm = 0;
  }
  bool usaA = (NUM_CANALI < 2) || stato.colore != GIALLO;
  bool usaB = (NUM_CANALI >= 2) && stato.colore != BIANCO;
  analogWrite(PIN_CANALE_A, usaA ? pwm : 0);
  analogWrite(PIN_CANALE_B, usaB ? pwm : 0);
  analogWrite(PIN_SPIA, pwm ? 40 : 0);   // spia tenue quando i faretti sono accesi
}

// ===================== PULSANTE =====================

bool premuto = false;            // stato stabile (dopo antirimbalzo)
bool letturaPrecedente = false;
unsigned long tempoLettura = 0;
unsigned long tempoPressione = 0;
unsigned long tempoRilascio = 0;
uint8_t click = 0;

bool inRampa = false;
int8_t direzioneRampa = -1;      // la prima regolazione abbassa
unsigned long ultimoPassoRampa = 0;
const unsigned long PASSO_RAMPA = RAMPA_MS / (LUM_MAX - LUM_MIN);

void eseguiClick(uint8_t n, unsigned long ora) {
  if (n == 1) {
    stato.acceso = !stato.acceso;
    if (stato.acceso) avviaLuce(ora);
  } else if (!stato.acceso) {
    return;                       // doppio/triplo click a luci spente: niente
  } else if (n == 2) {
    stato.modalita = (stato.modalita == FISSO) ? STROBO : FISSO;
    inizioStrobo = ora;
  } else if (NUM_CANALI >= 2) {
    stato.colore = (stato.colore + 1) % 3;
  }
  segnaModifica(ora);
}

void gestisciPulsante(unsigned long ora) {
  bool lettura = (digitalRead(PIN_PULSANTE) == LOW);
  if (lettura != letturaPrecedente) {
    letturaPrecedente = lettura;
    tempoLettura = ora;
  }

  if ((ora - tempoLettura) >= DEBOUNCE && lettura != premuto) {
    premuto = lettura;
    if (premuto) {
      tempoPressione = ora;
    } else if (inRampa) {
      inRampa = false;
      direzioneRampa = -direzioneRampa;  // la prossima volta al contrario
    } else if ((ora - tempoPressione) < PRESSIONE_LUNGA) {
      click++;
      tempoRilascio = ora;
    }
  }

  // Pressione lunga a luci accese -> regolazione luminosita'
  if (premuto && !inRampa && stato.acceso &&
      (ora - tempoPressione) >= PRESSIONE_LUNGA) {
    inRampa = true;
    click = 0;
    if (stato.luminosita >= LUM_MAX) direzioneRampa = -1;
    else if (stato.luminosita <= LUM_MIN) direzioneRampa = 1;
    ultimoPassoRampa = ora;
  }

  if (inRampa) {
    while ((ora - ultimoPassoRampa) >= PASSO_RAMPA) {
      ultimoPassoRampa += PASSO_RAMPA;
      int16_t nuovo = (int16_t)stato.luminosita + direzioneRampa;
      if (nuovo >= LUM_MIN && nuovo <= LUM_MAX) {
        stato.luminosita = (uint8_t)nuovo;
        segnaModifica(ora);
      }
    }
  }

  // Fine della sequenza di click
  if (click > 0 && !premuto && (ora - tempoRilascio) > FINESTRA_CLICK) {
    eseguiClick(click, ora);
    click = 0;
  }
}

// ===================== SETUP / LOOP =====================

void setup() {
  // Uscite basse = MOSFET spenti (le resistenze sulle basi le tengono
  // spente anche mentre l'Arduino si avvia).
  digitalWrite(PIN_CANALE_A, LOW);
  digitalWrite(PIN_CANALE_B, LOW);
  digitalWrite(PIN_SPIA, LOW);
  pinMode(PIN_CANALE_A, OUTPUT);
  pinMode(PIN_CANALE_B, OUTPUT);
  pinMode(PIN_SPIA, OUTPUT);
  pinMode(PIN_PULSANTE, INPUT_PULLUP);

  caricaStato();
  if (!RIPRISTINA_STROBO) stato.modalita = FISSO;

  delay(RITARDO_AVVIO);           // lascia passare l'avviamento del motore
  avviaLuce(millis());
}

void loop() {
  unsigned long ora = millis();
  gestisciPulsante(ora);

  if (statoCambiato() && (ora - ultimoCambio) >= RITARDO_SALVA && !inRampa) {
    salvaStato();
  }

  aggiornaUscite(ora);
}
