# Conexiuni – Theremin SIO (Arduino UNO R4 WiFi)

## Toți pinii pe scurt

| Pin Arduino | Se leagă la | Observații |
|---|---|---|
| D0, D1 | – | liberi (Serial1) |
| D2 | HC-SR04 stânga – ECHO | pin cu întrerupere |
| D3 | HC-SR04 dreapta – ECHO | pin cu întrerupere |
| D4 | HC-SR04 stânga – TRIG | |
| D5 | HC-SR04 dreapta – TRIG | |
| D6 | Matrice WS2812B – DIN | prin rezistor 330 Ω |
| D7 | Encoder 1 – SW (buton) | |
| D8 | Encoder 1 – CLK | pin cu întrerupere |
| D9 | Encoder 1 – DT | pin cu întrerupere |
| D10 | – | liber |
| D11 | Buzzer pasiv + | prin rezistor 1 kΩ; deocamdată de aici vine sunetul theremin-ului |
| D12, D13 | – | liberi (D13 are LED-ul de pe placă) |
| A0 (DAC) | PAM8403 – intrare L | mai târziu, prin divizor 10 kΩ / 1 kΩ; deocamdată liber |
| A1 | Encoder 2 – CLK | pin cu întrerupere |
| A2 | Encoder 2 – DT | pin cu întrerupere |
| A3 | Encoder 2 – SW (buton) | |
| A4 / SDA | LCD – SDA | I²C |
| A5 / SCL | LCD – SCL | I²C |
| 5V | linia + de pe breadboard | alimentează toate perifericele |
| GND | linia − de pe breadboard | masă comună pentru tot |

**De ce pinii aceștia:**
- **D11** are un timer PWM în spate, care poate schimba atât frecvența (nota), cât și factorul de umplere (volumul) semnalului pentru buzzer.
- **A0** este singurul pin cu DAC (convertor digital-analogic); rămâne pentru amplificator și difuzor, mai târziu.
- **A4/A5** sunt pinii I²C ai plăcii (aceiași cu pinii SDA/SCL de lângă AREF).
- Senzorii și encoderele au nevoie de pini care pot genera întreruperi. Pe R4 WiFi unii pini își împart același canal de întrerupere (D3 cu D4 și D5, D9 cu D12 și D1, D2 cu D13), deci o singură întrerupere pe fiecare grup. Planul folosește canale diferite: D2, D3, D8, D9, A1, A2.
- Senzorii păstrează pinii din schema bloc inițială (TRIG D4/D5, ECHO D2/D3).

## Alimentare

- Pe breadboard: **5V** de pe Arduino pe linia **+**, **GND** pe linia **−**. Toate perifericele iau 5V și GND de aici.
- Un pin al R4 WiFi suportă cel mult **8 mA**. Nu legați nimic care consumă mult direct pe un pin; de aceea buzzerul are rezistor de 1 kΩ.
- **Matricea WS2812B** poate cere până la ~3,8 A (64 LED-uri pe alb, la maxim). Alimentată din USB, țineți luminozitatea mică din cod (`setBrightness(20)` sau mai puțin). Pentru varianta finală folosiți o sursă externă de 5 V / 3 A, cu GND-ul legat la GND-ul Arduino. Puneți un condensator de 1000 µF între 5V și GND, lângă matrice (piciorul − la GND).
- **PAM8403** la volum mare consumă și el mult. Pe USB, țineți volumul moderat.

## Pe componente

### 1. Senzori ultrasonici HC-SR04 (×2)

| HC-SR04 stânga (frecvență) | Arduino |
|---|---|
| VCC | 5V |
| Trig | D4 |
| Echo | D2 |
| GND | GND |

| HC-SR04 dreapta (volum) | Arduino |
|---|---|
| VCC | 5V |
| Trig | D5 |
| Echo | D3 |
| GND | GND |

Puneți senzorii cu fața în sus, cât mai departe unul de altul (fiecare la un capăt al breadboard-ului sau pe fire mai lungi), ca mâna stângă să nu fie văzută și de senzorul din dreapta.

### 2. Buzzer pasiv (sunetul, deocamdată)

| Buzzer | Arduino |
|---|---|
| + (piciorul lung / marcat +) | rezistor 1 kΩ → D11 |
| − | GND |

