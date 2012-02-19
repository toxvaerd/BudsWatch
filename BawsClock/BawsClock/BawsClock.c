/*
 * BawsClock.c
 *
 * Created: 17-02-2012 19:17:59
 *  Author: Morten
 */ 
#define F_CPU 8000000UL // 8Mhz Crystal Oscillator

#include <avr/io.h>
#include <avr/interrupt.h>

unsigned char Seconds = 0;
unsigned char Pause = 0;
unsigned char Round = 1;

int main (void) 
{ 
	
	
	DDRA = 0xFF; // Enable all port A LEDs
	DDRB = 0xFF; // Enable all port B LEDs
	DDRC = 0x01;

	TCCR1B |= (1 << WGM12); // Enable CTC in timer 1's control register
	
	TIMSK |= (1 << OCIE1A); // Output Compare Interrup Enable, timer 1 channel A
	
	sei();
	
	OCR1AH = 0x7A; // Prescaling to 31249
	OCR1AL = 0x11; // -||-
	
	TCCR1B |= (1 << CS12); // Set up timer at Fcpu/256

	for (;;) 
	{ 
		PORTA = Seconds;
		PORTB = Round;
		PORTC = Pause;
	} 
}

ISR(TIMER1_COMPA_vect) {
	unsigned int Target = 20;
	
	if (Pause == 0) Target = 20;
	else Target = 10;
	if (++Seconds >= Target-1) {
		Seconds = 0;
		if (Pause) Round++;
		Pause = ~Pause;
	}
		
}