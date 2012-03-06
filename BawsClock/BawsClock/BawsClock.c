/*
 * BawsClock.c
 *
 * Created: 17-02-2012 19:17:59
 *  Author: Morten
 */ 
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <stdbool.h>

#define F_CPU 8000000UL // 8Mhz Crystal Oscillator

#define PRECOUNT			10
#define BUZZER_LONG			10
#define BUZZER_SHORT		5  // x~2ms = ~100ms
#define DEFAULT_BUZZCOUNT	4

#define DIGIT0				0
#define DIGIT1				1
#define DIGIT2				2
#define DIGIT3				3

#define KEY0_MASK			(1 << PD0)
#define KEY1_MASK			(1 << PD1)
#define KEY2_MASK			(1 << PD2)
#define KEY3_MASK			(1 << PD3)

#define KEY_PIN				PIND
#define KEY_PORT			PORTD
#define KEY_DDR				DDRD

// Define enums
typedef enum {
	STATE_SELECT,
	STATE_CONFIGURE,
	STATE_PRECOUNT,
	STATE_RUNNING,
	STATE_FINISHED
} state;

typedef enum {
	MODE_STOPWATCH = 1,	// Stopwatch: Count from 0
	MODE_TIMER = 2,     // Count down from selected time
	MODE_INTERVAL = 3,  // x sec work, y sec pause, z rounds
	MODE_TABATA = 4,    // 20 sec work, 10 sec pause, 8 rounds
	MODE_FGB = 5,
	MODE_LAST = MODE_FGB
} mode;

typedef enum {
	CONF_WORK_MINUTES,
	CONF_WORK_SECONDS,
	CONF_REST_MINUTES,
	CONF_REST_SECONDS,
	CONF_ROUNDS,
	CONF_LAST = CONF_ROUNDS
} interval_configure;

// Define structs
typedef struct {
	uint8_t Minutes;
	uint8_t Seconds;
} clock;

typedef struct {
	uint8_t digits[4];
	uint8_t showdigits;
	uint8_t dots;
} seven_segment_state;

typedef struct {
	clock Work;
	clock Pause;
	uint8_t RoundsWork;
	uint8_t RoundsPause;
} interval_timer;

// Global variables
seven_segment_state ssState;

volatile uint8_t SecondElapsed  = false;
volatile uint8_t TickCounter = 0;
static volatile uint8_t Buzzer = 0;
static volatile uint8_t BuzzCount = 0;
static volatile uint8_t key_press;

// Function prototypes
extern double floor(double x);
uint8_t digitToSevenSegment(uint8_t digit);
void showDigit(uint8_t digit, uint8_t port);
bool detectKeypress(uint8_t mask);
void UpdateBuzzer();

