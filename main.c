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

// Komendy LCD
#define LCD_CLEAR 0x01
#define LCD_CURSOR_RIGHT 0x14

// Definicja pierwszego znaku w animacji
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

// Definicja drugiego znaku w animacji
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

// Definicja trzeciego znaku w animacji
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

void lcd_clear_from(uint8_t pos);
void lcd_new_sign(char* sign, uint8_t index);

void lcd_clear();
void lcd_init();
void lcd_move_cursor(unsigned char w, unsigned char h);
void lcd_cmd(uint8_t byte);
void lcd_send(uint8_t byte);
void lcd_send_nibble(uint8_t byte);

// Inicjalizuje zmienna przechowujaca numer biezacej linii LCD
volatile uint8_t cursor_row = 0;

int main() {
	// Inicjalizuje LCD
	lcd_init();

    while (1) {
		lcd_anim();
		_delay_ms(400);
	}
}

/** Wyswietla nastepna w kolejnosci klatke animacji */
void lcd_anim() {
	// Inicjalizuje zmienna do przechowywania indeksu klatki
	static uint8_t anim = 0;

	// Przenosi kursor na poczatek pierwszej linii
	lcd_move_cursor(0, 0);

	// Wyswietla klatke animacji i przechodzi do nastepnej klatki
	lcd_send(anim++);

	// Zawija kolejnosc klatek
	if (anim == 3)
			anim = 0;
}

/** Rejestruje nowy znak w pamieci LCD */
void lcd_new_sign(char* sign, uint8_t index) {
	// Przenosi kursor do miejsca przeznaczonego na zapis znaku
	lcd_cmd(0x40 + index * 8);

	// Przesyla wszystkie bajty znaku we wskazane mejsce
	for (uint8_t i = 0; i < 8; i++)
		lcd_send(sign[i]);
}

/** Inicializuje LCD */
void lcd_init() {
	// Ustawia wszystkie linie na wyjscie
	LCD_DDR = (0xF0) | (_BV(LCD_RS)) | (_BV(LCD_EN));
	LCD_PORT = 0;

	// Ustawia tryb 4-bitowy
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

	// Czysci LCD
	lcd_clear();
}

/** Przesyla komende wyczyszczenia LCD */
void lcd_clear() {
	lcd_cmd(LCD_CLEAR);
}

/** Przesyla komende przeniesienia kursora */
void lcd_move_cursor(unsigned char w, unsigned char h) {
	// Zachowuje nowa pozycje w pamieci
	cursor_row = w;

	// Przenosi kursor w wyznaczone miejsce
	lcd_cmd((w * 0x40 + h) | 0x80);
}

/** Przesyla wybrana komende */
void lcd_cmd(uint8_t command) {
	// Przelacza LCD w tryb komend
	LCD_PORT &= ~(_BV(LCD_RS));

	// Przesyla wskazany bajt komendy
	lcd_send(command);

	// Przelacza LCD w tryb danych
	LCD_PORT |= _BV(LCD_RS);

	_delay_ms(5);
}

/** Przesyla bajt */
void lcd_send(uint8_t b) {
	// Przesyla starszy polbajt
	lcd_send_nibble(b & 0xF0);

	// Odczekuje jeden cykl
	asm volatile("nop");

	// Przesyla mlodszy polbajt
	lcd_send_nibble((b & 0x0F) << 4);

	_delay_us(50);
}

/** Przesyla starszy polbajt wskazanego bajtu */
void lcd_send_nibble(uint8_t byte) {
	// Aktywuje przesyl danych
	LCD_PORT |=_BV(LCD_EN);

	// Ustawia dane w porcie
	LCD_PORT = byte | (LCD_PORT & 0x0F);

	// Zatwierdza przesyl danych
	LCD_PORT &=~(_BV(LCD_EN));
}
