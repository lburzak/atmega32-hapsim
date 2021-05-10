#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <math.h>

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
#define LCD_HOME 0x02
#define LCD_CLEAR 0x01
#define LCD_CURSOR_RIGHT 0x14
#define LCD_CURSOR_LEFT 0x10

// Kody przyciskow
#define KEY_UP 4
#define KEY_DOWN 8
#define KEY_ENTER 16
#define KEY_CLEAR 12
#define SW_1 2
#define SW_2 3

#define LED_DDR DDRC
#define LED_PORT PORTC

uint8_t keypad_read();
void keypad_init();

void lcd_text(char *chars);
void lcd_number(uint8_t num);

void lcd_clear();
void lcd_new_sign(char* sign, uint8_t index);
void lcd_init();
void lcd_move_cursor(unsigned char w, unsigned char h);
void lcd_cmd(uint8_t byte);
void lcd_send(uint8_t byte);
void lcd_send_nibble(uint8_t byte);

void assert(uint8_t condition, char* message);

void timer2_setup_interrupt(float period);
static float timer2_tick_period = 60;

// Struktura okreslajaca pojedyncze menu
struct Menu {
	// Przechowuje indeks aktualnej pozycji w menu
	uint8_t current_option;

	// Przechowuje ilosc pozycji w menu
	uint8_t length;

	// Przechowuje pozycje menu
	struct Route {
		// Przechowuje typ pozycji
		enum {
			MENU,
			PROGRAM
		} type;

		// Przechowuje menu docelowe pozycji
		void *destination;

		// Przechowuje opis pozycji
		char label[16];
	} routes[];
};

void menu_render();
void menu_down();
void menu_up();
struct Menu* menu_get_dest();
void menu_navigate(struct Menu* dest);

// Definicja struktury okreslajacej pojedynczy program
// Przechowuje wskazniki funkcji obslugujacych poszczegolne zdarzenia
struct Program {
	void (*on_start)();
	void (*on_stop)();
	void (*on_key)(uint8_t);
	void (*on_tick)();
};

void program_launch(struct Program* program);

// Mapuje kod przycisku na opisujacy go lancuch
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

// Definicja znaku kursora menu
static const char menu_cursor_sign[8] = {
	0b00000,
	0b11000,
	0b11100,
	0b11110,
	0b11110,
	0b11100,
	0b11000,
	0b00000
};

// Pusta funkcja - pozwala na pominiecie zdarzenia
void skip() {}

// ------------------------- PROGRAM 1 ----------------------------

// Wyswietla opis przycisku na ekranie
void show_key(uint8_t keycode) {
	lcd_clear();
	lcd_text(keymap[keycode]);
}

// Inicjalizacja pierwszego programu
static const struct Program program1 = {
	.on_start = &lcd_clear,
	.on_key = &show_key,
	.on_stop = &skip,
	.on_tick = &skip,
};

// ------------------------- PROGRAM 2 ----------------------------

// Program 2 - rozruch
void button_counter_start() {
	LED_DDR = 0xFF;
	LED_PORT = 0x00;

	// Ustawia prescaler zbocza opadajacego, tryb NORMAL
	TCCR0 |= (1 << CS02) | (1 << CS01) | (1 << CS00);

	lcd_clear();

	// Zeruje licznik
	TCNT0 = 0;
}

// Program 2 - deaktywacja ledow i zatrzymanie timera
void button_counter_stop() {
	LED_PORT = 0x00;
	LED_DDR = 0x00;

	// Wylacza Timer0
	TCCR0 &= (0 << CS02) | (0 << CS01) | (0 << CS00);
}

// Program 2 - sterowanie diodami
void button_counter_update() {
	uint8_t counter = TCNT0;

	// Ustawia LEDy w taki sposob aby odzwierciedlaly liczbe nacisniec
	LED_PORT = counter;

	// Sprawdza stan flagi przepelnienia
	if (TIFR & (1 << TOV0)) {

		// Zatrzymuje timer
		TCCR0 &= (0 << CS02) | (0 << CS01) | (0 << CS00);
	}
}

// Inicjalizacja programu 2
static const struct Program button_counter = {
	.on_start = &button_counter_start,
	.on_key = &skip,
	.on_stop = &button_counter_stop,
	.on_tick = &button_counter_update,
};

// ------------------------- PROGRAM 3 ----------------------------
static float seconds_elapsed = 0;
static int stopwatch_seconds = 0;
static uint8_t stopwatch_is_running = 0;

// Start/stop stopera
void stopwatch_toggle() {
	// Neguje stan zatrzymania stopera
	stopwatch_is_running = ~(stopwatch_is_running | 0xfe);

	stopwatch_draw();
}