int main (void) 
{ 
	clock clockState;
	interval_timer intervalState;
	interval_configure intervalConfiguration = CONF_WORK_MINUTES;

	mode Mode           = MODE_STOPWATCH;
	state State			= STATE_SELECT;
	uint8_t PreCount    = PRECOUNT;
	uint8_t BuzzCount   = 0;
	bool Interval       = false;
	uint8_t Minutes     = 0; // Configure
	uint8_t Seconds     = 0; // Configure
	
	/* SET UP I/O */
	DDRA = 0xFF;		                   // Enable all port A LEDs
	DDRB = (1 << PB0) 
		 | (1 << PB1) 
		 | (1 << PB2) 
		 | (1 << PB3);                     // Enable 4 7 segment displays
	DDRC = (1 << PC0);
	KEY_DDR  = 0;		                   // Set keys as input input
	KEY_PORT = (1 << PD2) | 
			   (1 << PD1) | 
			   (1 << PD0);	               // Pull-ups on

	/* SET UP TIMERS */
	// Timer 0: Debouncing 2ms
	TCCR0 |= (1 << CS00) | (1 << CS02);    // Prescaling 1024
		
	// Timer 1: Count seconds
	TCCR1B |= (1 << WGM12);                // Enable CTC in timer 1's control register
	OCR1AH = 0x7A;                         // Compare to 31249
	OCR1AL = 0x11;                         // -||-
	TCCR1B |= (1 << CS12);                 // Set up timer prescaling at Fcpu/256

	TIMSK |= (1 << OCIE1A) | (1 << TOIE0); // Output Compare Interrup Enable on timer 1 channel A and timer 0
	
	sei();
	for (;;) 
	{ 
		switch (State)
		{
		    case STATE_SELECT:
				if (detectKeypress(KEY0_MASK))
				{					
					switch (Mode)
					{
					    case MODE_STOPWATCH:
							clockState.Minutes          = 0;
							clockState.Seconds          = 0;
							Interval = false;
							State = STATE_PRECOUNT;
							break;
						case MODE_TIMER:
							clockState.Minutes			= 0;
							clockState.Seconds			= 0;
							intervalState.Work.Minutes  = 1;
							intervalState.Work.Seconds  = 0;
							intervalState.Pause.Minutes = 1;
							intervalState.Pause.Seconds = 0;
							intervalState.RoundsWork    = 1;
							intervalState.RoundsPause   = 0;
							Interval				    = true;
							State = STATE_CONFIGURE;
							break;
						case MODE_INTERVAL:
							clockState.Minutes			= 0;
							clockState.Seconds			= 0;
							intervalState.Work.Minutes  = 1;
							intervalState.Work.Seconds  = 0;
							intervalState.Pause.Minutes = 1;
							intervalState.Pause.Seconds = 0;
							intervalState.RoundsWork    = 1;
							intervalState.RoundsPause   = 0;							
							Interval					= true;
							State = STATE_CONFIGURE;
							break;
						case MODE_TABATA:
							clockState.Minutes			= 0;
							clockState.Seconds			= 0;
							intervalState.Work.Minutes  = 0;
							intervalState.Work.Seconds  = 20;
							intervalState.Pause.Minutes = 0;
							intervalState.Pause.Seconds = 10;
							intervalState.RoundsWork    = 8;
							intervalState.RoundsPause   = 8;							
							Interval					= true;
							State = STATE_PRECOUNT;
							break;
						case MODE_FGB:
							clockState.Minutes			= 0;
							clockState.Seconds			= 0;
							intervalState.Work.Minutes  = 1;
							intervalState.Work.Seconds  = 0;
							intervalState.Pause.Minutes = 0;
							intervalState.Pause.Seconds = 0;
							intervalState.RoundsWork    = 18;
							intervalState.RoundsPause   = 0;							
							Interval					= true;
							State = STATE_PRECOUNT;
							break;
						default:
					        break;
					}
				}
				if (detectKeypress(KEY1_MASK)) {
					if (++Mode > MODE_LAST) Mode = MODE_STOPWATCH;
				}
				if (detectKeypress(KEY2_MASK)) {
					if (--Mode < 1) Mode = MODE_LAST;
				}
			
				ssState.showdigits = (1 << DIGIT0);
				ssState.digits[DIGIT0] = Mode;
				
				if (SecondElapsed > 0) {
					ssState.dots ^= (1 << DIGIT0);
					SecondElapsed--;
				}
				break;
			case STATE_CONFIGURE:
				switch (Mode)
				{
					case MODE_TIMER:
						if (detectKeypress(KEY0_MASK)) {
							State = STATE_PRECOUNT;							
						}
						if (detectKeypress(KEY1_MASK)) {
							if (intervalState.Work.Minutes >= 59) intervalState.Work.Minutes = 0;
							else intervalState.Work.Minutes++;							
						}
						if (detectKeypress(KEY2_MASK)) {
							if (intervalState.Work.Minutes == 0) intervalState.Work.Minutes = 59;
							else intervalState.Work.Minutes--;
						}
						Minutes = intervalState.Work.Minutes;
						Seconds = intervalState.Work.Seconds;
						ssState.showdigits = (1 << DIGIT3) | (1 << DIGIT2);
						break;
					case MODE_INTERVAL:
						switch (intervalConfiguration) {
							case CONF_WORK_MINUTES:
								if (detectKeypress(KEY0_MASK)) {
									intervalConfiguration++;
								}
								if (detectKeypress(KEY1_MASK)) {
									if (intervalState.Work.Minutes >= 59) intervalState.Work.Minutes = 0;
									else intervalState.Work.Minutes++;
								}
								if (detectKeypress(KEY1_MASK)) {
									if (intervalState.Work.Minutes == 0) intervalState.Work.Minutes = 59;
									else intervalState.Work.Minutes--;
								}
								Minutes = intervalState.Work.Minutes;
								Seconds = intervalState.Work.Seconds;
								ssState.showdigits = (1 << DIGIT3) | (1 << DIGIT2);
								break;
							case CONF_WORK_SECONDS:
								if (detectKeypress(KEY0_MASK)) {
									intervalConfiguration++;
								}
								if (detectKeypress(KEY1_MASK)) {
									if (intervalState.Work.Seconds >= 59) intervalState.Work.Seconds = 0;
									else intervalState.Work.Seconds++;
								}
								if (detectKeypress(KEY1_MASK)) {
									if (intervalState.Work.Seconds == 0) intervalState.Work.Seconds = 59;
									else intervalState.Work.Seconds--;
								}
								Minutes = intervalState.Work.Minutes;
								Seconds = intervalState.Work.Seconds;
								ssState.showdigits = (1 << DIGIT0) | (1 << DIGIT1);
								break;
							case CONF_REST_MINUTES:
								if (detectKeypress(KEY0_MASK)) {
									intervalConfiguration++;
								}
								if (detectKeypress(KEY1_MASK)) {
									if (intervalState.Pause.Minutes >= 59) intervalState.Pause.Minutes = 0;
									else intervalState.Pause.Minutes++;
								}
								if (detectKeypress(KEY1_MASK)) {
									if (intervalState.Pause.Minutes == 0) intervalState.Pause.Minutes = 59;
									else intervalState.Pause.Minutes--;
								}
								Minutes = intervalState.Pause.Minutes;
								Seconds = intervalState.Pause.Seconds;
								ssState.showdigits = (1 << DIGIT3) | (1 << DIGIT2);
								break;
							case CONF_REST_SECONDS:
								if (detectKeypress(KEY0_MASK)) {
									intervalConfiguration++;
								}
								if (detectKeypress(KEY1_MASK)) {
									if (intervalState.Pause.Seconds >= 59) intervalState.Pause.Seconds = 0;
									else intervalState.Pause.Seconds++;
								}
								if (detectKeypress(KEY1_MASK)) {
									if (intervalState.Pause.Seconds == 0) intervalState.Pause.Seconds = 59;
									else intervalState.Pause.Seconds--;
								}
								Minutes = intervalState.Pause.Minutes;
								Seconds = intervalState.Pause.Seconds;
								ssState.showdigits = (1 << DIGIT0) | (1 << DIGIT1);
								break;
							case CONF_ROUNDS:
								if (detectKeypress(KEY0_MASK)) {
									State = STATE_PRECOUNT;
								}
								if (detectKeypress(KEY1_MASK)) {
									if (intervalState.RoundsWork >= 99) {
										intervalState.RoundsWork = 0;
										intervalState.RoundsPause = 0;
									} else {
										intervalState.RoundsWork++;
										if (intervalState.Pause.Minutes > 0 || intervalState.Pause.Seconds > 0) intervalState.RoundsPause++;
									}										
								}
								if (detectKeypress(KEY1_MASK)) {
									if (intervalState.RoundsWork == 0) {
										intervalState.RoundsWork = 99;
										if (intervalState.Pause.Minutes > 0 || intervalState.Pause.Seconds > 0) intervalState.RoundsPause = 99;
									} else {
										intervalState.RoundsWork--;
										if (intervalState.Pause.Minutes > 0 || intervalState.Pause.Seconds > 0) intervalState.RoundsPause--;
									}	
								}
								Minutes = intervalState.RoundsWork;
								Seconds = intervalState.RoundsPause;
								ssState.showdigits = (1 << DIGIT3) | (1 << DIGIT2);
								
								break;
							default:
								break;
						}
						break;
				    default:
				        ssState.showdigits = 0;
				        break;
				}
				ssState.digits[DIGIT3] = floor(Minutes / 10);
				ssState.digits[DIGIT2] = Minutes % 10;
				ssState.digits[DIGIT1] = floor(Seconds / 10);
				ssState.digits[DIGIT0] = Seconds % 10;
				ssState.dots = 0;
				break;
			case STATE_PRECOUNT:
				if (SecondElapsed > 0) {
					SecondElapsed--;
					
					ssState.digits[DIGIT1] = floor(PreCount / 10);
					ssState.digits[DIGIT0] = PreCount % 10;
					ssState.showdigits = (1 << DIGIT0) | (PreCount > 9 ? (1 << DIGIT1) : 0);
					ssState.dots = (PreCount % 2 == 0 ? (1 << DIGIT0) : 0);
					if (PreCount == DEFAULT_BUZZCOUNT-1) BuzzCount = DEFAULT_BUZZCOUNT;
					UpdateBuzzer();
					PreCount--;
					if (PreCount == 0) State = STATE_RUNNING;
				}
				break;
			case STATE_RUNNING:
				if (SecondElapsed > 0) {
					SecondElapsed--;
					
					UpdateBuzzer();
					
					if (Interval && clockState.Minutes == 0 && clockState.Seconds == 0)
					{
						if (intervalState.RoundsPause > intervalState.RoundsWork)
						{
							clockState.Minutes = intervalState.Pause.Minutes;
							clockState.Seconds = intervalState.Pause.Seconds;
							intervalState.RoundsPause--;
						}
						else if (intervalState.RoundsWork > 0)
						{
							clockState.Minutes = intervalState.Work.Minutes;
							clockState.Seconds = intervalState.Work.Seconds;
							intervalState.RoundsWork--;
						}
						else
						{
							// sleep?
						}
					}

					ssState.showdigits = (1 << DIGIT0) | (1 << DIGIT1) | (1 << DIGIT2) | (1 << DIGIT3);
					if (clockState.Seconds % 2 == 1) ssState.dots = (1 << DIGIT2);
					else ssState.dots = 0;
					
					// BUDS
					if (clockState.Minutes == 0 && clockState.Seconds == 0) {
						ssState.digits[DIGIT3] = 0b011111111; // B
						ssState.digits[DIGIT2] = 0b001111110; // U
						ssState.digits[DIGIT1] = 0b011111110; // D
						ssState.digits[DIGIT0] = 0b011001101; // S
					} else {
						if (Mode == MODE_TABATA) {
							ssState.showdigits = (1 << DIGIT0) | (1 << DIGIT1) |  (1 << DIGIT3);
							ssState.digits[DIGIT3] = intervalState.RoundsPause;
							ssState.digits[DIGIT2] = 0;
							ssState.digits[DIGIT1] = floor(clockState.Seconds / 10);
							ssState.digits[DIGIT0] = clockState.Seconds % 10;	
						} else {
							ssState.digits[DIGIT3] = floor(clockState.Minutes / 10);
							ssState.digits[DIGIT2] = clockState.Minutes % 10;
							ssState.digits[DIGIT1] = floor(clockState.Seconds / 10);
							ssState.digits[DIGIT0] = clockState.Seconds % 10;
						}
					}
						
					if (Interval) {
						clockState.Seconds--;
						if (clockState.Seconds >= 60) {
							if (clockState.Minutes > 0) { 
								clockState.Minutes--;
								clockState.Seconds = 59;
							} else {
								clockState.Seconds = 0;
							}						
					
						}
						if (clockState.Minutes == 0 && clockState.Seconds == DEFAULT_BUZZCOUNT-1) {
							BuzzCount = DEFAULT_BUZZCOUNT;
						}
					}
					else
					{
						clockState.Seconds++;
						if (clockState.Seconds >= 60) {
							clockState.Seconds = 0;
							clockState.Minutes++;
							if (clockState.Minutes > 59) clockState.Minutes = 0;
						}
					}
					
				}
				break;
			default:
				// Do nothing
				break;
		}
		
		showDigit(DIGIT3, PB3);
		showDigit(DIGIT2, PB2);
		showDigit(DIGIT1, PB1);
		showDigit(DIGIT0, PB0);
		
		if (Buzzer > 0)
		{
			PORTC |= (1 << PC0);
		}
		else
		{
			PORTC &= (0 << PC0);
		}
	} 
}

