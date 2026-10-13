#define F_CPU 16000000UL
#define BAUD 9600
#define MYUBRR (F_CPU / 16 / BAUD - 1)

/* Direccion I2C del LCD */
#define PCF8574 0x27

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdlib.h>

#include "twi_lcd.h"


/* =========================================================
   PROBLEMA C
   CALIBRACION CON LEDs BLANCOS

   Fotocelda -> A0 / PC0 / ADC0

   Servo:
   D9 / PB1 / OC1A

   LCD I2C:
   SDA -> A4 / PC4
   SCL -> A5 / PC5

   UART:
   9600 baud

   LEDs blancos:
   Permanecen encendidos durante la medicion.
   ========================================================= */


/* =========================================================
   ANGULO DE ESPERA DEL SERVO
   ========================================================= */

#define ANG_ESPERA 90


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
       Habilitar ADC
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


    /* Esperar fin de conversion */

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


    UDR0 =
        dato;
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

    ICR1 =
        39999;
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
   MOSTRAR ADC EN LCD
   ========================================================= */

void LCD_mostrar_ADC(uint16_t adc)
{
    char textoADC[10];


    itoa(
        adc,
        textoADC,
        10
    );


    /* Primera linea */

    twi_lcd_cmd(
        0x80
    );


    twi_lcd_msg(
        "CALIBRACION     "
    );


    /* Segunda linea */

    twi_lcd_cmd(
        0xC0
    );


    twi_lcd_msg(
        "ADC:"
    );


    twi_lcd_msg(
        textoADC
    );


    twi_lcd_msg(
        "           "
    );
}


/* =========================================================
   PROGRAMA PRINCIPAL
   ========================================================= */

int main(void)
{
    uint16_t adc;


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


    /*
       Durante la calibracion,
       dejar servo fijo en 90 grados.
    */

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
        "PROBLEMA C"
    );


    twi_lcd_cmd(
        0xC0
    );


    twi_lcd_msg(
        "LED BLANCO"
    );


    /* =====================================================
       UART
       ===================================================== */

    USART_string(
        "\r\n"
    );


    USART_string(
        "================================\r\n"
    );


    USART_string(
        "PROBLEMA C - LEDS BLANCOS\r\n"
    );


    USART_string(
        "CALIBRACION DE FOTOCELDA\r\n"
    );


    USART_string(
        "================================\r\n\r\n"
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
           PROMEDIAR 10 LECTURAS ADC
           ================================================= */

        adc =
            ADC_promedio();


        /* =================================================
           MOSTRAR EN LCD
           ================================================= */

        LCD_mostrar_ADC(
            adc
        );


        /* =================================================
           MOSTRAR EN UART
           ================================================= */

        USART_string(
            "ADC = "
        );


        USART_numero(
            adc
        );


        USART_string(
            "\r\n"
        );


        /* =================================================
           NUEVA MEDICION CADA 500 ms
           ================================================= */

        _delay_ms(
            500
        );
    }


    return 0;
}
