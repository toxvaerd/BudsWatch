/*
 * BawsClock.c
 *
 * Created: 17-02-2012 19:17:59
 *  Author: Morten
 */ 
#define F_CPU 8000000UL // 8Mhz Crystal Oscillator

#define PRECOUNT		10

#define DIGIT0			0
#define DIGIT1			1
#define DIGIT2			2
#define DIGIT3			3

#define STATE_SELECT	0
#define STATE_CONFIGURE 1
#define STATE_RUNNING	2
#define STATE_FINISHED	3

#define MODE_COUNT		3
#define MODE_STOPWATCH	1	// Stopwatch: Count from 0
#define MODE_TIMER		2	// Count down from selected time
#define MODE_INTERVAL	3	// x sec work, y sec pause, z rounds
#define MODE_TABATA		4	// 20 sec work, 10 sec pause, 8 rounds

#define KEY0_MASK		(1 << PD0)
#define KEY1_MASK		(1 << PD1)
#define KEY2_MASK		(1 << PD2)
#define KEY3_MASK		(1 << PD3)

#define KEY_PIN			PIND
#define KEY_PORT		PORTD
#define KEY_DDR			DDRD

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <stdbool.h>

// Define structs
typedef struct {
	uint8_t digits[4];
	uint8_t showdigits;
	uint8_t dots;
} seven_segment_state;

typedef struct {
	uint8_t Seconds;
	uint8_t Minutes;
	uint8_t State;
	uint8_t Mode;
	bool CountDown;
	int PreCount;
} clock_state;

// Global variables
seven_segment_state ssState;
clock_state clockState;

volatile uint8_t SecondElapsed  = false;
static volatile uint8_t key_press;

// Function prototypes
extern double floor(double x);
uint8_t digitToSevenSegment(uint8_t digit);
void showDigit(uint8_t digit, uint8_t port);

int main (void) 
{ 
	
	clockState.Seconds    = 0;
	clockState.Minutes    = 0;
	clockState.State      = STATE_SELECT;     // Initial state is IDLE
	clockState.Mode	      = MODE_STOPWATCH;
	clockState.CountDown  = false;
	clockState.PreCount   = PRECOUNT;
	
	/* SET UP I/O */
	DDRA = 0xFF;		                   // Enable all port A LEDs
	DDRB = (1 << PB0) 
		 | (1 << PB1) 
		 | (1 << PB2) 
		 | (1 << PB3);                     // Enable 4 7 segment displays
	KEY_DDR  = 0;		                   // Set keys as input input
	KEY_PORT = 0xFF;	                   // Pull-ups on

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
		switch (clockState.State)
		{
		    case STATE_SELECT:
				if (key_press & KEY0_MASK)
				{
					clockState.State = STATE_RUNNING;
					key_press ^= KEY0_MASK;
				}
				if (key_press & KEY1_MASK) {
					if (++clockState.Mode > MODE_COUNT) clockState.Mode = MODE_STOPWATCH;
					key_press ^= KEY1_MASK;
				}
				if (key_press & KEY2_MASK) {
					if (--clockState.Mode < 1) clockState.Mode = MODE_COUNT;
					key_press ^= KEY2_MASK;
				}
			
				ssState.showdigits = (1 << DIGIT0);
				ssState.digits[DIGIT0] = clockState.Mode;
				
				if (SecondElapsed > 0) {
					ssState.dots ^= (1 << DIGIT0);
					SecondElapsed--;
				}
				break;
			//case STATE_CONFIGURE:
				//switch (Mode)
				//{
				    //case MODE_STOPWATCH:
				        //
				        //break;
				    //default:
				        ///* Your code here */
				        //break;
				//}
			case STATE_RUNNING:
				if (SecondElapsed > 0) {
					SecondElapsed--;
					if (clockState.PreCount > 0) {
						ssState.digits[DIGIT1] = floor(clockState.PreCount / 10);
						ssState.digits[DIGIT0] = clockState.PreCount % 10;
						ssState.showdigits = (1 << DIGIT0) | (clockState.PreCount > 9 ? (1 << DIGIT1) : 0);
						ssState.dots = (clockState.PreCount % 2 == 0 ? (1 << DIGIT0) : 0);
						clockState.PreCount--;
					} else {
						ssState.showdigits = (1 << DIGIT0) | (1 << DIGIT1) | (1 << DIGIT2) | (1 << DIGIT3);
						ssState.digits[DIGIT3] = floor(clockState.Minutes / 10);
						ssState.digits[DIGIT2] = clockState.Minutes % 10;
						ssState.digits[DIGIT1] = floor(clockState.Seconds / 10);
						ssState.digits[DIGIT0] = clockState.Seconds % 10;
						if (clockState.Seconds % 2 == 0) ssState.dots = (1 << DIGIT2);
						else ssState.dots = 0;
						
						if (clockState.CountDown) {
							clockState.Seconds--;
							if (clockState.Seconds >= 60) {
								if (clockState.Minutes > 0) { 
									clockState.Minutes--;
									clockState.Seconds = 59;
								} else {
									clockState.Seconds = 0;
								}						
					
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
				}
			default:
				// Do nothing
				break;
		}
		
		showDigit(DIGIT3, PB3);
		showDigit(DIGIT2, PB2);
		showDigit(DIGIT1, PB1);
		showDigit(DIGIT0, PB0);
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