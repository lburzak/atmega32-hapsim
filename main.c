#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#define F_CPU 1000000L

#define LCD_DDR DDRB
#define LCD_PORT PORTB
#define LCD_RS 1
#define LCD_EN 0
#define LCD_DB4 4
#define LCD_DB5 5
#define LCD_DB6 6
#define LCD_DB7 7

#define LCD_HOME 0x02
#define LCD_CLEAR 0x01
#define LCD_CURSOR_RIGHT 0x14
#define LCD_CURSOR_LEFT 0x10

static const char* keymap[] = {
	0,
	"1",
	"2",
	"3",
	"Up",
	"4",
	"5",
	"6",
	"Down",
	"7",
	"8",
	"9",
	"Right",
	"Clear",
	"0",
	"Enter",
	"Left"
};

static const char sign_3[8] = {
	0b00100,
	0b01010,
	0b10001,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000
};

static const char sign_2[8] = {
	0b00000,
	0b00000,
	0b00100,
	0b01010,
	0b10001,
	0b00000,
	0b00000,
	0b00000
};

static const char sign_1[8] = {
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00000,
	0b00100,
	0b01010,
	0b10001
};

void timer_init();

uint8_t keypad_read();
void keypad_init();

void lcd_text(char *chars);
void lcd_number(uint8_t num);
void lcd_fill(char c);
void lcd_clear_from(uint8_t pos);

void lcd_new_sign(char* sign, uint8_t index);
void lcd_clear();
void lcd_init();
void lcd_move_cursor(unsigned char w, unsigned char h);
void lcd_cmd(uint8_t byte);
void lcd_send(uint8_t byte);
void lcd_send_nibble(uint8_t byte);

volatile uint8_t keycode = 0;
volatile uint8_t cursor_row = 0;

// Obsluguje przerwania wywolane przez Timer 0 w trybie CTC
ISR(TIMER0_COMP_vect) {
    keycode = keypad_read();

	if (keycode > 0) {			
		lcd_move_cursor(1, 0);
		lcd_text(keymap[keycode]);
		lcd_clear_from(strlen(keymap[keycode]));		
	}
}

int main() {
	keypad_init();
	timer_init(0.4);
	lcd_init();		//wlaczenie i skonfigurowanie wyswietlacza 
	lcd_clear();

	lcd_new_sign(sign_1, 0);
	lcd_new_sign(sign_2, 1);
	lcd_new_sign(sign_3, 2);

	lcd_move_cursor(0, 0);
	lcd_text("Burzak L.");

    while (1) {
		lcd_anim();

		_delay_ms(400);
	}
}

void lcd_anim() {
	static uint8_t anim = 0;

	if (anim == 3)
			anim = 0;

	lcd_move_cursor(0, 0);
	lcd_send(anim++);
}

/** Wypelnia LCD znakiem */
void lcd_fill(char c) {
	int8_t i = 0;
	lcd_move_cursor(0,0);

	for (uint8_t i = 0; i <= 16; i++)
		lcd_send(c);

	lcd_move_cursor(1,0);

	for (uint8_t i = 0; i <= 16; i++)
		lcd_send(c);
}

/** Kasuje w biezacym wierszu od pozycji */
void lcd_clear_from(uint8_t pos) {
	lcd_move_cursor(cursor_row, 0);
	for (uint8_t i = 0; i < pos; i++)
		lcd_cmd(LCD_CURSOR_RIGHT); // Przesuwa kursor w prawo o jedna pozycje

	while (pos++ <= 16)
		lcd_send(' ');
}

/** Wypisuje liczbe z zakresu 0-19 */
void lcd_number(uint8_t num) {
	if (num == 0)
		return;

	if (num > 9) {
		lcd_send(num / 10 + '0');
		lcd_send(num % 10 + '0');
	} else {
		lcd_send(num + '0');
		lcd_send(' ');
	}
}

