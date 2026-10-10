#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DHT_DDR   DDRD
#define DHT_PORT  PORTD
#define DHT_PINR  PIND
#define DHT_BIT   PD4
#define PERIODO_MS 5000
#define HEATER_DDR   DDRD
#define HEATER_PORT  PORTD
#define HEATER_BIT   PD2

#define ACC_CALEFACTOR    0
#define ACC_NEUTRO        1
#define ACC_VENT_BAJO     2
#define ACC_VENT_MEDIO    3
#define ACC_VENT_ALTO     4

#define PM_MIN 5
#define PM_MAX 45
#define RX_BUF_LEN 16

#define EST_MONITOREO     0
#define EST_MENU          1
#define EST_ESPERA_VALOR  2

#define LCD_ADDR  0x3E  

/* Retardos independientes del nivel de optimizacion (Proteus con -O0) */
static void retardo_us(uint16_t us) {
    if (us == 0) return;
    _delay_loop_2((uint16_t)(us * (F_CPU / 4000000UL)));
}

static void retardo_ms(uint16_t ms) {
    while (ms--) retardo_us(1000);
}

#define _delay_us(x) retardo_us(x)
#define _delay_ms(x) retardo_ms(x)

volatile uint16_t ms_counter = 0;
volatile uint8_t  medir_flag = 0;
int8_t punto_medio = 20;

uint8_t estado_ui = EST_MONITOREO;
char    rx_buf[RX_BUF_LEN];
uint8_t rx_idx = 0;

int16_t ultima_temp10 = 0;
uint8_t hay_lectura = 0;

char dat[40];
uint16_t muestra = 0;

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
    _delay_ms(20);

    DHT_DDR  &= ~(1 << DHT_BIT);
    DHT_PORT |=  (1 << DHT_BIT);  
    cli();     

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

    *hum10 = (uint16_t)data[0] * 10 + data[1];
    *temp10 = (int16_t)data[2] * 10 + data[3];
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

void Menu_mostrar(void) {
    UART_sendString("\r\n--- MENU ---\r\n"
                    "1) Ver punto medio\r\n"
                    "2) Cambiar punto medio\r\n"
                    "3) Salir\r\n> ");
}

void Linea_procesar(void) {
    char msg[100];

    switch (estado_ui) {
    case EST_MONITOREO:
        if (rx_buf[0] == 'm' || rx_buf[0] == 'M') {
            estado_ui = EST_MENU;
            Menu_mostrar();
        }
        break;

    case EST_MENU:
        if (rx_buf[0] == '1') {
            sprintf(msg, "Punto medio actual: %d C\r\n", punto_medio);
            UART_sendString(msg);
            Menu_mostrar();
        } else if (rx_buf[0] == '2') {
            sprintf(msg, "Nuevo punto medio (%d a %d C): ", PM_MIN, PM_MAX);
            UART_sendString(msg);
            estado_ui = EST_ESPERA_VALOR;
        } else if (rx_buf[0] == '3') {
            UART_sendString("Saliendo del menu...\r\n");
            estado_ui = EST_MONITOREO;
        } else {
            UART_sendString("Opcion invalida\r\n");
            Menu_mostrar();
        }
        break;

    case EST_ESPERA_VALOR: {
        uint8_t len = strlen(rx_buf);
        uint8_t solo_digitos = (len > 0 && len <= 3);
        for (uint8_t i = 0; i < len; i++) {
            if (rx_buf[i] < '0' || rx_buf[i] > '9') solo_digitos = 0;
        }

        if (!solo_digitos) {
            UART_sendString("Valor no valido: solo numeros enteros positivos.\r\n");
        } else {
            int16_t v = atoi(rx_buf);
            if (v > PM_MAX) {
                sprintf(msg, "RECHAZADO: %d C muy alto. Maximo: %d C.\r\n", v, PM_MAX);
                UART_sendString(msg);
            } else if (v < PM_MIN) {
                sprintf(msg, "RECHAZADO: minimo permitido %d C.\r\n", PM_MIN);
                UART_sendString(msg);
            } else {
                punto_medio = (int8_t)v;
                sprintf(msg, "Punto medio actualizado a %d C\r\n", punto_medio);
                UART_sendString(msg);
                if (hay_lectura) {
                    sprintf(msg, "Accion: %s\r\n", Accion_texto(Control_aplicar(ultima_temp10)));
                    UART_sendString(msg);
                }
            }
        }
        estado_ui = EST_MENU;
        Menu_mostrar();
        break;
    }
    }
}

