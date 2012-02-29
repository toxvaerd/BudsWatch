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
#define MODE_TABATA		3	// 20 sec work, 10 sec pause, 8 rounds

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

volatile bool SecondElapsed  = false;

uint8_t digits[4]  = {0,0,0,0};
uint8_t showdigits = 0;
uint8_t dots	   = 0;

static volatile uint8_t key_press;

ISR(TIMER1_COMPA_vect) {
	SecondElapsed = true;
}

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
	if (showdigits & (1 << digit)) {
		PORTA = 0;
		PORTB = (1 << port);
		PORTA = digitToSevenSegment(digits[digit]) | (dots & (1 << digit) ? 1 << PA7 : 0);		
		_delay_us(50);
	}
}

int main (void) 
{ 
	uint8_t Seconds    = 0;
	uint8_t Minutes    = 0;
	uint8_t State      = STATE_SELECT; // Initial state is IDLE
	uint8_t Mode	   = MODE_STOPWATCH;
	bool    CountDown  = false;
	int     PreCount   = PRECOUNT;
	
	/* SET UP I/O */
	DDRA = 0xFF;		// Enable all port A LEDs
	DDRB = (1 << PB0) 
		 | (1 << PB1) 
		 | (1 << PB2) 
		 | (1 << PB3);  // Enable 4 7 segment displays
	KEY_DDR  = 0;		// Set keys as input input
	KEY_PORT = 0xFF;	// Pull-ups on

	/* SET UP TIMERS */
	// Timer 0: Debouncing 2ms
	TCCR0 |= (1 << CS00) | (1 << CS02); // Prescaling 1024
		
	// Timer 1: Count seconds
	TCCR1B |= (1 << WGM12); // Enable CTC in timer 1's control register
	OCR1AH = 0x7A; // Compare to 31249
	OCR1AL = 0x11; // -||-
	TCCR1B |= (1 << CS12); // Set up timer prescaling at Fcpu/256

	TIMSK |= (1 << OCIE1A) | (1 << TOIE0); // Output Compare Interrup Enable on timer 1 channel A and timer 0
	
	sei();
	for (;;) 
	{ 
		switch (State)
		{
		    case STATE_SELECT:
				if (key_press & KEY0_MASK)
				{
					State = STATE_RUNNING;
					key_press ^= KEY0_MASK;
				}
				if (key_press & KEY1_MASK) {
					if (++Mode > MODE_COUNT) Mode = MODE_STOPWATCH;
					key_press ^= KEY1_MASK;
				}
				if (key_press & KEY2_MASK) {
					if (--Mode < 1) Mode = MODE_COUNT;
					key_press ^= KEY2_MASK;
				}
			
				showdigits = (1 << DIGIT0);
				digits[DIGIT0] = Mode;
				
				if (SecondElapsed) {
					dots ^= (1 << DIGIT0);
					SecondElapsed = false;
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
				if (SecondElapsed) {
					SecondElapsed = 0;
					if (PreCount > 0) {
						digits[DIGIT1] = floor(PreCount / 10);
						digits[DIGIT0] = PreCount % 10;
						showdigits = (1 << DIGIT0) | (PreCount > 9 ? (1 << DIGIT1) : 0);
						dots = (PreCount % 2 == 0 ? (1 << DIGIT0) : 0);
						PreCount--;
					} else {
						showdigits = (1 << DIGIT0) | (1 << DIGIT1) | (1 << DIGIT2) | (1 << DIGIT3);
						digits[DIGIT3] = floor(Minutes / 10);
						digits[DIGIT2] = Minutes % 10;
						digits[DIGIT1] = floor(Seconds / 10);
						digits[DIGIT0] = Seconds % 10;
						if (Seconds % 2 == 0) dots = (1 << DIGIT2);
						else dots = 0;
						
						if (CountDown) {
							Seconds--;
							if (Seconds >= 60) {
								if (Minutes > 0) { 
									Minutes--;
									Seconds = 59;
								} else {
									Seconds = 0;
								}						
					
							}
						}
						else
						{
							Seconds++;
							if (Seconds >= 60) {
								Seconds = 0;
								Minutes++;
								if (Minutes > 59) Minutes = 0;
							}
						}
					}
				}
			default:
				// Do nothing
				break;
		}
		
		//if (key_press & KEY0_MASK) {
			//if (CountDown) CountDown = 0;
			//else CountDown = 1;
			//key_press = 0;
		//}
		//
		//if (SecondElapsed) {
			//if (CountDown) {
				//Seconds--;
				//if (Seconds >= 60) {
					//if (Minutes > 0) { 
						//Minutes--;
						//Seconds = 59;
					//} else {
						//Seconds = 0;
					//}						
					//
				//}
			//}
			//else
			//{
				//Seconds++;
				//if (Seconds >= 60) {
					//Seconds = 0;
					//Minutes++;
					//if (Minutes > 59) Minutes = 0;
				//}
			//}
			//SecondElapsed = false;
			//
			//
		//}
		
		//unsigned char digit3 = floor(Minutes / 10);
		//unsigned char digit2 = Minutes % 10;
		//unsigned char digit1 = floor(Seconds / 10);
		//unsigned char digit0 = Seconds % 10;
		showDigit(DIGIT3, PB3);
		showDigit(DIGIT2, PB2);
		showDigit(DIGIT1, PB1);
		showDigit(DIGIT0, PB0);
	} 
}