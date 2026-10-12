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

typedef struct
{
    uint8_t melodia;
    uint8_t bajo;
    uint16_t tiempo;

} PasoMusical;

const PasoMusical cancion1[] PROGMEM =
{
	/*== CANCION 1 IMAGINE==*/
/* primera estrofa vocal */

	{67, 0,  120},
	{ 0, 0,   86},
	{67, 0,  103},
	{ 0, 0,  103},
	{67, 0,  445},
	{ 0, 0,  137},
	{67, 0,  685},
	{ 0, 0,  137},
	{71, 0,  325},
	{ 0, 0,  103},
	{71, 0,  188},
	{69, 0, 1866},
	{ 0, 0, 2243},
	{67, 0,   86},
	{ 0, 0,  103},
	{67, 0,  274},
	{ 0, 0,  154},
	{67, 0,  685},
	{ 0, 0,  137},
	{71, 0,  736},
	{ 0, 0,   86},
	{71, 0,  223},
	{69, 0, 1849},
	{ 0, 0, 2466},
	{67, 0,  240},
	{ 0, 0,  188},
	{67, 0, 1199},
	{71, 0,  308},
	{ 0, 0,  137},
	{71, 0,  188},
	{ 0, 0,   17},
	{69, 0, 1832},
	{ 0, 0, 3065},

	/* siguiente frase */

	{60, 0,  205},
	{ 0, 0,   17},
	{67, 0,  103},
	{ 0, 0,  103},
	{67, 0,  651},
	{71, 0,  308},
	{ 0, 0,   86},
	{71, 0,  291},
	{ 0, 0,  103},
	{71, 0,  171},
	{ 0, 0,   17},
	{69, 0, 1644},
	{ 0, 0, 2038},

	/* continuación */

	{69, 0,  223},
	{72, 0,  274},
	{ 0, 0,   17},
	{69, 0,  308},
	{ 0, 0,   34},
	{72, 0,  856},
	{ 0, 0,  103},
	{76, 0,  342},
	{ 0, 0,  103},
	{76, 0,  428},
	{74, 0,  188},
	{72, 0,  257},
	{69, 0, 1644}
};

/*==CANCION 2 - NEW YORK==*/

const PasoMusical cancion2[] PROGMEM =
{
	/* Frase 1 */

	{62,50,437},
	{0,0,83},
	{62,0,835},
	{59,0,149},
	{0,0,55},
	{59,0,343},
	{57,0,1300},
	{0,0,1360},

	/* Frase 2 */

	{62,50,188},
	{64,0,348},
	{62,0,492},
	{59,0,133},
	{0,0,61},
	{59,0,337},
	{57,0,1803},
	{0,0,537},

	/* Frase 3 */

	{61,57,525},
	{62,0,525},
	{64,0,542},
	{66,0,503},
	{0,0,371},
	{66,0,509},
	{65,0,183},
	{66,0,227},
	{0,0,863},
	{69,57,332},
	{66,0,719},
	{0,0,304},
	{66,0,520},
	{64,0,1803},
	{0,0,542},

	/* Vuelve el motivo conocido */

	{62,50,514},
	{61,0,160},
	{62,0,183},
	{59,0,498},
	{0,0,72},
	{59,0,111},
	{0,0,50},
	{59,0,332},
	{57,0,1764},
	{0,0,890},

	/* Continuación */

	{62,50,188},
	{64,0,348},
	{62,0,503},
	{59,0,138},
	{0,0,72},
	{59,0,337},
	{57,0,1798},
	{0,0,498},

	/* Subida final */

	{61,57,548},
	{62,0,520},
	{64,0,531},
	{66,0,249},
	{0,0,288},
	{65,0,332},
	{66,0,487},
	{65,0,227},
	{66,0,216},
	{0,0,819},
	{69,57,348},
	{66,0,1139},
	{0,0,88},
	{66,0,371},
	{64,0,1228}
};