void UART_poll(void) {
    while (UCSR0A & (1 << RXC0)) {
        char c = UDR0;
        if (c == '\r' || c == '\n') {
            if (rx_idx > 0) {                 
                rx_buf[rx_idx] = '\0';
                rx_idx = 0;
                UART_sendString("\r\n");
                Linea_procesar();
            }
        } else if (c == 0x08 || c == 0x7F) {  
            if (rx_idx > 0) {
                rx_idx--;
                UART_sendString("\b \b");
            }
        } else if (rx_idx < RX_BUF_LEN - 1) {  
            rx_buf[rx_idx++] = c;
            UART_sendChar(c);                 
        }
    }
}

/* ---------- LCD ST7032 vía I2C ---------- */

void I2C_init(void) {
    TWSR = 0x00;  
    TWBR = 72;    
    TWCR = (1 << TWEN);
}

void I2C_start(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

void I2C_write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

void I2C_stop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void LCD_command(uint8_t cmd) {
    I2C_start();
    I2C_write((LCD_ADDR << 1) | 0); 
    I2C_write(0x00);             
    I2C_write(cmd);
    I2C_stop();
    _delay_ms(2);
}

void LCD_data(uint8_t data) {
    I2C_start();
    I2C_write((LCD_ADDR << 1) | 0);
    I2C_write(0x40); 
    I2C_write(data);
    I2C_stop();
}

void LCD_init(void) {
    _delay_ms(50);
    LCD_command(0x38); 
    LCD_command(0x39); 
    LCD_command(0x14); 
    LCD_command(0x70); 
    LCD_command(0x56); 
    LCD_command(0x6C); 
    _delay_ms(200);
    LCD_command(0x38); 
    LCD_command(0x0C); 
    LCD_command(0x01); 
    _delay_ms(2);
}

void LCD_setCursor(uint8_t row, uint8_t col) {
    uint8_t address = (row == 0) ? (0x00 + col) : (0x40 + col);
    LCD_command(0x80 | address);
}

void LCD_print(const char *str) {
    while (*str) {
        LCD_data((uint8_t)*str);
        str++;
    }
}

void LCD_linea(const char *texto) {
    char buffer[17];
    snprintf(buffer, sizeof(buffer), "%-16s", texto);
    LCD_print(buffer);
}

void LCD_actualizar(int16_t t10, uint8_t ok) {
    char l[20];
    LCD_setCursor(0, 0);
    if (ok) {
        int16_t a = abs(t10);
        sprintf(l, "Temp: %s%d.%d C", t10 < 0 ? "-" : "", a / 10, a % 10);
    } else {
        sprintf(l, "Temp: ERROR");
    }
    LCD_linea(l);

    LCD_setCursor(1, 0);
    sprintf(l, "PM:%d Rng:%d-%d", punto_medio, punto_medio - 5, punto_medio + 5);
    LCD_linea(l);
}

int main(void) {
    char buf[100];
    int16_t temp10;
    uint16_t hum10;
    uint8_t errores_seguidos = 0;

    HEATER_DDR |= (1 << HEATER_BIT);
    Calefactor_set(0);
    PWM_init();
    UART_init(103);
    Timer0_init();
    sei();

    UART_sendString("Sistema Iniciando...\r\n");

    I2C_init();
    LCD_init();
    _delay_ms(500);

    LCD_setCursor(0, 0);
    LCD_print("Iniciando...");
    _delay_ms(2000);

    cli(); ms_counter = 0; sei();
    medir_flag = 1;

    while (1) {
        if (medir_flag) {
            medir_flag = 0;
            muestra++;
            dat[0] = '\0';

            uint8_t err = DHT22_read(&temp10, &hum10);

            if (err == 0) {
                errores_seguidos = 0;
                ultima_temp10 = temp10;
                hay_lectura = 1;
                LCD_actualizar(temp10, 1);
                uint8_t acc = Control_aplicar(temp10);
                int16_t a = abs(temp10);
                sprintf(buf, "T: %s%d.%d C | Punto medio: %d | %s | PWM: %d%%\r\n",
                        temp10 < 0 ? "-" : "", a / 10, a % 10,
                        punto_medio, Accion_texto(acc), duty_actual);
                sprintf(dat, "D,%u,%s%d.%d,%d,%u,%u\r\n",
                        muestra, temp10 < 0 ? "-" : "", a / 10, a % 10,
                        punto_medio, acc, duty_actual);
            } else {
                if (++errores_seguidos >= 3) {
                    Calefactor_set(0);
                    Ventilador_set(0);
                }
                sprintf(buf, "Error DHT: %d (consecutivos: %d)\r\n", err, errores_seguidos);
                LCD_actualizar(0, 0);
            }

            if (estado_ui == EST_MONITOREO) {
                UART_sendString(buf);
                if (dat[0] != '\0') UART_sendString(dat);  
            }
        }

        UART_poll();                       
    }
}
