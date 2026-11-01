/*
 * Centralina faretti moto con MEMORIA allo spegnimento
 * ----------------------------------------------------
 * Per faretti a 3 funzioni: DRL, ANABBAGLIANTE, ABBAGLIANTE.
 * Sostituisce una centralina tipo "JS1 / JPAD" che a ogni giro di chiave
 * torna a "spento", mantenendo le stesse funzioni:
 *   - accensione / spegnimento
 *   - scelta della luce: DRL -> anabbagliante -> abbagliante
 *   - luminosita' regolabile (PWM), memorizzata separatamente per ogni luce
 *   - strobo
 *   - (facoltativo) abbagliante sincronizzato con l'abbagliante della moto
 *
 * Tutto (acceso/spento, luce, strobo, luminosita') viene salvato in EEPROM
 * e ripristinato a ogni accensione della moto.
 *
 * UN SOLO PULSANTE al manubrio:
 *   1 click        -> accende / spegne (riparte dall'ultima luce usata)
 *   2 click        -> luce successiva: DRL -> anabbagliante -> abbagliante
 *   3 click        -> strobo si' / no
 *   tieni premuto  -> luminosita' della luce attuale a rampa (rilascia quando
 *                     ti piace; alla pressione successiva va al contrario)
 *
 * Scheda: Arduino Nano / Uno / Pro Mini 5V (ATmega328P).
 * Uscite: MOSFET P-channel lato positivo (vedi schema.svg e README.md).
 */

#include <EEPROM.h>

// ===================== CONFIGURAZIONE =====================

// Pin
const uint8_t PIN_PULSANTE = 2;   // pulsante verso GND (pull-up interno)
const uint8_t PIN_ABB_MOTO = 4;   // ingresso abbagliante moto (PC817, attivo basso)
const uint8_t PIN_SPIA     = 5;   // PWM -> LED spia (facoltativo)
const uint8_t PIN_DRL      = 6;   // PWM -> + DRL faretti
const uint8_t PIN_ANAB     = 9;   // PWM -> + anabbagliante faretti
const uint8_t PIN_ABB      = 10;  // PWM -> + abbagliante faretti

// Ingresso abbagliante moto: con l'abbagliante della moto acceso (o il
// lampeggio), i faretti accesi passano in abbagliante al 100%.
// Se non colleghi l'optoisolatore lascia pure true: l'ingresso resta inattivo.
const bool USA_ABB_MOTO = true;

// Combinazioni delle luci (dipendono da come sono fatti i tuoi faretti)
const bool DRL_CON_FARI = true;   // il DRL resta acceso anche in anabb./abbagl.
const bool ANAB_CON_ABB = true;   // l'anabbagliante resta acceso con l'abbagliante

// Luminosita' (livello "percepito", con correzione per l'occhio)
const uint8_t LUM_MIN = 40;           // circa 2,5% di potenza
const uint8_t LUM_MAX = 255;          // 100%
const unsigned long RAMPA_MS = 2500;  // tempo per passare da minimo a massimo

// Strobo: durate in ms alternate ACCESO, SPENTO, ACCESO, SPENTO...
const uint16_t SEQUENZA_STROBO[] = { 40, 60, 40, 60, 40, 500 };

// Se false, all'accensione della moto lo strobo riparte spento
// (luce e luminosita' restano comunque memorizzate).
const bool RIPRISTINA_STROBO = true;

// Tempi (ms)
const unsigned long RITARDO_AVVIO   = 1500; // attesa all'accensione (avviamento)
const unsigned long DISSOLVENZA     = 300;  // accensione morbida
const unsigned long DEBOUNCE        = 30;
const unsigned long FINESTRA_CLICK  = 350;  // tempo massimo tra due click
const unsigned long PRESSIONE_LUNGA = 500;  // oltre = regolazione luminosita'
const unsigned long RITARDO_SALVA   = 2000; // salva dopo 2 s senza modifiche

// ===================== STATO E MEMORIA =====================

enum : uint8_t { DRL = 0, ANAB = 1, ABB = 2, NUM_LUCI = 3 };

struct Stato {
  uint8_t acceso;
  uint8_t luce;                 // DRL / ANAB / ABB
  uint8_t strobo;
  uint8_t lum[NUM_LUCI];        // luminosita' di ogni luce
};

struct Memoria {
  uint16_t firma;
  Stato    stato;
  uint8_t  controllo;
};

const uint16_t INDIRIZZO_EEPROM = 0;
const uint16_t FIRMA = 0xFA72;

Stato stato   = { 0, ANAB, 0, { LUM_MAX, LUM_MAX, LUM_MAX } };
Stato salvato = stato;

uint8_t calcolaControllo(const Stato &s) {
  const uint8_t *p = (const uint8_t *)&s;
  uint8_t c = 0xA5;
  for (uint8_t i = 0; i < sizeof(Stato); i++) c = (uint8_t)((c << 1) | (c >> 7)) ^ p[i];
  return c;
}