/** Wypisuje tekst */
void lcd_text(char *chars) {
	for (uint8_t i = 0; chars[i]; i++) {
		if (i==16)
			lcd_move_cursor(1,0);

		lcd_send(chars[i]);
	}
}

/** Rejestruje nowy znak w pamieci LCD */
void lcd_new_sign(char* sign, uint8_t index) {
	lcd_cmd(0x40 + index * 8);

	for (uint8_t i = 0; i < 8; i++)
		lcd_send(sign[i]);
}

/** Inicializuje LCD */
void lcd_init() {
	LCD_DDR = (0xF0) | (_BV(LCD_RS)) | (_BV(LCD_EN)); // ustawienie kierunku wyjsciowego dla wszystkich linii
	LCD_PORT = 0;

	// Ustawienie trybu 4-bitowego
	lcd_cmd(0x02);

	//ustawienie param wyswietlacza
	//bit4: 1 - 8 linii, 0 - 4 linie
	//bit3: 1 - 2 wiersze, 0 - 1 wiersz
	//bit2: 0 - wymiar znaku 5x8, 1 - wymiar 5x10
	lcd_cmd(0b00101000);

	//bit2: tryb pracy wyswietlacza
	//bit1: 1 - przesuniecie okna, 0 - przesuniecie kursora
	lcd_cmd(0b00000110);

	//bit2: 1 - wyswietlacz wlaczony, 0 - wylaczony
	//bit1:	1 - wlaczenie wyswietlacza kursora, 0 - kursor niewidoczny
	//bit0: 1 - kursor miga, 0 - kursor nie miga
	lcd_cmd(0b00001100);
	
	lcd_clear();
}

/** Przesyla rozkaz wyczyszczenia LCD */
void lcd_clear() {
	lcd_cmd(LCD_CLEAR);
}

/** Przesyla rozkaz przeniesienia kursora */
void lcd_move_cursor(unsigned char w, unsigned char h) {
	cursor_row = w;
	lcd_cmd((w * 0x40 + h) | 0x80);
}

/** Przesyla rozkaz */
void lcd_cmd(uint8_t command) {
	LCD_PORT &= ~(_BV(LCD_RS));
	lcd_send(command);
	LCD_PORT |= _BV(LCD_RS);
	_delay_ms(5);
}

/** Przesyla bajt */
void lcd_send(uint8_t b) {
	lcd_send_nibble(b & 0xF0); //wyslanie 4 starszych bitow
	asm volatile("nop"); // wstawka assemblerowa - odczekanie jednego cyklu
	lcd_send_nibble((b & 0x0F) << 4); //wyslanie 4 mlodszych bitow

	_delay_us(50); 	//odczekanie czasu na potwierdzenie wyslania danych
}

/** Przesyla polbajt */
void lcd_send_nibble(uint8_t byte) {
	LCD_PORT |=_BV(LCD_EN);
	LCD_PORT = byte | (LCD_PORT & 0x0F); 
	LCD_PORT &=~(_BV(LCD_EN));
}

void timer_init() {
    TCCR0 |= (1 << WGM01) | (0 << WGM00);
    TCCR0 |= (1 << CS01) | (1 << CS00);
    OCR0 = F_CPU / 1024 * 0.25 - 1;

    TCNT0 = 0;
    TIMSK |= (1 << OCIE0);

	sei();
}

/** Inicjalizuje klawiature */
void keypad_init() {
	DDRA = 0xf0;
}


/** Zwraca kod wcisnietego przycisku,
    lub 0 jesli zaden przycisk nie jest wcisniety **/
uint8_t keypad_read() {
    uint8_t col_state;

    for (uint8_t col = 0; col <= 3; col++) {
        PORTA = ~(1 << (col + 4));
        col_state = PINA & 0x0f;
        if (col_state < 0x0f)
            return __builtin_ctz(~col_state) * 4 + col + 1;

    }

    return 0;
}
