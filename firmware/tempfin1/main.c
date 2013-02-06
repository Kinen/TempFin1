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
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <avr/interrupt.h>

#include "kinen.h"
#include "tempfin1.h"
#include "system.h"
#include "heater.h"
#include "sensor.h"
#include "report.h"
#include "config.h"
#include "json_parser.h"
#include "util.h"
#include "xio/xio.h"

// local functions

static void _controller(void);
static uint8_t _dispatch(void);
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
	cfg_init();

	adc_init(ADC_CHANNEL);		// init system devices
	pwm_init();
	tick_init();
	led_init();

	// application level inits
	heater_init();				// setup the heater module and subordinate functions
	sensor_init();
	sei(); 						// enable interrupts
	rpt_initialized();			// send initalization string

//	_unit_tests();				// run any unit tests that are enabled
	canned_startup();

	while (true) {				// main loop
		_controller();
	}
	return (false);				// never returns
}

/*
 *	_controller()
 *	_dispatch()
 *
 *	The controller/dispatch loop is a set of pre-registered callbacks that (in effect)
 *	provide rudimentry multi-threading. Functions are organized from highest priority 
 *	to lowest priority. Each called function must return a status code (see kinen.h). 
 *	If SC_EAGAIN (02) is returned the loop restarts at the beginning of the list. 
 *	For any other status code exceution continues down the list.
 */
#define	RUN(func) if (func == SC_EAGAIN) return; 
static void _controller()
{
	RUN(tick_callback());		// regular interval timer clock handler (ticks)
	RUN(_dispatch());			// read and execute next incoming command
}

static uint8_t _dispatch()
{
	ritorno (xio_gets(kc.src, kc.in_buf, sizeof(kc.in_buf)));// read line or return if not completed
	js_json_parser(kc.in_buf);
	return (SC_OK);

//	if ((status = xio_gets(kc.src, kc.in_buf, sizeof(kc.in_buf))) != SC_OK) {
//		if (status == SC_EOF) {					// EOF can come from file devices only
//			fprintf_P(stderr, PSTR("End of command file\n"));
//			tg_reset_source();					// reset to default source
//		}
//		// Note that TG_EAGAIN, TG_NOOP etc. will just flow through
//		return (status);
//	}
}

/******************************************************************************
 * STARTUP TESTS
 ******************************************************************************/

/*
 * canned_startup() - run a string on startup
 *
 *	Pre-load the USB RX (input) buffer with some test strings that will be called 
 *	on startup. Be mindful of the char limit on the read buffer (RX_BUFFER_SIZE).
 *	It's best to create a test file for really complicated things.
 */
void canned_startup()	// uncomment __CANNED_STARTUP in tempfin1.h if you need this run
{
#ifdef __CANNED_STARTUP
	xio_queue_RX_string(XIO_DEV_USART, "{\"fv\":\"\"}\n");
#endif
}

/******************************************************************************
 * TEMPERATURE FIN UNIT TESTS
 ******************************************************************************/

static void _unit_tests(void)
{
	XIO_UNIT_TESTS				// uncomment __XIO_UNIT_TESTs in xio.h to enable these unit tests
	TF1_UNIT_TESTS				// uncomment __TF1_UNIT_TESTs in tempfin1.h to enable unit tests
//	heater_on(160);				// turn heater on for testing
}


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