bool statoValido(const Stato &s) {
  if (s.acceso > 1 || s.luce >= NUM_LUCI || s.strobo > 1) return false;
  for (uint8_t i = 0; i < NUM_LUCI; i++) if (s.lum[i] < LUM_MIN) return false;
  return true;
}

void caricaStato() {
  Memoria m;
  EEPROM.get(INDIRIZZO_EEPROM, m);
  if (m.firma == FIRMA && m.controllo == calcolaControllo(m.stato) &&
      statoValido(m.stato)) {
    stato = m.stato;
  }
  salvato = stato;
}

void salvaStato() {
  Memoria m = { FIRMA, stato, calcolaControllo(stato) };
  EEPROM.put(INDIRIZZO_EEPROM, m);  // riscrive solo i byte cambiati
  salvato = stato;
}

bool statoCambiato() {
  return memcmp(&stato, &salvato, sizeof(Stato)) != 0;
}

// ===================== USCITE =====================

unsigned long ultimoCambio = 0;
unsigned long inizioDissolvenza = 0;
unsigned long inizioStrobo = 0;
bool abbMoto = false;           // abbagliante della moto acceso (filtrato)

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
  uint8_t luce = stato.luce;
  uint8_t livello = stato.lum[luce];
  bool strobo = stato.strobo;

  // Abbagliante della moto acceso -> abbagliante pieno, niente strobo
  if (stato.acceso && abbMoto) {
    luce = ABB;
    livello = LUM_MAX;
    strobo = false;
  }

  uint8_t pwm = 0;
  if (stato.acceso) {
    pwm = pwmDaLivello(livello);
    unsigned long trascorso = ora - inizioDissolvenza;
    if (trascorso < DISSOLVENZA) pwm = (uint32_t)pwm * trascorso / DISSOLVENZA;
    if (strobo && !faseStroboAccesa(ora)) pwm = 0;
  }

  bool drl  = (luce == DRL) || DRL_CON_FARI;
  bool anab = (luce == ANAB) || (luce == ABB && ANAB_CON_ABB);
  bool abb  = (luce == ABB);

  analogWrite(PIN_DRL,  drl  ? pwm : 0);
  analogWrite(PIN_ANAB, anab ? pwm : 0);
  analogWrite(PIN_ABB,  abb  ? pwm : 0);
  analogWrite(PIN_SPIA, stato.acceso ? 40 : 0);  // spia tenue a faretti accesi
}

// ===================== INGRESSO ABBAGLIANTE MOTO =====================

bool letturaAbbPrec = false;
unsigned long tempoLetturaAbb = 0;

void gestisciAbbMoto(unsigned long ora) {
  if (!USA_ABB_MOTO) return;
  bool lettura = (digitalRead(PIN_ABB_MOTO) == LOW);
  if (lettura != letturaAbbPrec) {
    letturaAbbPrec = lettura;
    tempoLetturaAbb = ora;
  }
  if ((ora - tempoLetturaAbb) >= DEBOUNCE) abbMoto = lettura;
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
    stato.luce = (stato.luce + 1) % NUM_LUCI;
  } else {
    stato.strobo = !stato.strobo;
    inizioStrobo = ora;
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

  uint8_t &lum = stato.lum[stato.luce];

  // Pressione lunga a luci accese -> regolazione luminosita' della luce attuale
  if (premuto && !inRampa && stato.acceso &&
      (ora - tempoPressione) >= PRESSIONE_LUNGA) {
    inRampa = true;
    click = 0;
    if (lum >= LUM_MAX) direzioneRampa = -1;
    else if (lum <= LUM_MIN) direzioneRampa = 1;
    ultimoPassoRampa = ora;
  }

  if (inRampa) {
    while ((ora - ultimoPassoRampa) >= PASSO_RAMPA) {
      ultimoPassoRampa += PASSO_RAMPA;
      int16_t nuovo = (int16_t)lum + direzioneRampa;
      if (nuovo >= LUM_MIN && nuovo <= LUM_MAX) {
        lum = (uint8_t)nuovo;
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
  const uint8_t uscite[] = { PIN_DRL, PIN_ANAB, PIN_ABB, PIN_SPIA };
  for (uint8_t p : uscite) {
    digitalWrite(p, LOW);
    pinMode(p, OUTPUT);
  }
  pinMode(PIN_PULSANTE, INPUT_PULLUP);
  pinMode(PIN_ABB_MOTO, INPUT_PULLUP);

  caricaStato();
  if (!RIPRISTINA_STROBO) stato.strobo = 0;

  delay(RITARDO_AVVIO);           // lascia passare l'avviamento del motore
  avviaLuce(millis());
}

void loop() {
  unsigned long ora = millis();
  gestisciPulsante(ora);
  gestisciAbbMoto(ora);

  if (statoCambiato() && (ora - ultimoCambio) >= RITARDO_SALVA && !inRampa) {
    salvaStato();
  }

  aggiornaUscite(ora);
}