/*==CANCION 3 - WHOLE NEW WORLD==*/
const PasoMusical cancion3[] PROGMEM =
{
    {67,58,470},
    {0,0,60},
    {65,0,540},
    {0,0,20},
    {60,0,210},
    {0,0,60},
    {63,0,830},
    {62,0,250},
    {0,0,40},
    {62,0,860},
    {0,0,530},
    {62,0,310},
    {0,0,60},
    {63,58,300},
    {0,0,60},
    {65,0,290},
    {0,0,60},
    {69,0,520},
    {0,0,40},
    {67,0,240},
    {0,0,30},
    {65,0,830},
    {0,0,230},
    {58,50,320},
    {69,0,250},
    {0,0,40},
    {70,0,490},
    {0,0,70},
    {65,0,850},
    {0,0,260},
    {58,0,280},
    {62,0,450},
    {0,0,90},
    {60,55,240},
    {0,0,50},
    {58,0,780},
    {0,0,60},
    {58,0,230},
    {0,0,50},
    {63,0,790},
    {0,0,50},
    {62,0,240},
    {0,0,40},
    {60,56,810},
    {0,0,20},
    {58,0,230},
    {0,0,50},
    {58,0,1450},
    {0,0,3290},
    {65,49,430},
    {0,0,120},
    {63,0,250},
    {0,0,40},
    {66,0,520},
    {0,0,30},
    {65,0,240},
    {0,0,30},
    {61,0,240},
    {0,0,40},
    {56,0,1970},
    {0,0,540},
    {65,49,480},
    {0,0,60},
    {63,0,240},
    {0,0,40},
    {66,0,480},
    {0,0,60},
    {65,0,250},
    {0,0,30},
    {61,0,530},
    {0,0,30},
    {65,0,1090},
    {0,0,20},
    {63,48,870},
    {0,0,230},
    {63,0,520},
    {0,0,40},
    {62,0,240},
    {0,0,50},
    {65,0,550},
    {63,53,250},
    {0,0,20},
    {60,0,280},
    {63,0,560},
    {61,0,870},
    {0,0,530},
    {60,56,250},
    {0,0,20},
    {61,0,230},
    {0,0,50},
    {58,0,470},
    {0,0,70},
    {61,0,260},
    {0,0,30},
    {63,0,790},
    {0,0,50},
    {65,0,240},
    {0,0,40},
    {65,54,850},
    {0,0,260},
    {65,0,240},
    {0,0,40},
    {66,0,470},
    {0,0,80},
    {70,0,490},
    {0,0,60},
    {68,56,2700},
    {0,0,340},
    {65,49,260},
    {0,0,20},
    {66,0,30}
};

#define N_CAN1\
(sizeof(cancion1) / sizeof(cancion1[0]))
#define N_CAN2\
(sizeof(cancion2) / sizeof(cancion2[0]))
#define N_CAN3\
(sizeof(cancion3) / sizeof(cancion3[0]))

/*==BUZZER 1 D9 / PB1 / OC1A / TIMER1==*/
void tono1(uint16_t frecuencia)
{
    uint32_t top;
    if (frecuencia == 0)
    {
        TCCR1A = 0;
        TCCR1B = 0;
        PORTB &= ~(1 << PB1);
        return;
    }
    top =
        (F_CPU /
        (2UL * 8UL * frecuencia))
        - 1;

    DDRB |=
        (1 << PB1);

    TCCR1A =
        (1 << COM1A0);

    TCCR1B =
        (1 << WGM12) |
        (1 << CS11);

    OCR1A =
        (uint16_t)top;
}

/*==BUZZER 2 / D11 / PB3 / OC2A / TIMER2==*/
void tono2(uint16_t frecuencia)
{
    uint32_t top;
    if (frecuencia == 0)
    {
        TCCR2A = 0;
        TCCR2B = 0;

        PORTB &= ~(1 << PB3);

        return;
    }

    top =
        (F_CPU /
        (2UL * 256UL * frecuencia))
        - 1;

    if (top > 255)
    {
        top = 255;
    }

    DDRB |=
        (1 << PB3);

    TCCR2A =
        (1 << COM2A0) |
        (1 << WGM21);

    TCCR2B =
        (1 << CS22) |
        (1 << CS21);

    OCR2A =
        (uint8_t)top;
}

