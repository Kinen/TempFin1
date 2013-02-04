/*
 * main.c - Kinen temperature controller - clock functions
 *
 * Part of Kinen project
 * Based on Kinen Motion Control System 
 *
 * Copyright (c) 2012 - 2013 Alden S. Hart Jr.
 *
 * The Kinen Motion Control System is licensed under the LGPL license
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>				// for memset
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <math.h>

#include "system.h"
#include "kinen_core.h"
#include "tempfin1.h"
#include "report.h"
#include "util.h"
#include "xio/xio.h"

/**** Tick - Tick tock - Regular Interval Timer Clock Functions ****
 * tick_init() 	  - initialize RIT timers and data
 * RIT ISR()	  - RIT interrupt routine 
 * tick_callback() - run RIT from dispatch loop
 * tick_10ms()	  - tasks that run every 10 ms
 * tick_100ms()	  - tasks that run every 100 ms
 * tick_1sec()	  - tasks that run every 100 ms
 */

void tick_init(void)
{
	PRR &= ~PRTIM0_bm;				// Enable Timer0 in the power reduction register (system.h)
	TCCR0A = TICK_MODE;				// mode_settings
	TCCR0B = TICK_PRESCALER;		// 1024 ~= 7800 Hz
	OCR0A = TICK_COUNT;
	TIMSK0 = (1<<OCIE0A);			// enable compare interrupts
	device.tick_10ms_count = 10;
	device.tick_100ms_count = 10;
	device.tick_1sec_count = 10;	
}

ISR(TIMER0_COMPA_vect)
{
	device.tick_flag = true;
}

uint8_t tick_callback(void)
{
	if (device.tick_flag == false) { return (SC_NOOP);}

	device.tick_flag = false;
	tick_1ms();

	if (--device.tick_10ms_count != 0) { return (SC_OK);}
	device.tick_10ms_count = 10;
	tick_10ms();

	if (--device.tick_100ms_count != 0) { return (SC_OK);}
	device.tick_100ms_count = 10;
	tick_100ms();

	if (--device.tick_1sec_count != 0) { return (SC_OK);}
	device.tick_1sec_count = 10;
	tick_1sec();

	return (SC_OK);
}

void tick_1ms(void)				// 1ms callout
{
	sensor_callback();
}

void tick_10ms(void)			// 10 ms callout
{
}

void tick_100ms(void)			// 100ms callout
{
	heater_callback();
}

void tick_1sec(void)			// 1 second callout
{
//	led_toggle();
}



