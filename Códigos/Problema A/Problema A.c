#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
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

#define ACC_CALEFACTOR   0
#define ACC_NEUTRO       1
#define ACC_VENT_BAJO    2
#define ACC_VENT_MEDIO   3
#define ACC_VENT_ALTO    4

#define PM_MIN 5
#define PM_MAX 50
#define RX_BUF_LEN 16

#define EST_MONITOREO     0
#define EST_MENU          1
#define EST_ESPERA_VALOR  2

#define LCD_ADDR  0x27      
#define LCD_RS    0x01
#define LCD_EN    0x04
#define LCD_BL    0x08      

volatile uint16_t ms_counter = 0;
volatile uint8_t  medir_flag = 0;
int8_t punto_medio = 20;

uint8_t estado_ui = EST_MONITOREO;
char    rx_buf[RX_BUF_LEN];
uint8_t rx_idx = 0;

int16_t ultima_temp10 = 0;
uint8_t hay_lectura = 0;


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
                sprintf(msg, "RECHAZADO: %d C queda muy cerca del maximo del sensor (80 C). Maximo: %d C.\r\n", v, PM_MAX);
                UART_sendString(msg);
            } else if (v < PM_MIN) {
                sprintf(msg, "RECHAZADO: minimo permitido %d C.\r\n", PM_MIN);
                UART_sendString(msg);
            } else {
                punto_medio = (int8_t)v;
                LCD_actualizar(ultima_temp10, hay_lectura);
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

/* ---------- I2C (TWI) ---------- */

void TWI_init(void) {
    TWSR = 0;           
    TWBR = 72;          
}

uint8_t lcd_err = 0;     

static uint8_t TWI_wait(void) {
    uint16_t n = 60000;
    while (!(TWCR & (1 << TWINT))) {
        if (--n == 0) return 0;          
    }
    return 1;
}

uint8_t TWI_start(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    if (!TWI_wait()) return 0xFF;
    return TWSR & 0xF8;
}

uint8_t TWI_write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    if (!TWI_wait()) return 0xFF;
    return TWSR & 0xF8;
}

void TWI_stop(void) {
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
    uint16_t n = 2000;
    while ((TWCR & (1 << TWSTO)) && --n);   
}

static uint8_t START_ok(uint8_t st) {
    return (st == 0x08 || st == 0x10);
}

void I2C_scan(void) {
    char msg[40];
    uint8_t n = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        uint8_t s  = TWI_start();
        uint8_t ok = START_ok(s);
        uint8_t st = ok ? TWI_write(addr << 1) : s;
        TWI_stop();
        if (st == 0x18) {
            sprintf(msg, "I2C: dispositivo en 0x%02X\r\n", addr);
            UART_sendString(msg);
            n++;
        } else if (!ok) {
            sprintf(msg, "I2C: fallo el START (estado 0x%02X)\r\n", s);
            UART_sendString(msg);
            return;
        }
    }
    if (n == 0) UART_sendString("I2C: no respondio ningun dispositivo\r\n");
}

static void PCF_write(uint8_t b) {
    if (lcd_err) return;
    uint8_t err = 0;
    uint8_t st = TWI_start();
    if (!START_ok(st)) err = st ? st : 0xEE;
    else {
        st = TWI_write(LCD_ADDR << 1);
        if (st != 0x18) err = st ? st : 0xEE;
        else {
            st = TWI_write(b);
            if (st != 0x28) err = st ? st : 0xEE;
        }
    }
    TWI_stop();
    if (err) lcd_err = err;
}

/* ---------- LCD 16x2 vía PCF8574 ---------- */


static void LCD_nibble(uint8_t nib, uint8_t rs) {
    uint8_t d = (uint8_t)(nib << 4) | LCD_BL | (rs ? LCD_RS : 0);
    PCF_write(d | LCD_EN);      
    _delay_us(1);
    PCF_write(d & ~LCD_EN);     
    _delay_us(50);
}

static void LCD_send(uint8_t v, uint8_t rs) {
    LCD_nibble(v >> 4, rs);     
    LCD_nibble(v & 0x0F, rs);   
}

void LCD_cmd(uint8_t c) {
    LCD_send(c, 0);
    if (c <= 0x03) _delay_ms(2);    
}

void LCD_init(void) {
    TWI_init();
    _delay_ms(50);              
    LCD_nibble(0x03, 0); _delay_ms(5);
    LCD_nibble(0x03, 0); _delay_us(150);
    LCD_nibble(0x03, 0); _delay_us(150);
    LCD_nibble(0x02, 0);        
    LCD_cmd(0x28);              
    LCD_cmd(0x0C);              
    LCD_cmd(0x06);              
    LCD_cmd(0x01);              
}

void LCD_setCursor(uint8_t col, uint8_t row) {
    LCD_cmd(0x80 | (col + (row ? 0x40 : 0x00)));
}

void LCD_linea(const char *texto) {
    uint8_t n = 0;
    while (*texto && n < 16) { LCD_send(*texto++, 1); n++; }
    while (n < 16)           { LCD_send(' ', 1);      n++; }
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

    LCD_setCursor(0, 1);
    sprintf(l, "PM:%d Rng:%d-%d", punto_medio, punto_medio - 5, punto_medio + 5);
    LCD_linea(l);
}

int main(void) {
    char buf[100];
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
    UART_sendString("Parte 4: menu (escribe 'm' para abrirlo)\r\n");

    TWI_init();
    //I2C_scan();                          
    LCD_init();
    if (lcd_err) {
        sprintf(buf, "LCD: error I2C 0x%02X\r\n", lcd_err);
        UART_sendString(buf);
    } else {
        UART_sendString("LCD: init OK\r\n");
    }
    LCD_setCursor(0, 0);
    LCD_linea("Iniciando...");

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
                ultima_temp10 = temp10;
                hay_lectura = 1;
                LCD_actualizar(temp10, 1);
                uint8_t acc = Control_aplicar(temp10);
                int16_t a = abs(temp10);
                sprintf(buf, "T: %s%d.%d C | Punto medio: %d | %s | PWM: %d%%\r\n",
                        temp10 < 0 ? "-" : "", a / 10, a % 10,
                        punto_medio, Accion_texto(acc), duty_actual);
            } else {
                if (++errores_seguidos >= 3) {
                    Calefactor_set(0);
                    Ventilador_set(0);
                }
                sprintf(buf, "Error DHT22: %d (consecutivos: %d)\r\n", err, errores_seguidos);
                LCD_actualizar(0, 0);
            }

            if (estado_ui == EST_MONITOREO) {  
                UART_sendString(buf);
            }
        }

        UART_poll();                             
    }
}