/*==SILENCIO==*/
void silencio1(void)
{
    TCCR1A = 0;
    TCCR1B = 0;

    PORTB &= ~(1 << PB1);
}

void silencio2(void)
{
    TCCR2A = 0;
    TCCR2B = 0;

    PORTB &= ~(1 << PB3);
}

void silencio_total(void)
{
    silencio1();
    silencio2();
}

/*==BOTONES==*/
void Botones_init(void)
{
    /*
       D2 a D7 entradas
    */

    DDRD &=
        ~((1 << PD2) |
          (1 << PD3) |
          (1 << PD4) |
          (1 << PD5) |
          (1 << PD6) |
          (1 << PD7));

    /*
       Pull-up interno
    */

    PORTD |=
        (1 << PD2) |
        (1 << PD3) |
        (1 << PD4) |
        (1 << PD5) |
        (1 << PD6) |
        (1 << PD7);

    /*
       A2 y A3 entradas
    */

    DDRC &=
        ~((1 << PC2) |
          (1 << PC3));

    PORTC |=
        (1 << PC2) |
        (1 << PC3);
}

/*==UART==*/
void USART_init(unsigned int ubrr)
{
    UBRR0H =
        (unsigned char)(ubrr >> 8);

    UBRR0L =
        (unsigned char)ubrr;

    UCSR0B =
        (1 << RXEN0) |
        (1 << TXEN0);

    UCSR0C =
        (1 << UCSZ01) |
        (1 << UCSZ00);
}

uint8_t USART_disponible(void)
{
    return
        (UCSR0A & (1 << RXC0));
}

char USART_rx(void)
{
    return UDR0;
}

void USART_tx(char dato)
{
    while (
        !(UCSR0A & (1 << UDRE0))
    );

    UDR0 = dato;
}

void USART_string(const char *texto)
{
    while (*texto)
    {
        USART_tx(
            *texto++
        );
    }
}

/*==LCD==*/
void LCD_modo_piano(void)
{
    twi_lcd_cmd(0x80);

    twi_lcd_msg(
        "MODO PIANO      "
    );

    twi_lcd_cmd(0xC0);

    twi_lcd_msg(
        "ESPERANDO...    "
    );
}

void LCD_nota(const char *nota)
{
    twi_lcd_cmd(0x80);

    twi_lcd_msg(
        "MODO PIANO      "
    );

    twi_lcd_cmd(0xC0);

    twi_lcd_msg(
        "NOTA: "
    );

    twi_lcd_msg(
        (char *)nota
    );

    twi_lcd_msg(
        "         "
    );
}

void LCD_cancion(uint8_t numero)
{
    twi_lcd_cmd(0x80);

    twi_lcd_msg(
        "MODO CANCION    "
    );

    twi_lcd_cmd(0xC0);

    if (numero == 1)
    {
        twi_lcd_msg(
            "IMAGINE        "
        );
    }

    else if (numero == 2)
    {
        twi_lcd_msg(
            "NEW YORK       "
        );
    }

    else
    {
        twi_lcd_msg(
            "WHOLE NEW WORLD"
        );
    }
}

/*==ESPERA DE CANCION - P: detener y volver al piano==*/
void esperar_cancion(uint16_t tiempo)
{
    uint16_t transcurrido = 0;
    while (transcurrido < tiempo)
    {
        if (USART_disponible())
        {
            char comando =
                USART_rx();

            if (
                comando == 'P' ||
                comando == 'p'
            )
            {
                detener_cancion = 1;

                silencio_total();

                return;
            }
        }

        _delay_ms(10);

        transcurrido += 10;
    }
}

