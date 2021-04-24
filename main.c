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

void lcd_fill(char c);
void lcd_clear_from(uint8_t pos);

void lcd_clear();
void lcd_init();
void lcd_move_cursor(unsigned char w, unsigned char h);
void lcd_cmd(uint8_t byte);
void lcd_send(uint8_t byte);
void lcd_send_nibble(uint8_t byte);

// Inicjalizuje zmienna przechowujaca numer biezacego wiersza
static uint8_t cursor_row = 0;

int main() {
	// Inicjalizuje LCD
	lcd_init();

    while (1) {
		// Wypelnia wyswietlacz znakiem 'a'
		lcd_fill('a');
		_delay_ms(400);

		// Czysci wyswietlacz
		lcd_clear();
		_delay_ms(400);

		// Wypisuje znak 'Y' w drugiej linii na trzeciej pozycji
		lcd_move_cursor(1, 2);
		lcd_send('Y');
		_delay_ms(400);

		// Kasuje znaki w biezacym wierszu od pozycji 2
		lcd_clear_from(2);
		_delay_ms(400);
	}
}

/** Wypelnia LCD znakiem */
void lcd_fill(char c) {
	// Iteruje po liniach
	for (uint8_t row, col = 0; row <= 1; row++) {

		// Ustawia kursor na poczatku linii
		lcd_move_cursor(row, 0);

		// Iteruje po kolumnach i wyswietla znak
		for (col = 0; col <= 16; col++)
			lcd_send(c);
	}
}

/** Kasuje w biezacym wierszu od pozycji */
void lcd_clear_from(uint8_t pos) {
	// Przenosi kursor na poczatek biezacej linii
	lcd_move_cursor(cursor_row, 0);

	// Przesuwa kursor do wskazanej pozycji
	for (uint8_t i = 0; i < pos; i++)
		lcd_cmd(LCD_CURSOR_RIGHT);

	// Czysci znaki od wskazanej pozycji
	while (pos++ <= 16)
		lcd_send(' ');
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

/** Konfiguruje Timer 0 */
void timer_init() {
	// Ustawia Timer 0 w tryb CTC
    TCCR0 |= (1 << WGM01) | (0 << WGM00);

	// Ustawia preskaler 1/1024
    TCCR0 |= (1 << CS01) | (1 << CS00);

	// Ustawia liczbe impulsow, po ktorej nastepuje przerwanie
    // Przerwanie ma wystepowac po 0.25 s
    OCR0 = F_CPU / 1024 * 0.25 - 1;

	// Resetuje stan licznika
    TCNT0 = 0;

	// Aktywuje przerwania Timera 0 w trybie CTC
    TIMSK |= (1 << OCIE0);

	// Aktywuje obsluge przerwan
	sei();
}
