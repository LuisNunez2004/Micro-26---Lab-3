#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdio.h>

// Configurar la comunicacion UART a 9600 baud
void uart_iniciar(void)
{
    UCSR0A = 0;

    UBRR0H = 0;
    UBRR0L = 103;

    // Habilitar la transmision
    UCSR0B = (1 << TXEN0);

    // 8 bits de datos, sin paridad y 1 bit de parada
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// Enviar un caracter por UART
void uart_caracter(char caracter)
{
    // Esperar a que el registro de envio este disponible
    while (!(UCSR0A & (1 << UDRE0)))
    {
    }

    UDR0 = caracter;
}

// Enviar un texto por UART
void uart_texto(const char *texto)
{
    while (*texto != '\0')
    {
        uart_caracter(*texto);
        texto++;
    }
}

// Configurar las entradas analogicas
void adc_iniciar(void)
{
    // A0 y A1 como entradas
    DDRC &= ~((1 << PC0) | (1 << PC1));

    // Desactivar las resistencias pull-up
    PORTC &= ~((1 << PC0) | (1 << PC1));

    // Desactivar la entrada digital en A0 y A1
    DIDR0 = (1 << ADC0D) | (1 << ADC1D);

    // Usar AVCC como referencia de tension
    ADMUX = (1 << REFS0);

    // Habilitar ADC con divisor de reloj de 128
    ADCSRA = (1 << ADEN)
           | (1 << ADPS2)
           | (1 << ADPS1)
           | (1 << ADPS0);
}

// Leer el canal analogico seleccionado
uint16_t adc_leer(uint8_t canal)
{
    // Seleccionar el canal y conservar la referencia AVCC
    ADMUX = (1 << REFS0) | (canal & 0x07);

    // Primera conversion despues de cambiar de canal
    ADCSRA |= (1 << ADSC);

    while (ADCSRA & (1 << ADSC))
    {
    }

    // Descartar esta primera lectura
    (void)ADC;

    // Realizar la conversion que vamos a utilizar
    ADCSRA |= (1 << ADSC);

    while (ADCSRA & (1 << ADSC))
    {
    }

    return ADC;
}

int main(void)
{
    uint16_t luz;
    uint16_t posicion;
    char mensaje[48];

    // Mantener las señales del motor en cero
    PORTD &= ~((1 << PD5) | (1 << PD7));
    PORTB &= ~(1 << PB0);

    // D5, D7 y D8 como salidas
    DDRD |= (1 << PD5) | (1 << PD7);
    DDRB |= (1 << PB0);

    uart_iniciar();
    adc_iniciar();

    uart_texto("ETAPA 1: lectura de sensores\r\n");

    while (1)
    {
        // Leer la LDR conectada a A0
        luz = adc_leer(0);

        // Leer el potenciometro conectado a A1
        posicion = adc_leer(1);

        // Preparar el mensaje con las dos lecturas
        snprintf(
            mensaje,
            sizeof(mensaje),
            "LDR=%u  POSICION=%u\r\n",
            (unsigned int)luz,
            (unsigned int)posicion
        );

        uart_texto(mensaje);

        // Esperar medio segundo antes de repetir
        _delay_ms(500);
    }
}