/*==TOCAR MEDLODIA + ACOMPAÑAMIENTO==*/
    uint8_t nota_melodia,
    uint8_t nota_bajo,
    uint16_t tiempo
)
{
    uint16_t frecuencia1;
    uint16_t frecuencia2;

    if (detener_cancion)
        return;

    frecuencia1 =
        frecuencia_midi(
            nota_melodia
        );

    frecuencia2 =
        frecuencia_midi(
            nota_bajo
        );

    if (frecuencia1 > 0)
    {
        tono1(
            frecuencia1
        );
    }
    else
    {
        silencio1();
    }

    if (frecuencia2 > 0)
    {
        tono2(
            frecuencia2
        );
    }
    else
    {
        silencio2();
    }

    esperar_cancion(
        tiempo
    );
}

/*==REPRODUCTOR==*/
void reproducir_cancion(
    const PasoMusical *cancion,
    uint16_t cantidad
)
{
    uint16_t i;

    detener_cancion = 0;

    for (i = 0; i < cantidad; i++)
    {
        uint8_t melodia;
        uint8_t bajo;
        uint16_t tiempo;

        melodia =
            pgm_read_byte(
                &cancion[i].melodia
            );

        bajo =
            pgm_read_byte(
                &cancion[i].bajo
            );

        tiempo =
            pgm_read_word(
                &cancion[i].tiempo
            );

        tocar_doble(
            melodia,
            bajo,
            tiempo
        );

        if (detener_cancion)
            break;
    }

    silencio_total();
}

/*==MAIN==*/

