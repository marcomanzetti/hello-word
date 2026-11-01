# Centralina faretti moto con memoria

Centralina per **faretti piccoli da moto a 3 funzioni: DRL, anabbagliante e
abbagliante**. **Ricorda tutto anche dopo aver tolto la chiave**: se i faretti
erano accesi, quale luce, lo strobo e la luminosità di ogni luce.
Sostituisce le centraline tipo **JS1 / JPAD**, che sono alimentate sotto
chiave, non hanno batteria tampone e a ogni accensione ripartono da "spento".

| Schema elettrico | Render |
|---|---|
| ![Schema elettrico](schema.png) | ![Render](render.png) |

I file vettoriali `schema.svg` e `render.svg` si possono ingrandire senza
perdere qualità.

## Funzioni

- Accensione e spegnimento.
- Scelta della luce: **DRL → anabbagliante → abbagliante**.
- **Luminosità regolabile** (PWM), memorizzata **separatamente per ogni luce**:
  per esempio DRL al 30 % e anabbagliante al 100 %.
- **Strobo** (doppio lampo, la sequenza si modifica nel codice).
- **Abbagliante sincronizzato con la moto** (facoltativo). Quando accendi
  l'abbagliante della moto o fai il lampeggio, i faretti accesi passano in
  abbagliante al 100 %. Quando lo spegni tornano come prima.
- **Memoria**: l'Arduino è alimentato sotto chiave, quindi a moto spenta
  consuma zero, e lo stato viene salvato nella **EEPROM**, una memoria che non
  si cancella quando manca la corrente.
- Accensione ritardata di 1,5 s, per non accendere i faretti durante
  l'avviamento, e accensione morbida in 0,3 s.

## Comandi (un solo pulsante al manubrio)