- Dacă este un modul cu 3 pini: **S** → rezistor 1 kΩ → D11, **−** → GND, pinul din mijloc → 5V doar dacă e marcat VCC/+, altfel rămâne liber.
- Rezistorul ține curentul sub limita de 8 mA a pinului. Dacă sunetul e prea încet și aveți un tranzistor NPN (BC547, 2N2222): D11 → rezistor 1 kΩ → baza; emitorul → GND; colectorul → buzzer −; buzzer + → 5V.

### 3. LCD 1602 cu modul I²C

| Modul I²C (pe spatele LCD-ului) | Arduino |
|---|---|
| GND | GND |
| VCC | 5V |
| SDA | A4 (SDA) |
| SCL | A5 (SCL) |

- Adresa modulului este de obicei **0x27** sau **0x3F**. `test_lcd` o caută și o scrie în Serial Monitor.
- Dacă ecranul e aprins dar nu apare text, rotiți potențiometrul albastru de pe modul (contrastul).
- Jumper-ul de pe modul (lumina de fundal) trebuie să rămână pus.

### 4. Matrice LED RGB WS2812B 8×8

| Matrice (partea cu **DIN**) | Arduino |
|---|---|
| DIN | rezistor 330 Ω → D6 |
| 5V (VCC) | 5V (sau sursa externă) |
| GND | GND |

Matricea are două capete: **DIN** (intrare) și **DOUT** (ieșire). Legați-o pe partea **DIN**; săgețile de pe placă arată sensul datelor.

### 5. Encodere rotative (×2)

Encoder 1 – navigare prin meniu / schimbarea modului:

| Encoder 1 (KY-040) | Arduino |
|---|---|
| CLK | D8 |
| DT | D9 |
| SW | D7 |
| + | 5V |
| GND | GND |

Encoder 2 – reglarea valorii selectate (octavă, gamă, luminozitate etc.):

| Encoder 2 | Arduino |
|---|---|
| CLK | A1 |
| DT | A2 |
| SW | A3 |
| + | 5V |
| GND | GND |

Dacă al doilea encoder nu e KY-040:
- **Alt modul, cu alte etichete:** A / S1 / CLK → A1, B / S2 / DT → A2, KEY / SW → A3.
- **Encoder simplu, fără plăcuță** (3 picioare pe o parte, 2 pe cealaltă): piciorul din mijloc al celor 3 → GND, cele din margini → A1 și A2. Dintre cele 2 picioare ale butonului, unul → A3, celălalt → GND. Codul folosește rezistențele interne de pull-up (`INPUT_PULLUP`), deci nu trebuie rezistoare în plus.
- Dacă se rotește invers, inversați firele de la CLK și DT.

### 6. Sunet prin difuzor: DAC → amplificator PAM8403 → difuzor (mai târziu)

| De la | La |
|---|---|
| A0 | rezistor 10 kΩ → PAM8403 intrarea **L** |
| PAM8403 intrarea **L** | rezistor 1 kΩ → GND |
| PAM8403 masa intrării (**G** / ⊥, între L și R) | GND |
| PAM8403 **5V** (VCC) | 5V |
| PAM8403 **GND** alimentare | GND |
| Difuzor fir 1 | PAM8403 **L+** |
| Difuzor fir 2 | PAM8403 **L−** |

- Cele două rezistoare formează un divizor: semnalul DAC (0–5 V) ajunge de ~11 ori mai mic, cât îi trebuie amplificatorului.
- Ieșirile **L+ / L−** nu se leagă niciodată la GND, nici una dintre ele. Difuzorul stă doar între ele.
- Intrarea **R** rămâne liberă.

## Ordinea montajului și testele

Montați și testați câte un periferic pe rând. În VS Code, alegeți programul din bara de jos a PlatformIO (`env:...`), apoi apăsați Upload (→) și deschideți Serial Monitor (ștecherul).

| Pas | Ce montați | Program (env) | Ce trebuie să vedeți / auziți |
|---|---|---|---|
| 1 | Cei doi senzori HC-SR04 | `test_hcsr04` | Distanța fiecărei mâini în Serial Monitor |
| 2 | Buzzerul pasiv | `test_buzzer` | Gama Do major, o glisare, apoi aceeași notă tot mai încet |
| 3 | LCD-ul | `test_lcd` | „Theremin SIO” și secundele; adresa în Serial Monitor |
| 4 | Toate trei împreună | `theremin` | Theremin-ul funcțional: notă, frecvență și volum pe LCD |
| 5 | Encodere, matrice, amplificator + difuzor | urmează | |
