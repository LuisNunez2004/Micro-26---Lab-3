#define F_CPU 16000000UL
#define BAUD 9600
#define MYUBRR ((F_CPU / 16UL / BAUD) - 1)
#define PCF8574 0x27
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdint.h>
#include "twi_lcd.h"

/*==MODOS*/
#define MODO_PIANO     0
#define MODO_CANCION1  1
#define MODO_CANCION2  2
#define MODO_CANCION3  3

volatile uint8_t detener_cancion = 0;

/*==FRECUENCIAS MIDI 48 A 79==*/
const uint16_t tabla_frecuencias[] PROGMEM =
{
    131,    // 48 C3
    139,    // 49 C#3
    147,    // 50 D3
    156,    // 51 D#3
    165,    // 52 E3
    175,    // 53 F3
    185,    // 54 F#3
    196,    // 55 G3
    208,    // 56 G#3
    220,    // 57 A3
    233,    // 58 A#3
    247,    // 59 B3

    262,    // 60 C4
    277,    // 61 C#4
    294,    // 62 D4
    311,    // 63 D#4
    330,    // 64 E4
    349,    // 65 F4
    370,    // 66 F#4
    392,    // 67 G4
    415,    // 68 G#4
    440,    // 69 A4
    466,    // 70 A#4
    494,    // 71 B4

    523,    // 72 C5
    554,    // 73 C#5
    587,    // 74 D5
    622,    // 75 D#5
    659,    // 76 E5
    698,    // 77 F5
    740,    // 78 F#5
    784     // 79 G5
};


uint16_t frecuencia_midi(uint8_t nota)
{
    if (nota == 0)
        return 0;

    if (nota < 48 || nota > 79)
        return 0;

    return pgm_read_word(
        &tabla_frecuencias[nota - 48]
    );
}

/*==ESTRUCTURA MUSICAL
melodia: BUZ1 D9
bajo: BUZ2 D11
tiempo: milisegundos==*/


