/*
 * main.c - Kinen temperature controller example
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
#include <avr/interrupt.h>

//#include <avr/pgmspace.h>
//#include <math.h>
//#include <string.h>				// for memset
//#include <avr/io.h>

#include "system.h"
#include "kinen.h"
#include "tempfin1.h"
#include "report.h"
#include "util.h"
#include "xio/xio.h"

// local functions

static void _controller(void);
static void _unit_tests(void);

// Had to move the struct definitions and declarations to .h file for reporting purposes

/****************************************************************************
 * main
 *
 *	Device and Kinen initialization
 *	Main loop handler
 */
int main(void)
{
	cli();

	// system-level inits
	sys_init();					// do this first
	xio_init();					// do this second
	kinen_init();				// do this third

	// init system devices
//	adc_init(ADC_CHANNEL);
//	pwm_init();
//	tick_init();
	led_init();

	// application level inits
//	heater_init();				// setup the heater module and subordinate functions
//	sensor_init();
	sei(); 						// enable interrupts
	rpt_initialized();			// send initalization string

	_unit_tests();				// run any unit tests that are enabled

	// main loop
	while (true) {
		_controller();
	}
	return (false);				// never returns
}

static void _unit_tests(void)
{
	XIO_UNIT_TESTS				// uncomment __XIO_UNIT_TESTs in xio.h to enable these unit tests
	TF1_UNIT_TESTS				// uncomment __TF1_UNIT_TESTs in tempfin1.h to enable unit tests
//	heater_on(160);				// turn heater on for testing
}

/*
 * Dispatch loop
 *
 *	The dispatch loop is a set of pre-registered callbacks that (in effect) 
 *	provide rudimentry multi-threading. Functions are organized from highest
 *	priority to lowest priority. Each called function must return a status code
 *	(see kinen_core.h). If SC_EAGAIN (02) is returned the loop restarts at the
 *	start of the list. For any other status code exceution continues down the list
 */

#define	DISPATCH(func) if (func == SC_EAGAIN) return; 
static void _controller()
{
	DISPATCH(tick_callback());		// regular interval timer clock handler (ticks)
}


/******************************************************************************
 * TEMPERATURE FIN UNIT TESTS
 ******************************************************************************/

#ifdef __TF1_UNIT_TESTS

/*
#define SETPOINT 200

static void _pid_test(void);
static void _pwm_test(void);

void tf1_unit_tests()
{
	_pid_test();
	_pwm_test();
}

static void _pid_test()
{
	pid_init();
	pid_calculate(SETPOINT, 0);
	pid_calculate(SETPOINT, SETPOINT-150);
	pid_calculate(SETPOINT, SETPOINT-100);
	pid_calculate(SETPOINT, SETPOINT-66);
	pid_calculate(SETPOINT, SETPOINT-50);
	pid_calculate(SETPOINT, SETPOINT-25);
	pid_calculate(SETPOINT, SETPOINT-20);
	pid_calculate(SETPOINT, SETPOINT-15);
	pid_calculate(SETPOINT, SETPOINT-10);
	pid_calculate(SETPOINT, SETPOINT-5);
	pid_calculate(SETPOINT, SETPOINT-3);
	pid_calculate(SETPOINT, SETPOINT-2);
	pid_calculate(SETPOINT, SETPOINT-1);
	pid_calculate(SETPOINT, SETPOINT);
	pid_calculate(SETPOINT, SETPOINT+1);
	pid_calculate(SETPOINT, SETPOINT+5);
	pid_calculate(SETPOINT, SETPOINT+10);
	pid_calculate(SETPOINT, SETPOINT+20);
	pid_calculate(SETPOINT, SETPOINT+25);
	pid_calculate(SETPOINT, SETPOINT+50);
}

static void _pwm_test()
{
	pwm_set_freq(50000);
	pwm_set_freq(10000);
	pwm_set_freq(5000);
	pwm_set_freq(2500);
	pwm_set_freq(1000);
	pwm_set_freq(500);
	pwm_set_freq(250);
	pwm_set_freq(100);

	pwm_set_freq(1000);
	pwm_set_duty(1000);
	pwm_set_duty(100);
	pwm_set_duty(99);
	pwm_set_duty(75);
	pwm_set_duty(50);
	pwm_set_duty(20);
	pwm_set_duty(10);
	pwm_set_duty(5);
	pwm_set_duty(2);
	pwm_set_duty(1);
	pwm_set_duty(0.1);

	pwm_set_freq(5000);
	pwm_set_duty(1000);
	pwm_set_duty(100);
	pwm_set_duty(99);
	pwm_set_duty(75);
	pwm_set_duty(50);
	pwm_set_duty(20);
	pwm_set_duty(10);
	pwm_set_duty(5);
	pwm_set_duty(2);
	pwm_set_duty(1);
	pwm_set_duty(0.1);

// exception cases
}
*/

#endif // __UNIT_TESTS

