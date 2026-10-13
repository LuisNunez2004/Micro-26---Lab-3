#define F_CPU 16000000UL
#define BAUD 9600
#define MYUBRR (F_CPU / 16 / BAUD - 1)

/* Direccion I2C del LCD */
#define PCF8574 0x27

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "twi_lcd.h"


/* =========================================================
   PROBLEMA C - CLASIFICADOR DE COLOR RGB

   METODO:
   Medicion por reflectancia

   LED RGB CATODO COMUN:
   Rojo  -> D3 / PD3
   Verde -> D4 / PD4
   Azul  -> D5 / PD5
   Comun -> GND

   Fotocelda -> A0 / PC0 / ADC0

   Servo -> D9 / PB1 / OC1A

   LCD I2C:
   SDA -> A4 / PC4
   SCL -> A5 / PC5

   UART -> 9600 baud
   ========================================================= */


/* =========================================================
   PINES LED RGB
   ========================================================= */

#define LED_R PD3
#define LED_G PD4
#define LED_B PD5


/* =========================================================
   ANGULOS DEL SERVO
   ========================================================= */

#define ANG_VERDE      25
#define ANG_NARANJA    65
#define ANG_ESPERA     90
#define ANG_VIOLETA   115
#define ANG_BLANCO    155


/* =========================================================
   PATRONES RGB DEFINITIVOS

   Valores obtenidos con el montaje final
   ========================================================= */

#define R_SIN_OBJETO   24
#define G_SIN_OBJETO   24
#define B_SIN_OBJETO   20

#define R_VIOLETA      25
#define G_VIOLETA      31
#define B_VIOLETA      47

#define R_VERDE        24
#define G_VERDE        48
#define B_VERDE        22

#define R_NARANJA      56
#define G_NARANJA      30
#define B_NARANJA      25

#define R_BLANCO       54
#define G_BLANCO       78
#define B_BLANCO       66


/* =========================================================
   LED RGB
   CATODO COMUN

   HIGH = ENCENDIDO
   LOW  = APAGADO
   ========================================================= */

void RGB_init(void)
{
    DDRD |=
        (1 << LED_R) |
        (1 << LED_G) |
        (1 << LED_B);

    /* Inicialmente apagado */

    PORTD &=
        ~(
            (1 << LED_R) |
            (1 << LED_G) |
            (1 << LED_B)
        );
}


/* =========================================================
   APAGAR LED RGB
   ========================================================= */

void RGB_apagar(void)
{
    PORTD &=
        ~(
            (1 << LED_R) |
            (1 << LED_G) |
            (1 << LED_B)
        );
}


/* =========================================================
   ENCENDER SOLO ROJO
   ========================================================= */

void RGB_rojo(void)
{
    RGB_apagar();

    _delay_ms(100);

    PORTD |=
        (1 << LED_R);
}


/* =========================================================
   ENCENDER SOLO VERDE
   ========================================================= */

void RGB_verde(void)
{
    RGB_apagar();

    _delay_ms(100);

    PORTD |=
        (1 << LED_G);
}


/* =========================================================
   ENCENDER SOLO AZUL
   ========================================================= */

void RGB_azul(void)
{
    RGB_apagar();

    _delay_ms(100);

    PORTD |=
        (1 << LED_B);
}


/* =========================================================
   ADC
   Fotocelda -> A0 / ADC0
   ========================================================= */

void ADC_init(void)
{
    /* Referencia AVcc */

    ADMUX =
        (1 << REFS0);


    /*
       ADC habilitado
       Prescaler = 128

       16 MHz / 128 = 125 kHz
    */

    ADCSRA =
        (1 << ADEN)  |
        (1 << ADPS2) |
        (1 << ADPS1) |
        (1 << ADPS0);
}


/* =========================================================
   LEER ADC
   ========================================================= */

uint16_t ADC_read(uint8_t canal)
{
    ADMUX =
        (ADMUX & 0xF0) |
        (canal & 0x0F);


    /* Iniciar conversion */

    ADCSRA |=
        (1 << ADSC);


    /* Esperar fin */

    while (
        ADCSRA &
        (1 << ADSC)
    );


    return ADC;
}


/* =========================================================
   PROMEDIO DE 10 LECTURAS
   ========================================================= */

uint16_t ADC_promedio(void)
{
    uint32_t suma = 0;


    for (uint8_t i = 0; i < 10; i++)
    {
        suma +=
            ADC_read(0);

        _delay_ms(10);
    }


    return
        (uint16_t)(
            suma / 10
        );
}


/* =========================================================
   UART
   ========================================================= */