// Resetuje stoper
void stopwatch_reset() {
	stopwatch_is_running = 0;
	seconds_elapsed = 0;
	stopwatch_seconds = 0;
}

// Wyswietla stoper
void stopwatch_draw() {
	lcd_clear();
	lcd_move_cursor(0, 0);
	lcd_text("Stoper  ");

	// Wyswietla odpowiednia "ikone"
	if (stopwatch_is_running)
		lcd_send(0);
	else
		lcd_text("||");


	// Wyswietla liczbe zliczonych sekund
	lcd_move_cursor(1, 0);
	lcd_number(stopwatch_seconds);
	lcd_text(" sekund");
}

// Inicjalizuje stoper
void stopwatch_start() {
	stopwatch_is_running = 1;
	stopwatch_draw();
}

// Obsluguje przyciski stopera
void stopwatch_key(uint8_t keycode) {
	switch (keycode) {
		case SW_1: stopwatch_toggle(); break;
		case SW_2: 
				stopwatch_reset();
				stopwatch_draw();
			break;
	}
}

// Obsluguje impulsy zegara
void stopwatch_tick() {
	// Jezeli stoper jest zatrzymany, ignoruje impuls
	if (!stopwatch_is_running)
		return;

	// Sumuje czas pomiedzy impulsami
	seconds_elapsed += timer2_tick_period;

	// Jezeli suma czasu pomiedzy impulsami jest wieksza niz 1,
	// oznacza to ze minela sekunda
	if (seconds_elapsed > 1) {
		// Zwieksza licznik wyswietlany przez stoper
		stopwatch_seconds++;

		// Wyswietla stoper
		stopwatch_draw();

		// Resetuje sume czasu pomiedzy impulsami
		seconds_elapsed = 0;
	}
}

// Inicjalizacja programu stopera
static const struct Program stopwatch = {
	.on_start = &stopwatch_start,
	.on_key = &stopwatch_key,
	.on_stop= &stopwatch_reset,
	.on_tick = &stopwatch_tick,
};

// ------------------------- PROGRAM 4 ----------------------------
static volatile uint8_t i;
static volatile uint8_t back;

// Inicjalizuje pokaz ledow
void leds_run() {
	lcd_clear();

	LED_DDR = 0xFF;

	// Ustawia licznik. Zaczynamy od najstarszych bitow.
	i = 7;

	 // Ustawia, czy wyswietlanie "wraca" w strone starszych bitow
	back = 0;
}

// Obsluguje impuls zegara aby wyswietlic nastepny krok ledow
void leds_tick() {
	if(i == 1) back = 1; // Jesli jestesmy przy najmlodszym bicie, zawracamy

	LED_PORT = 0x00;
	LED_PORT^= _BV(i) | _BV(i-1); // Neguje bity ustawione w masce

	if(back) {
		i++; // Inkrementuje licznik
	} else {
		i--; // Dekrementuje licznik
	}

	// Jesli zawracamy i jestesmy przy najstarszym bicie, przestajemy zawracac
	if(back && i == 7) {
		back = 0;
	}
}

// Zatrzymuje pokaz ledow
void leds_stop() {
	LED_DDR = 0x00;
}

// Inicjalizacja programu pokazu ledow
static const struct Program leds = {
	.on_start = &leds_run,
	.on_tick = &leds_tick,
	.on_stop = &leds_stop,
	.on_key = &skip,
};

// Definicje posczegolnych menu
static const struct Menu menu_1 = {
	.length = 2,
	.routes = {
		{PROGRAM, &program1, "Odczyt klawiat."},
		{MENU, NULL, "Program 1.2"},
	},
};

static const struct Menu menu_2 = {
	.length = 2,
	.routes = {
		{PROGRAM, &button_counter, "Licznik przyc."},
		{PROGRAM, &stopwatch, "Stoper"},
	},
};

static const struct Menu menu_3 = {
	.length = 2,
	.routes = {
		{PROGRAM, &leds, "Pokaz LEDow"},
		{MENU, &menu_1, "Menu 1"},
	},
};

static const struct Menu main_menu = {
	.length = 3,
	.routes = {
		{MENU, &menu_1, "Menu 1"},
		{MENU, &menu_2, "Menu 2"},
		{MENU, &menu_3, "Menu 3"},
	},
};

// Inicjalizuje zmienna przechowujaca obecne menu
static struct Menu* current_menu;

// Inicjalizuje zmienna przechowujaca obecny program
static struct Program* current_program = NULL;

// Inicjalizuje zmienna przechowujaca kod wcisnietego przycisku
volatile uint8_t keycode = 0;

// Inicjalizuje zmienna przechowujaca numer biezacej linii LCD
volatile uint8_t cursor_row = 0;

