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
#define HEATER_DDR   DDRD
#define HEATER_PORT  PORTD
#define HEATER_BIT   PD2

volatile uint16_t ms_counter = 0;
volatile uint8_t  medir_flag = 0;
int8_t punto_medio = 20;

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

#define ACC_CALEFACTOR   0
#define ACC_NEUTRO       1
#define ACC_VENT_BAJO    2
#define ACC_VENT_MEDIO   3
#define ACC_VENT_ALTO    4

void PWM_init(void) {
    TCCR1A = (1 << COM1A1) | (1 << WGM11);                
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);   
    ICR1   = 1999;                                        
    OCR1A  = 0;
    DDRB  |= (1 << DDB1);
}

uint8_t duty_actual = 0;

void Ventilador_set(uint8_t pct) {
    duty_actual = pct;
    OCR1A = (uint32_t)pct * (ICR1 + 1) / 100;
}

void Calefactor_set(uint8_t on) {
    if (on) HEATER_PORT |=  (1 << HEATER_BIT);
    else    HEATER_PORT &= ~(1 << HEATER_BIT);
}

uint8_t Control_aplicar(int16_t t10) {
    int16_t m10 = (int16_t)punto_medio * 10;

    if (t10 <= m10 - 50) {
        Calefactor_set(1); Ventilador_set(0);
        return ACC_CALEFACTOR;
    } else if (t10 <= m10 + 50) {
        Calefactor_set(0); Ventilador_set(0);
        return ACC_NEUTRO;
    } else if (t10 <= m10 + 150) {
        Calefactor_set(0); Ventilador_set(40);
        return ACC_VENT_BAJO;
    } else if (t10 <= m10 + 250) {
        Calefactor_set(0); Ventilador_set(70);
        return ACC_VENT_MEDIO;
    } else {
        Calefactor_set(0); Ventilador_set(100);
        return ACC_VENT_ALTO;
    }
}

const char *Accion_texto(uint8_t a) {
    switch (a) {
        case ACC_CALEFACTOR:  return "Calefactor ENCENDIDO";
        case ACC_NEUTRO:      return "Calefactor y ventilador apagados";
        case ACC_VENT_BAJO:   return "Ventilador velocidad BAJA";
        case ACC_VENT_MEDIO:  return "Ventilador velocidad MEDIA";
        default:              return "Ventilador velocidad ALTA";
    }
}

int main(void) {
    char buf[80];
    int16_t temp10;
    uint16_t hum10;
    uint8_t errores_seguidos = 0;

  #ifdef TEST_FORZADO
    const int16_t pruebas[] = {100, 200, 300, 400, 500};   
    uint8_t idx = 0;
  #endif

    HEATER_DDR |= (1 << HEATER_BIT);
    Calefactor_set(0);
    PWM_init();
    UART_init(103);
    Timer0_init();
    sei();
    UART_sendString("Parte 3: control de temperatura\r\n");

    _delay_ms(2000);                      
    cli(); ms_counter = 0; sei();         
    medir_flag = 1;                       

    while (1) {
        if (medir_flag) {
            medir_flag = 0;

#ifdef TEST_FORZADO
            temp10 = pruebas[idx];
            idx = (idx + 1) % 5;
            uint8_t err = 0;
#else
            uint8_t err = DHT22_read(&temp10, &hum10);
#endif

            if (err == 0) {
                errores_seguidos = 0;
                uint8_t acc = Control_aplicar(temp10);
                int16_t a = abs(temp10);
                sprintf(buf, "T: %s%d.%d C | Punto medio: %d | %s%\r\n",
                        temp10 < 0 ? "-" : "", a / 10, a % 10,
                        punto_medio, Accion_texto(acc));
            } else {
                if (++errores_seguidos >= 3) {
                    Calefactor_set(0);
                    Ventilador_set(0);
                }
                sprintf(buf, "Error DHT22: %d (consecutivos: %d)\r\n", err, errores_seguidos);
            }
            UART_sendString(buf);
        }
    }
}