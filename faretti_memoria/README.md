# Centralina faretti moto con memoria

Centralina per **faretti piccoli da moto** che **ricorda l'ultima modalità**
anche dopo aver tolto la chiave. Nasce per sostituire le centraline tipo
**JS1 / JPAD**, che sono alimentate sotto chiave, non hanno batteria tampone e
a ogni accensione ripartono da "spento".

## Come funziona

- L'Arduino è alimentato **sotto chiave**, come la centralina originale. A
  chiave spenta tutto è spento: **consumo zero** e la batteria non si scarica.
- L'ultima modalità scelta viene salvata nella **EEPROM** del microcontrollore,
  una memoria che **non si cancella** quando manca la corrente.
- Alla riaccensione aspetta 1,5 s (per non accendere i faretti durante
  l'avviamento) e poi riaccende i faretti come li avevi lasciati.
- La modalità viene salvata **2 secondi dopo l'ultima modifica**, per non
  consumare la EEPROM, che regge circa 100.000 scritture. Se cambi modalità e
  spegni la moto entro 2 s, quell'ultimo cambio non viene ricordato.

### Pulsante al manubrio

| Azione | Effetto |
|---|---|
| pressione breve | modalità successiva |
| pressione lunga (> 0,8 s) | spegne i faretti |

Modalità predefinite: `spenti → canale A → canale B → A+B → spenti…`
(per esempio A = bianchi e B = gialli, oppure due coppie di faretti).
Con **una sola coppia** di faretti, in `faretti_memoria.ino` lascia nell'array
`MODALITA` solo le prime due righe: `{0, 0}` e `{1, 0}`.

## Materiale (circa 10–15 €)

| Componente | Note |
|---|---|
| Arduino Nano (o Pro Mini 5V) | ATmega328P |
| Convertitore step-down 12V → 5V | mini-360 / LM2596, regolato a 5,0 V |
| Modulo relè 2 canali 5V con optoisolatore | oppure 1 canale se hai una sola coppia di faretti |
| Pulsante NA (normalmente aperto) da manubrio | puoi riusare quello della centralina JS1 |
| 2 portafusibili + fusibili | 1 A per l'elettronica, 10 A per i faretti |
| Diodo TVS SMBJ18A *(consigliato)* | protegge dai picchi dell'impianto della moto |
| Scatoletta stagna, guaina termorestringente | |

## Collegamenti

```
                         FUSIBILE 1A
 + SOTTO CHIAVE ─────────[===]──────┬──── IN+  step-down  OUT+ ──── 5V   Arduino
                                    │                                      │
                                 TVS SMBJ18A                               ├── VCC modulo relè
                                    │                                      │
 MASSA ─────────────────────────────┴──── IN-  step-down  OUT- ──── GND  Arduino
                                                                           └── GND modulo relè

 Arduino D9  ──── IN1 modulo relè   (canale A)
 Arduino D10 ──── IN2 modulo relè   (canale B)
 Arduino D2  ──── pulsante ──── GND (usa la resistenza di pull-up interna)

                         FUSIBILE 10A
 + BATTERIA ─────────────[===]──────┬──── COM relè 1 ── NO relè 1 ──── + faretti A
                                    └──── COM relè 2 ── NO relè 2 ──── + faretti B
 - faretti ──── MASSA
```

**Note importanti**

1. **La corrente dei faretti passa solo dai relè**, mai dall'Arduino.
2. Se i relè sono sotto chiave, il + diretto della batteria sui contatti non
   crea problemi: a chiave spenta i relè sono aperti e i faretti spenti.
3. Se il modulo relè si attiva con **HIGH**, o se usi MOSFET o transistor,
   imposta `RELE_ATTIVO_BASSO = false` nel codice.
4. **Prima di collegare l'Arduino**, regola lo step-down a 5,0 V con il tester.
5. Monta l'elettronica in una scatola stagna, lontano da motore e scarico.

## Caricare il programma

1. Installa l'[Arduino IDE](https://www.arduino.cc/en/software).
2. Apri `faretti_memoria/faretti_memoria.ino`.
3. Scegli la scheda *Arduino Nano* (processore *ATmega328P*, oppure
   *ATmega328P (Old Bootloader)* sui cloni economici) e la porta USB giusta.
4. Clicca su **Carica**.

## Alternativa: tenere la centralina JS1 originale

Se preferisci non sostituirla, puoi affiancarle l'Arduino. All'accensione
"preme" lui i tasti della JS1 attraverso degli optoisolatori PC817 collegati in
parallelo ai pulsanti, così la riporta all'ultima modalità. È più complicato da
cablare e dipende da come è fatta la tastiera della JS1, per questo la
soluzione consigliata è quella descritta sopra.

## Cosa NON fare

Non collegare la centralina originale al **+ permanente** per farle tenere la
memoria. Resterebbe sempre accesa e, con i faretti accesi, potrebbe rimanere
accesa anche a moto spenta: le batterie delle moto sono piccole e si
scaricherebbero in pochi giorni.