// Timer 1 interrupt (1 sec)
ISR(TIMER1_COMPA_vect) {
	SecondElapsed++;
}

// Timer 0 interrupt
ISR(TIMER0_OVF_vect) {
	static uint8_t key_state;		// debounced and inverted key state:
	static uint8_t ct0, ct1;      // holds two bit counter for each key
	uint8_t i;

	if (Buzzer > 0)	Buzzer--;

	/*
	* read current state of keys (active-low),
	* clear corresponding bit in i when key has changed
	*/
	i = key_state ^ ~KEY_PIN;   // key changed ?
  
	/* 
	* ct0 and ct1 form a two bit counter for each key,  
	* where ct0 holds LSB and ct1 holds MSB
	* After a key is pressed longer than four times the
	* sampling period, the corresponding bit in key_state is set
	*/
	ct0 = ~( ct0 & i );			// reset or count ct0
	ct1 = (ct0 ^ ct1) & i;	    // reset or count ct1  
	i &= ct0 & ct1;			    // count until roll over ?
	key_state ^= i;			    // then toggle debounced state
  
	/*
	* To notify main program of pressed key, the correspondig bit
	* in global variable key_press is set.
	* The main loop needs to clear this bit
	*/
	key_press |= key_state & i;	// 0->1: key press detect
}

uint8_t digitToSevenSegment(uint8_t digit) {
	switch (digit)
	{
	    case 0: return 0x3F;
		case 1: return 0x06;
		case 2: return 0x5B;
		case 3: return 0x4F;
		case 4: return 0x66;
		case 5: return 0x6D;
		case 6: return 0x7D;
		case 7: return 0x07;
		case 8: return 0x7F;
		case 9: return 0x6F;
	    default:
			return 0x00;
		    break;
	}
}

void showDigit(uint8_t digit, uint8_t port) {
	if (ssState.showdigits & (1 << digit)) {
		PORTA = 0; // Avoid ghosting
		PORTB = (1 << port);
		PORTA = digitToSevenSegment(ssState.digits[digit]) | (ssState.dots & (1 << digit) ? 1 << PA7 : 0);		
		_delay_us(50);
	}
}

bool detectKeypress(uint8_t mask) {
	if (key_press & mask) {
		key_press ^= mask;
		return true;
	}
	return false;
}

void UpdateBuzzer() {
	if (BuzzCount > 1) {
		Buzzer = BUZZER_SHORT;
		BuzzCount--;
	}
	else if (BuzzCount == 1) {
		Buzzer = BUZZER_LONG;
		BuzzCount--;
	}
}