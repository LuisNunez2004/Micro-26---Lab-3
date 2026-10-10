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
uint16_t ADC_read(uint8_t canal)
{
  ADMUX =
      (ADMUX & 0xF0)|
      (canal & 0x0F);
/*Iniciar conversion*/
ADSRA |= (1 << ADSC);
/*Esperar fin de conversion*/
while (ADCSRA & (1 << ADSC));
return ADC;
}

/*==PROMEDIO DE 10 LECTURAS==*/
uint16_t ADC_promedio(void)
{
  uint32_t suma = 0;
for (uint8_t i = 0; i < 10; i++)
{
suma += ADC_read(0);
_delay_ms(10);
}
return (uint16_t)(suma / 10);
}

/*==UART==*/
void USART_init(unsigned int ubrr)
{
  UBRR0H = (unsigned char)(ubrr >> 8);
  UBRR0L = (unsigned char)ubrr;
/*Habilitar transmisor*/
UCSR0B = (1 << TXEN0);
/*8 bits - sin paridad - 1 bit stop*/
UCSR0C=
      (1 << UCSZ01) |
      (1 << UCSZ00);
}

/*==ENVIAR CARACTER UART==*/
void USART_tx(char dato)
{
  while (!(UCSR0A & (1 << UDRE0));
    UDR0 = dato;
}

/*==ENVIAR TEXTO UART==*/
void USART_string(const char *texto)
{
  while (*texto)
{
USART_tx(*texto++);
}
}

/*==SERVOMOTOR D9/PB1/OC1A==*/
void Servo_init(void)
{
  /*D9 como salida*/
  DDRB |= (1 << PB1);
/*Timer1 Fast PWM modo 14 
TOP = ICR1
Prescaler = 8*/
TCCR1A =
  (1 << COM1A1) |
  (1 << WGM11);
TCCR1B=
  (1 << WGM13) |
  (1 << WGM12) |
  (1 << CS11);
/*Periodo de 20ms
Frecuencia = 50Hz*/
ICR1 = 39999;
}
/*==POSICIONAR SERVO
0 grados -> 0,5ms
90 grados -> 1,5ms
180 grados -> 2,5ms ==*/
void Servo_angle(uint8_t angulo)
{
  uint16_t pulso;

  pulso =
      1000 +
      ((uint32_t)angulo * 4000 / 180);

  OCR1A = pulso;
}

/*==MOSTRAR DATOS EN LCD==*/
void LCD_mostrar(uint16_t adc,
                 char *color,
                 uint8_t angulo)
{
    char textoADC[8];
    char textoAngulo[8];

    itoa(adc, textoADC, 10);

    itoa(angulo, textoAngulo, 10);


    /* Primera linea */

    twi_lcd_cmd(0x80);

    twi_lcd_msg("ADC:");

    twi_lcd_msg(textoADC);

    twi_lcd_msg("          ");


    /* Segunda linea */

    twi_lcd_cmd(0xC0);

    twi_lcd_msg(color);

    twi_lcd_msg(" A:");

    twi_lcd_msg(textoAngulo);

    twi_lcd_msg("      ");
}

/*==PROGRAMA PRINCIPAL==*/
int main(void)
{
  uint16_t adc;
  uint8_t anfulo;
  uint8_t rojo;
  uint8_t verde;
  uint8_t azul;
  char color[12];
  char textoADC[8];
  char textoAngulo[8];

/*INICIALIZAR ADC*/
ADC_int();

/*INICIALIZAR UART*/
USART_init(MYUBRR);

/*INICIALIZAR SERVOMOTOR*/
Servo_init();
/*Servo en posicion de espera*/
Servo_angle(ANG_ESPERA);

/*INICIALIZAR TIRA RGB*/
WS_DDR |= (1 << WS_PIN);
WS_PORT &= ~(1 << WS_PIN);
/*Tira apagada*/
WS_mostrar(0, 0, 0);

/*INICIALIZAR LCD I2C*/
twi_init();
twi_lcd_init();
_delay_ms(5);
twi_lcd_clear();
_delay_ms(5);
/* Pantalla inicial */
twi_lcd_cmd(0x80);
twi_lcd_msg("PROBLEMA C");
twi_lcd_cmd(0xC0);
twi_lcd_msg("INICIANDO");

 /* UART */
USART_string("\r\n");
USART_string("PROBLEMA C\r\n");
USART_string("CLASIFICADOR DE COLOR\r\n");
USART_string("----------------------\r\n");
_delay_ms(1500);
twi_lcd_clear();
_delay_ms(5);

/*==BUCLE PRINCIPAL==*/
while (1)
{
/*Leer la fotocelda - se promedian 10 mediciones para mejorar estabilidad*/
adc = ADC_promedio();

/*SIN OBJETO
Valor medio: ADC: 273-274
Rango utilizado: 0 - 355
Servo: 90 grados
RGB apagado*/
if (adc <= 355)
{
strcpy(color, "SIN OBJETO");
angulo = ANG_ESPERA;
rojo = 0;
verde = 0;
azul = 0;
}

/*VIOLETA
Color 3
ADC real: 436 - 441
Rango: 356 - 490
RGB: 75 , 0 , 130
Servo: 115 grados*/
else if (adc <=490)
{  
  strcpy(color, "VIOLETA");
angulo = ANG_VIOLETA;
rojo = 75;
verde = 0;
azul = 130;
}

/*VERDE MANZANA
Color 1
ADC real: 539 - 552
Rango: 491 - 562
RGB: 124, 252, 0
Servo: 25 grados*/
else if (adc <=562)
{
  strcpy(color, "VERDE MANZANA");
angulo = ANG_VERDE;
rojo = 124;
verde = 252;
azul = 0;
}

/*NARANJA
Color 2
ADC real: 573 - 577
Rango: 563 - 639
RGB: 255 , 69, 0
Servo: 65 grados*/
else if (adc <= 639)
{
  strcpy(color, "NARANJA");
angulo = ANG_NARANJA;
rojo = 255;
verde = 69;
azul = 0;
}

/*BLANCO
Color 4
ADC real = 701 - 707
Rango: 640 - 1023
RGB: 255, 255, 255
Servo: 155 grados*/
else
{
  strcpy(color, "BLANCO");
angulo = ANG_BLANCO;
rojo = 255;
verde = 255;
azul = 255;
}

/*MOSTRAR COLOR EN TIRA RGB*/
WS_mostrar(
rojo,
verde,
azul
);

/*MOVER SERVOMOTOR*/
Servo_angle(angulo);

/*MOSTRAR EN LCD*/
if (adc <= 355)
{
itoa(adc, textoADC, 10);
/*Primera linea*/
twi_lcd_cmd(0x80);
twi_lcd_msg("SIN OBJETO");
/*Segunda linea*/
twi_lcd_cmd(0xC0);
twi_lcd_msg("ADC:");
twi_lcd_msg("       ");
}
else
{
LCD_mostrar(
adc,
color,
angulo
);
}

/*MOSTRAR POR UART*/
itoa(adc, textoADC, 10);
itoa(angulo, textoAngulo, 10);
USART_string("ADC: ");
USART_string(textoADC);
USART_string(" Color: ");
USART_string(color);
USART_string(" Servo: ");
USART_string(textoAngulo);
USART_string(" grados\r\n");

/*Esperar antes de nueva medicion*/
_delay_ms(500);
}
return 0;
}
