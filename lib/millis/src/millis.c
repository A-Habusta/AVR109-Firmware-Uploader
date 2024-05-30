/*
 * Project: Lightweight millisecond tracking library
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2018 by Zak Kemble
 * License: GNU GPL v3 (see License_GPL-3.0.txt) or MIT (see License_MIT.txt)
 * Web: http://blog.zakkemble.net/millisecond-tracking-library-for-avr/
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <util/atomic.h>
#include "millis.h"

#ifndef F_CPU
	#error "F_CPU not defined!"
#endif

#if F_CPU < 256 || F_CPU >= 32640000
	#error "Bad F_CPU setting (<256 or >=32640000)"
#endif

#ifndef MILLIS_TIMER
	#error "Bad MILLIS_TIMER set"
#endif

// Decide what what prescaler and registers to use
#if MILLIS_TIMER == MILLIS_TIMER0

// Timer0

#if F_CPU > 16320000 // 16.32MHz - 65.28MHz
	#define CLOCKSEL (_BV(CS02))
	#define PRESCALER 256
#elif F_CPU > 2040000 // 2.04MHz - 16.32MHz
	#define CLOCKSEL (_BV(CS01)|_BV(CS00))
	#define PRESCALER 64
#elif F_CPU > 255 // 256Hz - 2.04MHz
	#define CLOCKSEL (_BV(CS01))
	#define PRESCALER 8
#endif

#define REG_TCCRA		TCCR0
#define REG_TCCRB		TCCR0
#define REG_TIMSK		TIMSK
#define REG_OCR			OCR0
#define BIT_WGM			WGM01
#define BIT_OCIE		OCIE0
#ifdef TIMER0_COMP_vect
	#define ISR_VECT		TIMER0_COMP_vect
#else
	#define ISR_VECT		TIM0_COMPA_vect
#endif
#define pwr_enable()	power_timer0_enable()
#define pwr_disable()	power_timer0_disable()

#define SET_TCCRA()	(REG_TCCRA |= _BV(BIT_WGM))
#define SET_TCCRB()	(REG_TCCRB |= CLOCKSEL)

#elif MILLIS_TIMER == MILLIS_TIMER1

// Timer1

// 1KHz - 65.28MHz
#define CLOCKSEL (_BV(CS10))
#define PRESCALER 1

#define REG_TCCRA		TCCR1
#define REG_TCCRB		TCCR1
#define REG_TIMSK		TIMSK
#define REG_OCR			OCR1
#define BIT_WGM			WGM12
#define BIT_OCIE		OCIE1
#ifdef TIMER1_COMP_vect
	#define ISR_VECT		TIMER1_COMP_vect
#else
	#define ISR_VECT		TIM1_COMP_vect
#endif
#define pwr_enable()	power_timer1_enable()
#define pwr_disable()	power_timer1_disable()

#define SET_TCCRA()	(REG_TCCRA |= 0)
#define SET_TCCRB()	(REG_TCCRB |= _BV(BIT_WGM)|CLOCKSEL)

#elif MILLIS_TIMER == MILLIS_TIMER2

// Timer2

#if F_CPU > 16320000 // 16.32MHz - 32.64MHz
	#define CLOCKSEL (_BV(CS22)|_BV(CS20))
	#define PRESCALER 128
#elif F_CPU > 8160000 // 8.16MHz - 16.32MHz
	#define CLOCKSEL (_BV(CS21)|_BV(CS20))
	#define PRESCALER 64
#elif F_CPU > 2040000 // 2.04MHz - 8.16MHz
	#define CLOCKSEL (_BV(CS21)|_BV(CS20))
	#define PRESCALER 32
#elif F_CPU > 255 // 256Hz - 2.04MHz
	#define CLOCKSEL (_BV(CS21))
	#define PRESCALER 8
#endif

#define REG_TCCRA		TCCR2
#define REG_TCCRB		TCCR2
#define REG_TIMSK		TIMSK
#define REG_OCR			OCR2
#define BIT_WGM			WGM21
#define BIT_OCIE		OCIE2
#define ISR_VECT		TIMER2_COMP_vect
#define pwr_enable()	power_timer2_enable()
#define pwr_disable()	power_timer2_disable()

#define SET_TCCRA()	(REG_TCCRA |= _BV(BIT_WGM))
#define SET_TCCRB()	(REG_TCCRB |= CLOCKSEL)

#else
	#error "Bad MILLIS_TIMER set"
#endif

static volatile millis_t milliseconds;

// Initialise library
void millis_init()
{
	// Timer settings
	SET_TCCRA();
	SET_TCCRB();
	REG_TIMSK = _BV(BIT_OCIE);
	REG_OCR = ((F_CPU / PRESCALER) / 1000 - 1);
}

// Get current milliseconds
millis_t millis_get()
{
	millis_t ms;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		ms = milliseconds;
	}
	return ms;
}

// Turn on timer and resume time keeping
void millis_resume()
{
	REG_TIMSK |= _BV(BIT_OCIE);
}

// Pause time keeping and turn off timer to save power
void millis_pause()
{
	REG_TIMSK &= ~_BV(BIT_OCIE);
}

// Reset milliseconds count to 0
void millis_reset()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		milliseconds = 0;
	}
}

// Add time
void millis_add(millis_t ms)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		milliseconds += ms;
	}
}

// Subtract time
void millis_subtract(millis_t ms)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		milliseconds -= ms;
	}
}

ISR(ISR_VECT)
{
	++milliseconds;
}