// Obsluguje przerwania wywolane przez Timer 2 w trybie CTC
ISR(TIMER2_COMP_vect) {
	// Odczytuje kod przycisku
    keycode = keypad_read();

	// Jezeli jakis program jest otworzony
	if (program_is_running())
		// Wywoluje funkcje obslugujaca impuls
		current_program->on_tick();
}

int main() {
	// Inicjalizuje LCD
	lcd_init();

	// Inicjalizuje klawiature
	keypad_init();

	// Inicjalizuje timer glowny
	timer2_setup_interrupt(0.01);

	// Tworzy znak kursora menu
	lcd_new_sign(menu_cursor_sign, 0);

	// Przechodzi do menu glownego
	menu_navigate(&main_menu);

	// Reaguje na przycisniecia
    while (1) on_key(keycode);
}

/** Rozroznia poszczegolne wcisniecia przycisku,
	aby wyeliminowac niepozadane duplikacje dzialan */
void on_key(uint8_t keycode) {
	// Inicjalizuje zmienna przechowujaca stan wcisniecia przycisku
	static uint8_t key_pressed = 0;

	// Sprawdza czy ktorykolwiek przycisk jest wcisniety i czy przycisk
	// nie jest trzymany
	if (keycode > 0 && !key_pressed) {
		// Okresla stan przycisku jako przytrzymany
		key_pressed = 1;

		// Rozpoczyna obsluge przycisku
		handle_key(keycode);

	} else if (keycode == 0 && key_pressed) {
		// Jezeli zaden przycisk nie jest wcisniety
		// i jakis przycisk jest okreslony jako przytrzymany
		// mozna uznac ze przycisk zostal puszczony
		key_pressed = 0;
	}
}

// Uruchamia okreslony program
void program_launch(struct Program* program) {
	// Ustawia okreslony program jako obecnie otwarty
	current_program = program;

	// Wywoluje funkcje obslugujaca rozruch programu
	current_program->on_start();
}

// Zamyka obecny program
void program_close() {
	// Wywoluje funkcje obslugujaca zamykanie programu
	current_program->on_stop();

	// Czysci wskaznik do obecnego programu
	current_program = NULL;

	// Wyswietla menu
	menu_render();
}

// Sprawdza czy jakis program jest otwarty
int program_is_running() {
	return current_program != NULL;
}

// Uruchamia wybrana pozycje menu
void menu_advance() {
	struct Route route = current_menu->routes[current_menu->current_option];

	// Jesli pozycja wskazuje na menu, przechodzi do tego menu
	// Jesli pozycja wskazuje na program, uruchamia ten program
	switch (route.type) {
		case MENU: menu_navigate(route.destination); break;
		case PROGRAM: program_launch(route.destination); break;
	}
}

/** Przeprowadza akcje w zaleznosci od kodu przycisku */
void handle_key(uint8_t keycode) {
	if (program_is_running()) {
		// Jezeli program dziala, zamyka program lub
		// wywoluje funkcje obslugujaca przycisk klawiatury
		switch (keycode) {
			case KEY_CLEAR: program_close(); break;
			default: current_program->on_key(keycode);
		}			
	} else {
		// Jezeli zaden program nie dziala, obsluguje nawigacje w menu
		switch (keycode) {
			case KEY_UP: menu_up(); break;
			case KEY_DOWN: menu_down(); break;
			case KEY_CLEAR: menu_navigate(&main_menu); break;
			case KEY_ENTER: menu_advance(); break;
		}
	}
}

/** Przenosi kursor menu nizej */
void menu_down() {
	// Jezeli wybrana pozycja nie jest ostatnia, wybiera linie ponizej
	if (current_menu->current_option < current_menu->length - 1) {
		current_menu->current_option++;
		menu_render();
	}
}

/** Przenosi kursor menu wyzej */
void menu_up() {
	// Jezeli wybrana pozycja nie jest pierwsza, wybiera linie powyzej
	if (current_menu->current_option > 0) {
		current_menu->current_option--;
		menu_render();
	}
}

/** Zwraca menu docelowe obecnie wybranej pozycji */
struct Menu* menu_get_dest() {
	return current_menu->routes[current_menu->current_option].destination;
}

/** Przechodzi do okreslonego menu */
void menu_navigate(struct Menu* dest) {
	// Sprawdza czy okreslone menu jest zdefiniowane
	if (dest != NULL) {
		// Podmienia obecne menu na okreslone w parametrze
		current_menu = dest;

		// Ustawia kursor menu na pierwszej pozycji
		current_menu->current_option = 0;

		// Wyswietla nowe menu
		menu_render();
	}
}

