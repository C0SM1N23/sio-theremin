# SIO Theremin

Theremin digital controlat prin mișcarea mâinilor, realizat cu Arduino UNO pentru laboratorul de SIO-EP.

## Descriere

Instrument muzical digital care se folosește fără atingere. Doi senzori ultrasonici HC-SR04 măsoară distanța față de mâinile utilizatorului: unul controlează frecvența sunetului, iar celălalt intensitatea acestuia. Sunetul este produs de un buzzer pasiv, iar pe un LCD 1602 I²C sunt afișate nota și modul de funcționare. Cu un encoder KY-040 se alege între redarea continuă a frecvențelor și redarea notelor unei game muzicale.

## Obiective

- Citirea senzorilor HC-SR04 cu timere și întreruperi, fără `pulseIn()` sau biblioteci dedicate
- Generarea sunetelor cu buzzerul pasiv folosind registrele Timer2, fără `tone()`
- Comunicarea I²C cu afișajul LCD prin registrele TWI
- Transformarea distanței măsurate în frecvență și intensitate sonoră
- Citirea encoderului pentru selectarea modului de funcționare

## Componente

| Nr. | Denumire | Preț (lei) | Sursă |
|-----|----------|-----------:|-------|
| 1 | 2 × senzori HC-SR04 | 12,98 | Optimus Digital |
| 2 | Buzzer pasiv | 1,45 | SigmaNortec |
| 3 | LCD 1602 I²C | 14,99 | Optimus Digital |
| 4 | Encoder KY-040 | 4,99 | Bitmi |
| 5 | Arduino UNO, breadboard, fire | — | Laborator |
| | **Total periferice** | **34,41** | |