void USART_init(unsigned int ubrr)
{
    UBRR0H =
        (unsigned char)(
            ubrr >> 8
        );


    UBRR0L =
        (unsigned char)ubrr;


    /* Habilitar transmisor */

    UCSR0B =
        (1 << TXEN0);


    /*
       8 bits
       sin paridad
       1 bit stop
    */

    UCSR0C =
        (1 << UCSZ01) |
        (1 << UCSZ00);
}


/* =========================================================
   ENVIAR CARACTER UART
   ========================================================= */

void USART_tx(char dato)
{
    while (
        !(UCSR0A &
        (1 << UDRE0))
    );


    UDR0 = dato;
}


/* =========================================================
   ENVIAR TEXTO UART
   ========================================================= */

void USART_string(const char *texto)
{
    while (*texto)
    {
        USART_tx(
            *texto++
        );
    }
}


/* =========================================================
   ENVIAR NUMERO UART
   ========================================================= */

void USART_numero(uint16_t numero)
{
    char texto[10];


    itoa(
        numero,
        texto,
        10
    );


    USART_string(
        texto
    );
}


/* =========================================================
   SERVOMOTOR
   D9 / PB1 / OC1A
   ========================================================= */

void Servo_init(void)
{
    /* D9 como salida */

    DDRB |=
        (1 << PB1);


    /*
       Timer1
       Fast PWM modo 14
       TOP = ICR1
       Prescaler = 8
    */

    TCCR1A =
        (1 << COM1A1) |
        (1 << WGM11);


    TCCR1B =
        (1 << WGM13) |
        (1 << WGM12) |
        (1 << CS11);


    /*
       Periodo = 20 ms
       Frecuencia = 50 Hz
    */

    ICR1 = 39999;
}


/* =========================================================
   POSICIONAR SERVO
   ========================================================= */

void Servo_angle(uint8_t angulo)
{
    uint16_t pulso;


    pulso =
        1000 +
        ((uint32_t)angulo * 4000UL / 180UL);


    OCR1A =
        pulso;
}


/* =========================================================
   MEDIR CON ROJO
   ========================================================= */

uint16_t medir_rojo(void)
{
    uint16_t valor;


    RGB_apagar();

    _delay_ms(100);


    RGB_rojo();

    /*
       Esperar estabilizacion
       de la fotocelda
    */

    _delay_ms(500);


    valor =
        ADC_promedio();


    RGB_apagar();

    _delay_ms(100);


    return valor;
}


/* =========================================================
   MEDIR CON VERDE
   ========================================================= */

uint16_t medir_verde(void)
{
    uint16_t valor;


    RGB_apagar();

    _delay_ms(100);


    RGB_verde();

    _delay_ms(500);


    valor =
        ADC_promedio();


    RGB_apagar();

    _delay_ms(100);


    return valor;
}


/* =========================================================
   MEDIR CON AZUL
   ========================================================= */

uint16_t medir_azul(void)
{
    uint16_t valor;


    RGB_apagar();

    _delay_ms(100);


    RGB_azul();

    _delay_ms(500);


    valor =
        ADC_promedio();


    RGB_apagar();

    _delay_ms(100);


    return valor;
}


/* =========================================================
   DIFERENCIA ABSOLUTA
   ========================================================= */

uint16_t diferencia(uint16_t valor,
                    uint16_t referencia)
{
    if (valor >= referencia)
    {
        return
            valor - referencia;
    }
    else
    {
        return
            referencia - valor;
    }
}


/* =========================================================
   DISTANCIA RGB

   Diferencia total:

   |R-Rref| + |G-Gref| + |B-Bref|

   El patron con menor distancia
   corresponde al color detectado.
   ========================================================= */

uint16_t distancia_RGB(uint16_t r,
                       uint16_t g,
                       uint16_t b,
                       uint16_t refR,
                       uint16_t refG,
                       uint16_t refB)
{
    return
        diferencia(r, refR) +
        diferencia(g, refG) +
        diferencia(b, refB);
}


/* =========================================================
   MOSTRAR PRIMERA PANTALLA LCD

   Color
   Angulo
   Lecturas RGB
   ========================================================= */

