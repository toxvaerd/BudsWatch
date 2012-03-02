/*
 * BawsClock.c
 *
 * Created: 17-02-2012 19:17:59
 *  Author: Morten
 */ 
#define F_CPU 8000000UL // 8Mhz Crystal Oscillator

#define PRECOUNT		10
#define BUZZER_LONG		1000000
#define BUZZER_SHORT	500000

#define DIGIT0			0
#define DIGIT1			1
#define DIGIT2			2
#define DIGIT3			3

#define STATE_SELECT	0
#define STATE_CONFIGURE 1
#define STATE_PRECOUNT  2
#define STATE_RUNNING	3
#define STATE_FINISHED	4

#define MODE_COUNT		4
#define MODE_STOPWATCH	1	// Stopwatch: Count from 0
#define MODE_TIMER		2	// Count down from selected time
#define MODE_TABATA		3	// 20 sec work, 10 sec pause, 8 rounds
#define MODE_INTERVAL	4	// x sec work, y sec pause, z rounds

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
static volatile uint8_t key_press;

// Function prototypes
extern double floor(double x);
uint8_t digitToSevenSegment(uint8_t digit);
void showDigit(uint8_t digit, uint8_t port);

int main (void) 
{ 
	clock clockState;
	interval_timer intervalState;

	uint8_t Mode        = MODE_STOPWATCH;
	uint8_t State       = STATE_SELECT;
	uint8_t PreCount    = PRECOUNT;
	uint8_t BuzzerCount = 0;
	uint8_t Buzzer      = 0;
	bool Interval       = false;
	
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
				if (key_press & KEY0_MASK)
				{
					key_press ^= KEY0_MASK;
					
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
							State = STATE_PRECOUNT;
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
						case MODE_INTERVAL:
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
						default:
					        break;
					}
				}
				if (key_press & KEY1_MASK) {
					if (++Mode > MODE_COUNT) Mode = MODE_STOPWATCH;
					key_press ^= KEY1_MASK;
				}
				if (key_press & KEY2_MASK) {
					if (--Mode < 1) Mode = MODE_COUNT;
					key_press ^= KEY2_MASK;
				}
			
				ssState.showdigits = (1 << DIGIT0);
				ssState.digits[DIGIT0] = Mode;
				
				if (SecondElapsed > 0) {
					ssState.dots ^= (1 << DIGIT0);
					SecondElapsed--;
				}
				break;
			case STATE_CONFIGURE:
				//switch (Mode)
				//{
//
				    //default:
				        ///* Your code here */
				        //break;
				//}
			case STATE_PRECOUNT:
				if (SecondElapsed > 0) {
					SecondElapsed--;
					
					ssState.digits[DIGIT1] = floor(PreCount / 10);
					ssState.digits[DIGIT0] = PreCount % 10;
					ssState.showdigits = (1 << DIGIT0) | (PreCount > 9 ? (1 << DIGIT1) : 0);
					ssState.dots = (PreCount % 2 == 0 ? (1 << DIGIT0) : 0);
					Buzzer = BUZZER_LONG;
					PreCount--;
					if (PreCount == 0) State = STATE_RUNNING;
				}
			case STATE_RUNNING:
				if (SecondElapsed > 0) {
					SecondElapsed--;
					
					Buzzer = BUZZER_LONG;
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
					ssState.digits[DIGIT3] = floor(clockState.Minutes / 10);
					ssState.digits[DIGIT2] = clockState.Minutes % 10;
					ssState.digits[DIGIT1] = floor(clockState.Seconds / 10);
					ssState.digits[DIGIT0] = clockState.Seconds % 10;
					if (clockState.Seconds % 2 == 0) ssState.dots = (1 << DIGIT2);
					else ssState.dots = 0;
						
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
			PORTC = (1 << PC0);
			Buzzer--;
		}
		else
		{
			PORTC = 0;
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