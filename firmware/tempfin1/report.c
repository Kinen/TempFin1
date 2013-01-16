/*
 * report.c - contains all reporting statements
 * Part of Kinen project
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
//#include <stdlib.h>
//#include <stdbool.h>
//#include <string.h>
#include <avr/pgmspace.h>

#include "report.h"
#include "tempfin1.h"
#include "xio/xio.h"

/*** Strings and string arrays in program memory ***/
/*++++++++++++++++++
static const char initialized[] PROGMEM = "\nDevice Initialized\n"; 

static const char msg_scode0[] PROGMEM = "";
static const char msg_scode1[] PROGMEM = "  Taking Reading";
static const char msg_scode2[] PROGMEM = "  Bad Reading";
static const char msg_scode3[] PROGMEM = "  Disconnected";
static const char msg_scode4[] PROGMEM = "  No Power";
static PGM_P const msg_scode[] PROGMEM = { msg_scode0, msg_scode1, msg_scode2, msg_scode3, msg_scode4 };

static const char msg_hstate0[] PROGMEM = "  OK";
static const char msg_hstate1[] PROGMEM = "  Shutdown";
static const char msg_hstate2[] PROGMEM = "  Heating";
static const char msg_hstate3[] PROGMEM = "  REGULATED";
static PGM_P const msg_hstate[] PROGMEM = { msg_hstate0, msg_hstate1, msg_hstate2, msg_hstate3 };
+++++++++++++++++*/
/*** Display routines ***/

void rpt_initialized()
{
	printf_P(PSTR("\nDevice Initialized\n"));
}
/*+++++++++++++++++++
void rpt_readout()
{
	printf_P(PSTR("Temp:%1.3f  "), 		sensor.temperature);
	printf_P(PSTR("PWM:%1.3f  "),		pid.output);
//	printf_P(PSTR("s[0]:%1.3f  "), 		sensor.sample[0]);
	printf_P(PSTR("StdDev:%1.3f  "),	sensor.std_dev);
//	printf_P(PSTR("Samples:%1.3f  "),	sensor.samples);
	printf_P(PSTR("Err:%1.3f  "),		pid.error);
	printf_P(PSTR("I:%1.3f  "),			pid.integral);
//	printf_P(PSTR("D:%1.3f  "),			pid.derivative);
//	printf_P(PSTR("Hy:%1.3f  "),		heater.hysteresis);

	printf_P((PGM_P)pgm_read_word(&msg_hstate[heater.state]));
//	printf_P((PGM_P)pgm_read_word(&msg_scode[sensor.code]));
	printf_P(PSTR("\n")); 
}

+++++++++++++++*/