void LCD_medicion(char *color,
                  uint8_t angulo,
                  uint16_t r,
                  uint16_t g,
                  uint16_t b)
{
    char textoR[8];
    char textoG[8];
    char textoB[8];
    char textoAngulo[8];


    itoa(r, textoR, 10);
    itoa(g, textoG, 10);
    itoa(b, textoB, 10);
    itoa(angulo, textoAngulo, 10);


    /* Limpiar linea 1 */

    twi_lcd_cmd(0x80);

    twi_lcd_msg(
        "                "
    );


    /* COLOR + ANGULO */

    twi_lcd_cmd(0x80);

    twi_lcd_msg(
        color
    );

    twi_lcd_msg(
        " A:"
    );

    twi_lcd_msg(
        textoAngulo
    );


    /* Limpiar linea 2 */

    twi_lcd_cmd(0xC0);

    twi_lcd_msg(
        "                "
    );


    /* Valores RGB */

    twi_lcd_cmd(0xC0);

    twi_lcd_msg(
        "R"
    );

    twi_lcd_msg(
        textoR
    );

    twi_lcd_msg(
        " G"
    );

    twi_lcd_msg(
        textoG
    );

    twi_lcd_msg(
        " B"
    );

    twi_lcd_msg(
        textoB
    );
}


/* =========================================================
   MOSTRAR REFERENCIA Y DIFERENCIA LCD
   ========================================================= */

void LCD_referencia(uint16_t refR,
                    uint16_t refG,
                    uint16_t refB,
                    uint16_t distancia)
{
    char textoR[8];
    char textoG[8];
    char textoB[8];
    char textoDif[8];


    itoa(refR, textoR, 10);
    itoa(refG, textoG, 10);
    itoa(refB, textoB, 10);
    itoa(distancia, textoDif, 10);


    /* Linea 1 */

    twi_lcd_cmd(0x80);

    twi_lcd_msg(
        "                "
    );


    twi_lcd_cmd(0x80);

    twi_lcd_msg(
        "REF "
    );

    twi_lcd_msg(
        textoR
    );

    twi_lcd_msg(
        " "
    );

    twi_lcd_msg(
        textoG
    );

    twi_lcd_msg(
        " "
    );

    twi_lcd_msg(
        textoB
    );


    /* Linea 2 */

    twi_lcd_cmd(0xC0);

    twi_lcd_msg(
        "                "
    );


    twi_lcd_cmd(0xC0);

    twi_lcd_msg(
        "DIF:"
    );

    twi_lcd_msg(
        textoDif
    );
}


/* =========================================================
   PROGRAMA PRINCIPAL
   ========================================================= */