| Azione | Effetto |
|---|---|
| **1 click** | accende / spegne (riparte dall'ultima luce usata) |
| **2 click** | luce successiva: DRL → anabbagliante → abbagliante |
| **3 click** | strobo sì / no |
| **tieni premuto** | luminosità della luce attuale: rilascia quando ti piace. Alla pressione successiva la regolazione va nel verso opposto |

Lo stato viene salvato **2 secondi dopo l'ultima modifica**. Se cambi qualcosa
e togli la chiave entro 2 s, quell'ultima modifica non viene ricordata.

### Impostazioni nel codice (`faretti_memoria.ino`)

| Costante | A cosa serve |
|---|---|
| `DRL_CON_FARI` | `true` = il DRL resta acceso anche in anabbagliante e abbagliante |
| `ANAB_CON_ABB` | `true` = l'anabbagliante resta acceso insieme all'abbagliante |
| `USA_ABB_MOTO` | `false` = ignora l'ingresso dell'abbagliante della moto |
| `LUM_MIN` | luminosità minima |
| `RAMPA_MS` | velocità della regolazione |
| `SEQUENZA_STROBO` | tempi del lampeggio in ms (acceso, spento, acceso…) |
| `RIPRISTINA_STROBO` | `false` = alla riaccensione lo strobo riparte spento |

## Componenti

### Base

| Q.tà | Componente | Sigla |
|---|---|---|
| 1 | Arduino Nano (ATmega328P) | U2 |
| 1 | Step-down 12 V → 5 V (mini-360 o LM2596) | U1 |
| 1 | Diodo Schottky SS34 o 1N5819 | D1 |
| 1 | Diodo TVS SMBJ18A (o P6KE18A) | D2 |
| 1 | Condensatore elettrolitico 100 µF 25 V | C1 |
| 1 | Condensatore ceramico 100 nF | C2 |
| 2 | Portafusibili a lama + fusibili 1 A e 15 A | F1, F2 |
| 1 | Pulsante NA da manubrio (va bene quello della JS1) | SW1 |
| 1 | LED spia + resistenza 1 kΩ *(facoltativo)* | LED1, R13 |
| 1 | Basetta millefori ~100×80 mm | |
| 4 | Morsettiere a vite passo 5 mm (2 da 2 poli, 1 da 4, 1 da 5) | |
| 1 | Scatola stagna IP65 + 4 pressacavi PG7 | |
| — | Cavo 1,5 mm² (potenza) e 0,5 mm² (segnali), guaina termorestringente | |

### Stadi di potenza (×3: DRL, anabbagliante, abbagliante)

| Q.tà per canale | Componente | Sigle DRL / ANAB / ABB |
|---|---|---|
| 1 | MOSFET P-channel **IRF4905** (TO-220) + dissipatore | Q1 / Q2 / Q3 |
| 1 | Transistor NPN **BC547** (o 2N2222) | T1 / T2 / T3 |
| 1 | Resistenza 1 kΩ (base) | R1 / R5 / R9 |
| 1 | Resistenza 1 kΩ (gate–source) | R2 / R6 / R10 |
| 1 | Resistenza 470 Ω (collettore) | R3 / R7 / R11 |
| 1 | Resistenza 10 kΩ (base–massa) | R4 / R8 / R12 |
| 1 | Diodo Zener 15 V (BZX55C15) | DZ1 / DZ2 / DZ3 |

### Ingresso abbagliante moto (facoltativo)

| Q.tà | Componente | Sigla |
|---|---|---|
| 1 | Optoisolatore **PC817** | U3 |
| 1 | Resistenza 2,2 kΩ | R14 |
| 1 | Diodo 1N4148 | D3 |

Resistenze da 1/4 W. Spesa totale indicativa: **35–45 €**.

## Collegamenti

Vedi `schema.svg`. In breve:

1. **+12 V sotto chiave** → F1 1 A → D1 → step-down → **5 V dell'Arduino**.
   Il TVS e C1 vanno tra l'ingresso dello step-down e la massa.
2. **+ batteria diretto** → F2 15 A → **source** di Q1, Q2 e Q3 (piedino
   destro dell'IRF4905, guardandolo dal lato scritto).
3. **Drain** (piedino centrale): Q1 → + DRL, Q2 → + anabbagliante,
   Q3 → + abbagliante dei faretti.
4. **Negativo dei faretti** → massa (telaio o − batteria).
5. Pulsante tra **D2** e massa. Uscite **D6** (DRL), **D9** (anabbagliante) e
   **D10** (abbagliante) verso le resistenze di base R1 / R5 / R9.
6. *(facoltativo)* **Filo dell'abbagliante della moto** → R14 2,2 kΩ → PC817.
   L'uscita del PC817 va su **D4**. L'optoisolatore separa l'Arduino
   dall'impianto della moto.

Perché i relè non vanno bene: un relè non regge lo strobo (si consuma in poco
tempo) e non può regolare la luminosità. I MOSFET IRF4905 commutano il
**positivo** dei faretti, quindi funzionano anche se la carcassa del faretto è
a massa.

**Prima di collegare l'Arduino**, regola lo step-down a **5,0 V** con il tester.

**Fili dei faretti**: di solito sono 4 (DRL, anabbagliante, abbagliante,
massa), ma i colori cambiano da modello a modello. Prima di collegarli
verifica quale filo accende quale luce con una batteria da 12 V e un fusibile.

## Caricare il programma

1. Installa l'[Arduino IDE](https://www.arduino.cc/en/software).
2. Apri `faretti_memoria/faretti_memoria.ino`.
3. Scegli la scheda *Arduino Nano* (processore *ATmega328P*, oppure
   *ATmega328P (Old Bootloader)* sui cloni economici) e la porta USB giusta.
4. Clicca su **Carica**.

## Avvertenze

- Su strada pubblica lo **strobo e i faretti non omologati non sono ammessi**
  dal Codice della Strada. Usa lo strobo solo dove è consentito.
- Non collegare la centralina al **+ permanente**: resterebbe sempre accesa e
  scaricherebbe la batteria della moto.
- Monta la scatola lontano da motore e scarico. Fissa bene i dissipatori e
  proteggi la scheda con vernice protettiva (conformal coating).
