/*
 * BawsClock.c
 *
 * Created: 17-02-2012 19:17:59
 *  Author: Morten
 */ 
#define F_CPU 8000000UL // 8Mhz Crystal Oscillator

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <stdbool.h>

static bool SecondElapsed = false,
			Button0Pressed = false,
			Button1Pressed = false;

static unsigned char debounceB0,debounceB1,debouncedBState;
// vertical stacked counter based debounce
unsigned debounceB(unsigned char sample)
{
    unsigned char delta, changes;
    
    // Set delta to changes from last sample
    delta = sample ^ debouncedBState;
    
    // Increment counters
    debounceB1 = debounceB1 ^ debounceB0;
    debounceB0  = ~debounceB0;
    
    // reset any unchanged bits
    debounceB0 &= delta;
    debounceB1 &= delta;
    
    // update state & calculate returned change set
    changes = ~(~delta | debounceB0 | debounceB1);
    debouncedBState ^= changes;
    
    return changes;
}
unsigned digitToSevenSegment(unsigned char digit) {
	switch (digit)
	{
	    case 0: return 0x7E;
		case 1: return 0x30;
		case 2: return 0x6D;
		case 3: return 0x79;
		case 4: return 0x33;
		case 5: return 0x5B;
		case 6: return 0x5F;
		case 7: return 0x70;
		case 8: return 0x7F;
		case 9: return 0x7B;
	    default:
			return 0x00;
		    break;
	}
}

int main (void) 
{ 
	unsigned char Seconds = 0;
	unsigned char Minutes = 0;
	bool CountDown = false;
	
	/* SET UP I/O */
	DDRA = 0xFF; // Enable all port A LEDs
	DDRB = (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3); // Enable 4 7 segment displays
	DDRC &= (1 << PC0) | (1 << PC1); // Set PC0 and PC1 as input
	PORTC |= (1 << PC0) | (1 << PC1); // Turn on pullup resistors

	/* SET UP TIMERS */
	// Timer 0: Overflow counter for button debouncing
	TCCR0 |= (1 << CS00) | (1 << CS01);
	TIMSK |= (1 << TOIE0);

	// Timer 1: Count seconds
	TCCR1B |= (1 << WGM12); // Enable CTC in timer 1's control register
	TIMSK |= (1 << OCIE1A); // Output Compare Interrup Enable, timer 1 channel A
	OCR1AH = 0x7A; // Compare to 31249
	OCR1AL = 0x11; // -||-
	TCCR1B |= (1 << CS12); // Set up timer prescaling at Fcpu/256

	sei();

	for (;;) 
	{ 
		if (Button0Pressed) CountDown = true;
		if (Button1Pressed) CountDown = false;
		
		if (SecondElapsed) {
			if (CountDown) Seconds--;
			else Seconds++;
			SecondElapsed = false;
			
			if (Seconds >= 60) {
				Seconds = 0;
				Minutes++;
			}
		}
		
		unsigned char digit1 = Minutes % 10;
		unsigned char digit0 = (Minutes - digit1) / 10;
		unsigned char digit2 = Seconds % 10;
		unsigned char digit3 = (Seconds - digit3) / 10;
		PORTA = digitToSevenSegment(digit0);
		PORTB = (1 << PB0);
		PORTA = digitToSevenSegment(digit1) & (Seconds%2==0? 1 << PA7 : 0); // Add . to minutes every second
		PORTB = (1 << PB1);
		PORTA = digitToSevenSegment(digit2);
		PORTB = (1 << PB2);
		PORTA = digitToSevenSegment(digit3);
		PORTB = (1 << PB3);
	} 
}

ISR(TIMER1_COMPA_vect) {
	SecondElapsed = true;
}

ISR(TIMER0_OVF_vect) {
	unsigned char sample = PINC;
    unsigned char changes = debounceB(sample);
	
	Button0Pressed = changes & (1 << PC0) && !(sample & (1 << PC0));
	Button1Pressed = changes & (1 << PC1) && !(sample & (1 << PC1));
}