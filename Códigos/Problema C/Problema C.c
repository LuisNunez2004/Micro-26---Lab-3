#define F_CPU 16000000UL
#define BAUD 9600
/* Direccion I2C del LCD */
#define PCF8574 0x27

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "twi_lcd.h"

/*==============================
PROBLEMA C CLASIFICADOR DE COLOR

Fotocelda     -> A0/PC0/ADC0
Tira RGB DIN  -> D6/PD6
Servo         -> D9/PB1/OC1A
LCD SDA       -> A4/PC4
LCD SCL       -> A5/PC5
UART          -> 9600 buad
===============================*/

/*== ANGULOS DEL SERVO ==*/
#define ANG_VERDE    25
#define ANG_NARANJA  65
#define ANG_ESPERA   90
#define ANG_VIOLETA 115
#define ANG_BLANCO  155

/*==TIRA LED WS2812 - DIN -> D6/PD6==*/
#define WS_PORT PORTD
#define WS_DDR DDRD
#define WS_PIN PD6

/*==CANTIDAD DE LEDs/GRUPOS DE LA TIRA==*/
#define NUM_PIXELS 10

/*==ENVIAR UN BIT A LA TIRA RGB==*/
static inline_attribute_((always_inline))
void WS_bit(uint8_t bit)
{
  if (bit)
{
  asm volatile (
  "sbi %0, %1 \n\t"

            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"

            "cbi %0, %1 \n\t"

            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
  :
  : "I" (_SFR_IO_ADDR(WS_PORT)),
    "I" (WS_PIN)
  );
}
else
{
  asm volatile(
            "sbi %0, %1 \n\t"

            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"

            "cbi %0, %1 \n\t"

            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
            "nop \n\t"
  :
  : "I" (_SFR_IO_ADDR(WS_PORT)),
    "I" (WS_PIN)
  );
 }
}
/*==ENVIAR BYTE==*/
void WS_byte(uint8 dato)
{
  for (unit8_t i= 0; i < 8; i++)
  { 
    WS_bit(dato & 0x80);
    dato <<= 1;
  }
}

/*==ENVIAR COLOR - ORDEN RGB==*/
void WS_color(uint8_t rojo,
              uint8_t verde,
              uint8_t azul)
{
     WS_byte(verde);
     WS_byte(rojo);
     WS_byte(azul);
}

/*==MOSTRAR COLOR EN LA TIRA==*/
void WS_mostrar(uint8_t rojo,
              uint8_t verde,
              uint8_t azul)
{
   uint8_t estado_interrupciones = SREG;
   cli();
   for (uint8_t i = 0, i < NUM_PIXELS; i++)
   {
     WS_color(rojo, verde, azul);
   }
   SREG = estado_interrupciones;
   _delay_us(80);
}

/*==ADC Fotocelda -> A0/ADC0==*/
void ADC_init(void)
{
  /*Ref AVcc*/
  ADMUX = (1 << REFS0);
  /*ADC habilitado - Prescaler = 128*/
  ADCDRA =
   (1 << ADENN)|
   (1 << ADPS2)|
   (1 << ADPS1)|
   (1 << ADPS0);
}

/*==LEER ADC==*/
