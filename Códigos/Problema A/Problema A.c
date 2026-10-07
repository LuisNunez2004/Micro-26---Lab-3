#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>

#define DHT_DDR   DDRD
#define DHT_PORT  PORTD
#define DHT_PINR  PIND
#define DHT_BIT   PD4
#define PERIODO_MS 5000

volatile uint16_t ms_counter = 0;
volatile uint8_t  medir_flag = 0;

void UART_init(unsigned int ubrr) {
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}
void UART_sendChar(char data) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}
void UART_sendString(const char *str) {
    while (*str) UART_sendChar(*str++);
}

static uint8_t wait_level(uint8_t level, uint8_t timeout_us) {
    while (((DHT_PINR >> DHT_BIT) & 1) != level) {
        if (timeout_us-- == 0) return 0;
        _delay_us(1);
    }
    return 1;
}

uint8_t DHT22_read(int16_t *temp10, uint16_t *hum10) {
    uint8_t data[5] = {0, 0, 0, 0, 0};

    DHT_DDR  |=  (1 << DHT_BIT);    
    DHT_PORT &= ~(1 << DHT_BIT);    
    _delay_ms(2);

    cli();                          
    DHT_DDR &= ~(1 << DHT_BIT);     

    if (!wait_level(0, 100)) { sei(); return 1; }
    if (!wait_level(1, 100)) { sei(); return 1; }
    if (!wait_level(0, 100)) { sei(); return 1; }

    for (uint8_t i = 0; i < 40; i++) {
        if (!wait_level(1, 70)) { sei(); return 2; }  
        _delay_us(40);                                
        if ((DHT_PINR >> DHT_BIT) & 1) {
            data[i / 8] |= (1 << (7 - (i % 8)));      
        }
        if (!wait_level(0, 70)) { sei(); return 2; }  
    }
    sei();

    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4]) return 3;

    *hum10 = ((uint16_t)data[0] << 8) | data[1];
    int16_t t = ((uint16_t)(data[2] & 0x7F) << 8) | data[3];
    if (data[2] & 0x80) t = -t;              
    *temp10 = t;
    return 0;
}

void Timer0_init(void) {
    TCCR0A = (1 << WGM01);                
    TCCR0B = (1 << CS01) | (1 << CS00);   
    OCR0A  = 249;                         
    TIMSK0 = (1 << OCIE0A);               
}

ISR(TIMER0_COMPA_vect) {                  
    if (++ms_counter >= PERIODO_MS) {
        ms_counter = 0;
        medir_flag = 1;
    }
}

int main(void) {
    char buf[40];
    int16_t temp10;
    uint16_t hum10;

    UART_init(103);
    Timer0_init();
    sei();                                
    UART_sendString("Lectura DHT22\r\n");

    _delay_ms(2000);                      
    cli(); ms_counter = 0; sei();         
    medir_flag = 1;                       

    while (1) {
        if (medir_flag) {
            medir_flag = 0;
            uint8_t err = DHT22_read(&temp10, &hum10);
            if (err == 0) {
                int16_t a = abs(temp10);
                sprintf(buf, "Temperatura: %s%d.%d C\r\n", temp10 < 0 ? "-" : "", a / 10, a % 10);
            } else {
                sprintf(buf, "Error DHT22: %d\r\n", err);
            }
            UART_sendString(buf);
        }

        if (UCSR0A & (1 << RXC0)) {
            char c = UDR0;
            UART_sendString("Recibido: ");
            UART_sendChar(c);
            UART_sendString("\r\n");
        }
    }
}