int main(void)
{
    uint16_t adcR;
    uint16_t adcG;
    uint16_t adcB;


    uint16_t dist_sin_objeto;
    uint16_t dist_violeta;
    uint16_t dist_verde;
    uint16_t dist_naranja;
    uint16_t dist_blanco;


    uint16_t menor_distancia;


    uint16_t refR;
    uint16_t refG;
    uint16_t refB;


    uint8_t angulo;


    char color[12];


    /* =====================================================
       INICIALIZAR RGB
       ===================================================== */

    RGB_init();


    /* =====================================================
       INICIALIZAR ADC
       ===================================================== */

    ADC_init();


    /* =====================================================
       INICIALIZAR UART
       ===================================================== */

    USART_init(
        MYUBRR
    );


    /* =====================================================
       INICIALIZAR SERVO
       ===================================================== */

    Servo_init();


    Servo_angle(
        ANG_ESPERA
    );


    /* =====================================================
       INICIALIZAR LCD
       ===================================================== */

    twi_init();

    twi_lcd_init();

    _delay_ms(5);


    twi_lcd_clear();

    _delay_ms(5);


    /* =====================================================
       PANTALLA INICIAL
       ===================================================== */

    twi_lcd_cmd(
        0x80
    );


    twi_lcd_msg(
        "PROBLEMA C RGB"
    );


    twi_lcd_cmd(
        0xC0
    );


    twi_lcd_msg(
        "INICIANDO..."
    );


    /* =====================================================
       UART
       ===================================================== */

    USART_string(
        "\r\n"
    );


    USART_string(
        "========================================\r\n"
    );


    USART_string(
        "PROBLEMA C - CLASIFICADOR RGB\r\n"
    );


    USART_string(
        "MEDICION POR REFLECTANCIA\r\n"
    );


    USART_string(
        "========================================\r\n"
    );


    _delay_ms(
        1500
    );


    twi_lcd_clear();


    /* =====================================================
       BUCLE PRINCIPAL
       ===================================================== */

    while (1)
    {
        /* =================================================
           MEDIR R
           ================================================= */

        adcR =
            medir_rojo();


        /* =================================================
           MEDIR G
           ================================================= */

        adcG =
            medir_verde();


        /* =================================================
           MEDIR B
           ================================================= */

        adcB =
            medir_azul();


        /* =================================================
           CALCULAR DISTANCIAS
           ================================================= */

        dist_sin_objeto =
            distancia_RGB(
                adcR,
                adcG,
                adcB,

                R_SIN_OBJETO,
                G_SIN_OBJETO,
                B_SIN_OBJETO
            );


        dist_violeta =
            distancia_RGB(
                adcR,
                adcG,
                adcB,

                R_VIOLETA,
                G_VIOLETA,
                B_VIOLETA
            );


        dist_verde =
            distancia_RGB(
                adcR,
                adcG,
                adcB,

                R_VERDE,
                G_VERDE,
                B_VERDE
            );


        dist_naranja =
            distancia_RGB(
                adcR,
                adcG,
                adcB,

                R_NARANJA,
                G_NARANJA,
                B_NARANJA
            );


        dist_blanco =
            distancia_RGB(
                adcR,
                adcG,
                adcB,

                R_BLANCO,
                G_BLANCO,
                B_BLANCO
            );


        /* =================================================
           POR DEFECTO:
           SIN OBJETO
           ================================================= */

        menor_distancia =
            dist_sin_objeto;


        strcpy(
            color,
            "SIN OBJETO"
        );


        angulo =
            ANG_ESPERA;


        refR =
            R_SIN_OBJETO;


        refG =
            G_SIN_OBJETO;


        refB =
            B_SIN_OBJETO;


        /* =================================================
           VIOLETA
           ================================================= */

        if (
            dist_violeta <
            menor_distancia
        )
        {
            menor_distancia =
                dist_violeta;


            strcpy(
                color,
                "VIOLETA"
            );


            angulo =
                ANG_VIOLETA;


            refR =
                R_VIOLETA;


            refG =
                G_VIOLETA;


            refB =
                B_VIOLETA;
        }


        /* =================================================
           VERDE MANZANA
           ================================================= */

        if (
            dist_verde <
            menor_distancia
        )
        {
            menor_distancia =
                dist_verde;


            strcpy(
                color,
                "V.MANZANA"
            );


            angulo =
                ANG_VERDE;


            refR =
                R_VERDE;


            refG =
                G_VERDE;


            refB =
                B_VERDE;
        }


        /* =================================================
           NARANJA
           ================================================= */

        if (
            dist_naranja <
            menor_distancia
        )
        {
            menor_distancia =
                dist_naranja;


            strcpy(
                color,
                "NARANJA"
            );


            angulo =
                ANG_NARANJA;


            refR =
                R_NARANJA;


            refG =
                G_NARANJA;


            refB =
                B_NARANJA;
        }


        /* =================================================
           BLANCO
           ================================================= */

        if (
            dist_blanco <
            menor_distancia
        )
        {
            menor_distancia =
                dist_blanco;


            strcpy(
                color,
                "BLANCO"
            );


            angulo =
                ANG_BLANCO;


            refR =
                R_BLANCO;


            refG =
                G_BLANCO;


            refB =
                B_BLANCO;
        }


        /* =================================================
           MOVER SERVO
           ================================================= */

        Servo_angle(
            angulo
        );


        /* =================================================
           UART

           Se muestran:
           - R
           - G
           - B
           - Color
           - Referencia
           - Diferencia
           - Angulo
           ================================================= */

        USART_string(
            "R="
        );

        USART_numero(
            adcR
        );


        USART_string(
            " G="
        );

        USART_numero(
            adcG
        );


        USART_string(
            " B="
        );

        USART_numero(
            adcB
        );


        USART_string(
            " COLOR="
        );

        USART_string(
            color
        );


        USART_string(
            " REF=("
        );

        USART_numero(
            refR
        );


        USART_string(
            ","
        );

        USART_numero(
            refG
        );


        USART_string(
            ","
        );

        USART_numero(
            refB
        );


        USART_string(
            ") DIF="
        );

        USART_numero(
            menor_distancia
        );


        USART_string(
            " SERVO="
        );

        USART_numero(
            angulo
        );


        USART_string(
            " grados\r\n"
        );


        /* =================================================
           LCD - PANTALLA 1

           Color + angulo
           Valores medidos RGB
           ================================================= */

        LCD_medicion(
            color,
            angulo,
            adcR,
            adcG,
            adcB
        );


        _delay_ms(
            1000
        );


        /* =================================================
           LCD - PANTALLA 2

           Valores de referencia
           Diferencia
           ================================================= */

        LCD_referencia(
            refR,
            refG,
            refB,
            menor_distancia
        );


        _delay_ms(
            1000
        );
    }


    return 0;
}