// Przechowuje indeks pozycji menu, ktora jest wyswietlona
// w pierwszej linii LCD
static uint8_t first_option = 0;

/** Wyswietla obecne menu na LCD */
void menu_render() {
	// Czysci ekran
	lcd_cmd(LCD_CLEAR);

	// Jezeli zostala wybrana pozycja, ktora nie jest obecnie widoczna na LCD,
	// zmienia indeks linii ktora powinna byc wyswietlona jako pierwsza
	if (first_option > current_menu->current_option || first_option + 1 < current_menu->current_option)
		first_option = current_menu->current_option == 0 ? 0 : current_menu->current_option - 1;

	// Iteruje po liniach LCD
	for (uint8_t row = 0; row <= 1; row++) {

		// Jesli potrzebne pozycje sa juz widoczne, konczy algorytm
		if (current_menu->length < row + first_option)
			break;

		// Przenosi kursor na poczatek linii
		lcd_move_cursor(row, 0);

		// Jesli pozycja jest obecnie wybrana, ustawia znak kursora na poczatku linii
		if (row == current_menu->current_option - first_option)
			lcd_send(0);
		else
			lcd_send(' ');

		// Wypisuje opis pozycji
		lcd_text(current_menu->routes[row + first_option].label);
	}
}

/** Wypisuje tekst */
void lcd_text(char *chars) {
	// Iteruje po znakach we wskazanym lancuchu
	for (uint8_t i = 0; chars[i]; i++) {
		// Jezeli brakuje miejsca w linii, przechodzi do nastepnej
		if (i==16)
			lcd_move_cursor(1,0);

		// Wypisuje znak
		lcd_send(chars[i]);
	}
}

/** Wypisuje liczbe z zakresu 0-19 */
void lcd_number(uint8_t num) {
	if (num > 9) {
		lcd_send(num / 10 + '0');
		lcd_send(num % 10 + '0');
	} else {
		lcd_send(num + '0');
		lcd_send(' ');
	}
}

void lcd_clear() {
	lcd_cmd(LCD_CLEAR);
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

/** Konfiguruje Timer 2 w taki sposob aby generowal impuls
	co czesc sekundy okreslona w argumencie */
void timer2_setup_interrupt(float period) {
	// Ustawia Timer 2 w tryb CTC
    TCCR2 |= (1 << WGM21) | (0 << WGM20);

	// Ustawia preskaler 1/64
    TCCR2 |= (1 << CS22);

	// Oblicza liczbe impulsow, po ktorej nastepuje przerwanie
    int counter_top = F_CPU / 64 * period - 1;

	// Sprawdza czy obecny preskaler pozwala na obsluzenie wybranego okresu
	assert(counter_top < 256, "Invalid prescaler");

	// Ustawia liczbe impulsow, po ktorej nastepuje przerwanie 
	OCR2 = counter_top;

	// Okresla okres impulsow na potrzeby stopera
	timer2_tick_period = period;

	// Resetuje stan licznika
    TCNT2 = 0;

	// Aktywuje przerwania Timera 2 w trybie CTC
    TIMSK |= (1 << OCIE2);

	// Aktywuje obsluge przerwan
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

	// Petla iteruje po indeksach kolumn
    for (uint8_t col = 0; col <= 3; col++) {
		// Ustawia kalwiature w sposob umozliwajacy odczyt danej kolumny:
		// Bity odpowiadajace kolumnom zaczynaja sie od bitu czwartego
        PORTA = ~(1 << (col + 4));

		// Odczytuje stan kolumny, uwzgledniajac jedynie istotne bity
        col_state = PINA & 0x0f;

		// Sprawdza, czy w kolumnie jest wcisniety przycisk
        if (col_state < 0x0f)
			/*
			Zwraca kod przycisku
		 		- Funkcja `__builtin_ctz` (count trailing zeros) zwraca liczbe zer z prawej strony najmlodszego ustawionego bitu
				- Wyrazenie `__builtin_ctz(~col_state) * 4` odpowiada za przejscie do odpowiedniego wiersza
				- Wyrazenie `+ col` odpowiada za przejscie do odpowiedniego przycisku w wierszu
				- Wyrazenie `+ 1` ustawia kod pierwszego przycisku na `1`,
				  bo `0` jest zarezerwowane na brak wcisnietego przycisku
			*/
            return __builtin_ctz(~col_state) * 4 + col + 1;

    }

	// Zaden wcisniety przycisk nie zostal wykryty w petli
    return 0;
}

// Pozwala na sprawdzenie blednych warunkow
void assert(uint8_t condition, char* message) {
	if (condition != 1) {
		lcd_text(strcat("E:", message));
		_delay_ms(1000);
	}
}
