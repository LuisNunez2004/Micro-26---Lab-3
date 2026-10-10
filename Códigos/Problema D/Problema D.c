#define F_CPU 16000000UL
#define BAUD 9600
#define MYUBRR ((F_CPU / 16UL / BAUD) - 1)
#define PCF8574 0x27