int main(void)
{
    uint8_t modo =
        MODO_PIANO;
    uint8_t nota_actual = 0;

    uint8_t nota_anterior = 255;

/*BUZZERS*/

DDRB |=
    (1 << PB1)|
    (1 << PB3);
silencio_total();

/*BOTONES*/

Botones_init();

/*UART*/

    USART_init(
        MYUBRR
    );

/*LCD*/

    twi_init();

    twi_lcd_init();

    _delay_ms(5);

    twi_lcd_clear();

    _delay_ms(5);

    twi_lcd_cmd(0x80);

    twi_lcd_msg(
        "PIANO ELECTRON."
    );

    twi_lcd_cmd(0xC0);

    twi_lcd_msg(
        "INICIANDO..."
    );

    _delay_ms(1200);

    twi_lcd_clear();

    _delay_ms(5);

    LCD_modo_piano();

/*==MENU UART==*/
    USART_string(
        "\r\n"
        "==============================\r\n"
        "       PIANO ELECTRONICO\r\n"
        "==============================\r\n"
        "1 = Imagine\r\n"
        "2 = New York, New York\r\n"
        "3 = A Whole New World\r\n"
        "P = Piano / Stop\r\n"
        "==============================\r\n"
    );

/*==LOOP PRINCIPAL==*/

while (1)
{
/*==UART==*/

        if (USART_disponible())
        {
            char comando =
                USART_rx();

            if (comando == '1')
            {
                modo =
                    MODO_CANCION1;

                silencio_total();

                LCD_cancion(1);
            }

            else if (comando == '2')
            {
                modo =
                    MODO_CANCION2;

                silencio_total();

                LCD_cancion(2);
            }

            else if (comando == '3')
            {
                modo =
                    MODO_CANCION3;

                silencio_total();

                LCD_cancion(3);
            }

            else if (
                comando == 'P' ||
                comando == 'p'
            )
            {
                modo =
                    MODO_PIANO;

                detener_cancion = 1;

                silencio_total();

                nota_anterior = 255;

                LCD_modo_piano();
            }
        }

/*==PIANO==*/

        if (modo == MODO_PIANO)
        {
            /*En modo piano solamente usamos BUZ1*/

            silencio2();
            nota_actual = 0;

            /*DO*/
            if (!(PIND & (1 << PD2)))
            {
                tono1(
                    frecuencia_midi(60)
                );

                nota_actual = 1;

                if (
                    nota_actual !=
                    nota_anterior
                )
                {
                    LCD_nota("DO");

                    nota_anterior =
                        nota_actual;
                }
            }

            /*RE*/

            else if (!(PIND & (1 << PD3)))
            {
                tono1(
                    frecuencia_midi(62)
                );

                nota_actual = 2;

                if (
                    nota_actual !=
                    nota_anterior
                )
                {
                    LCD_nota("RE");

                    nota_anterior =
                        nota_actual;
                }
            }

            /*MI*/

            else if (!(PIND & (1 << PD4)))
            {
                tono1(
                    frecuencia_midi(64)
                );

                nota_actual = 3;

                if (
                    nota_actual !=
                    nota_anterior
                )
                {
                    LCD_nota("MI");

                    nota_anterior =
                        nota_actual;
                }
            }

            /*FA*/

            else if (!(PIND & (1 << PD5)))
            {
                tono1(
                    frecuencia_midi(65)
                );

                nota_actual = 4;

                if (
                    nota_actual !=
                    nota_anterior
                )
                {
                    LCD_nota("FA");

                    nota_anterior =
                        nota_actual;
                }
            }

            /*SOL*/

            else if (!(PIND & (1 << PD6)))
            {
                tono1(
                    frecuencia_midi(67)
                );

                nota_actual = 5;

                if (
                    nota_actual !=
                    nota_anterior
                )
                {
                    LCD_nota("SOL");

                    nota_anterior =
                        nota_actual;
                }
            }

            /*LA*/

            else if (!(PIND & (1 << PD7)))
            {
                tono1(
                    frecuencia_midi(69)
                );

                nota_actual = 6;

                if (
                    nota_actual !=
                    nota_anterior
                )
                {
                    LCD_nota("LA");

                    nota_anterior =
                        nota_actual;
                }
            }

            /*SI*/

            else if (!(PINC & (1 << PC2)))
            {
                tono1(
                    frecuencia_midi(71)
                );

                nota_actual = 7;

                if (
                    nota_actual !=
                    nota_anterior
                )
                {
                    LCD_nota("SI");

                    nota_anterior =
                        nota_actual;
                }
            }

            /*DO AGUDO*/

            else if (!(PINC & (1 << PC3)))
            {
                tono1(
                    frecuencia_midi(72)
                );

                nota_actual = 8;

                if (
                    nota_actual !=
                    nota_anterior
                )
                {
                    LCD_nota(
                        "DO AGUDO"
                    );

                    nota_anterior =
                        nota_actual;
                }
            }

            /*SIN TECLA*/

            else
            {
                silencio1();

                nota_actual = 0;

                if (
                    nota_actual !=
                    nota_anterior
                )
                {
                    LCD_modo_piano();

                    nota_anterior =
                        nota_actual;
                }
            }
        }

/*==CANCION 1 - IMAGINE==*/
        else if (
            modo == MODO_CANCION1
        )
        {
            USART_string(
                "\r\nReproduciendo IMAGINE\r\n"
            );

            reproducir_cancion(
                cancion1,
                N_CAN1
            );

            modo =
                MODO_PIANO;

            detener_cancion = 0;

            nota_anterior = 255;

            LCD_modo_piano();
        }

/*==CANCION 2 - NEW YORK==*/
        else if (
            modo == MODO_CANCION2
        )
        {
            USART_string(
                "\r\nReproduciendo NEW YORK, NEW YORK\r\n"
            );

            reproducir_cancion(
                cancion2,
                N_CAN2
            );

            modo =
                MODO_PIANO;

            detener_cancion = 0;

            nota_anterior = 255;

            LCD_modo_piano();
        }

/*==CANCION 3 - WHOLE NEW WORLD==*/
                    else if (
            modo == MODO_CANCION3
        )
        {
            USART_string(
                "\r\nReproduciendo A WHOLE NEW WORLD\r\n"
            );

            reproducir_cancion(
                cancion3,
                N_CAN3
            );

            modo =
                MODO_PIANO;

            detener_cancion = 0;

            nota_anterior = 255;

            LCD_modo_piano();
        }

        _delay_ms(10);
    }

    return 0;
}

            
