/*
 * tempfin1.c - Kinen temperature controller example
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
#include "print.h"
#include "report.h"
#include "util.h"
#include "xio/xio.h"

// local functions

static void _controller(void);
static double _sensor_sample(uint8_t adc_channel);

// Had to move the struct definitions and declarations to .h file for reporting purposes

/****************************************************************************
 * main
 *
 *	Device and Kinen initialization
 *	Main loop handler
 */
int main(void)
{
	cli();						// initializations
	sys_init();					// do this first
	xio_init();					// do this second
	kinen_init();				// do this third
	device_init();				// handles all the low-level device peripheral inits
	heater_init();				// setup the heater module and subordinate functions
	sei(); 						// enable interrupts

	UNIT_TESTS;					// uncomment __UNIT_TEST_TC to enable unit tests

	heater_on(160);				// turn heater on for testing
	rpt_initialized();			// send initalization string

	while (true) {				// go to the controller loop and never return
		_controller();
	}
	return (false);				// never returns
}

/*
 * Device Init 
 */
void device_init(void)
{
	DDRB = PORTB_DIR;			// initialize all ports for proper IO function
	DDRC = PORTC_DIR;
	DDRD = PORTD_DIR;

	adc_init(ADC_CHANNEL);
	pwm_init();
	tick_init();
	led_off();					// put off the red light [~Sting, 1978]
}

/**** PWM Port Functions ****
 * _pwm_bit_hi()
 * _pwm_bit_lo()
 */
//void _pwm_bit_hi(void) { PWM_PORT |= PWM_OUTB;}
//void _pwm_bit_lo(void) { PWM_PORT &= ~(PWM_OUTB);}

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
//	DISPATCH(kinen_callback());		// intercept low-level communication events
	DISPATCH(tick_callback());		// regular interval timer clock handler (ticks)
}

/**** Heater Functions ****/
/*
 * heater_init() - initialize heater with default values
 * heater_on()	 - turn heater on
 * heater_off()	 - turn heater off	
 * heater_callback() - 100ms timed loop for heater control
 *
 *	heater_init() sets default values that may be overwritten via Kinen communications. 
 *	heater_on() sets initial values used regardless of any changes made to settings.
 */

void heater_init()
{ 
	// initialize heater, start PID and PWM
	// note: PWM and ADC are initialized as part of the device init
	memset(&heater, 0, sizeof(Heater));
	heater.regulation_range = HEATER_REGULATION_RANGE;
	heater.ambient_timeout = HEATER_AMBIENT_TIMEOUT;
	heater.regulation_timeout = HEATER_REGULATION_TIMEOUT;
	heater.ambient_temperature = HEATER_AMBIENT_TEMPERATURE;
	heater.overheat_temperature = HEATER_OVERHEAT_TEMPERATURE;
	heater.bad_reading_max = HEATER_BAD_READING_MAX;
	sensor_init();
	pid_init();
}

void heater_on(double setpoint)
{
	// no action if heater is already on
	if ((heater.state == HEATER_HEATING) || (heater.state == HEATER_REGULATED)) {
		return;
	}
	// turn on lower level functions
	sensor_on();						// enable the sensor
	sensor_start_reading();				// now start a reading
	pid_reset();
	pwm_on(PWM_FREQUENCY, 0);			// duty cycle will be set by PID loop

	// initialize values for a heater cycle
	heater.setpoint = setpoint;
	heater.hysteresis = 0;
	heater.bad_reading_count = 0;
	heater.regulation_timer = 0;		// reset timeouts
	heater.state = HEATER_HEATING;
	led_off();
}

void heater_off(uint8_t state, uint8_t code) 
{
	pwm_off();							// stop sending current to the heater
	sensor_off();						// stop taking readings
	heater.state = state;
	heater.code = code;
	led_off();
}

void heater_callback()
{
	// catch the no-op cases
	if ((heater.state == HEATER_OFF) || (heater.state == HEATER_SHUTDOWN)) { return;}
	rpt_readout();

	// get current temperature from the sensor
	heater.temperature = sensor_get_temperature();

	// trap overheat condition
	if (heater.temperature > heater.overheat_temperature) {
		heater_off(HEATER_SHUTDOWN, HEATER_OVERHEATED);
		return;
	}

	sensor_start_reading();				// start reading for the next interval

	// handle bad readings from the sensor
	if (heater.temperature < ABSOLUTE_ZERO) {
		if (++heater.bad_reading_count > heater.bad_reading_max) {
			heater_off(HEATER_SHUTDOWN, HEATER_SENSOR_ERROR);
			printf_P(PSTR("Heater Sensor Error Shutdown\n"));	
		}
		return;
	}
	heater.bad_reading_count = 0;		// reset the bad reading counter

	double duty_cycle = pid_calculate(heater.setpoint, heater.temperature);
	pwm_set_duty(duty_cycle);

	// handle HEATER exceptions
	if (heater.state == HEATER_HEATING) {
		heater.regulation_timer += HEATER_TICK_SECONDS;

		if ((heater.temperature < heater.ambient_temperature) &&
			(heater.regulation_timer > heater.ambient_timeout)) {
			heater_off(HEATER_SHUTDOWN, HEATER_AMBIENT_TIMED_OUT);
			printf_P(PSTR("Heater Ambient Error Shutdown\n"));	
			return;
		}
		if ((heater.temperature < heater.setpoint) &&
			(heater.regulation_timer > heater.regulation_timeout)) {
			heater_off(HEATER_SHUTDOWN, HEATER_REGULATION_TIMED_OUT);
			printf_P(PSTR("Heater Timeout Error Shutdown\n"));	
			return;
		}
	}

	// Manage regulation state and LED indicator
	// Heater.regulation_count is a hysteresis register that increments if the 
	// heater is at temp, decrements if not. It pegs at max and min values.
	// The LED flashes if the heater is not in regulation and goes solid if it is.

	if (fabs(pid.error) <= heater.regulation_range) {
		if (++heater.hysteresis > HEATER_HYSTERESIS) {
			heater.hysteresis = HEATER_HYSTERESIS;
			heater.state = HEATER_REGULATED;
		}
	} else {
		if (--heater.hysteresis <= 0) {
			heater.hysteresis = 0;
			heater.regulation_timer = 0;			// reset timeouts
			heater.state = HEATER_HEATING;
		}
	}
	if (heater.state == HEATER_REGULATED) {
		led_on();
	} else {
		if (++heater.toggle > 3) {
			heater.toggle = 0;
			led_toggle();
		}
	}
}

/**** Heater PID Functions ****/
/*
 * pid_init() - initialize PID with default values
 * pid_reset() - reset PID values to cold start
 * pid_calc() - derived from: http://www.embeddedheaven.com/pid-control-algorithm-c-language.htm
 */

void pid_init() 
{
	memset(&pid, 0, sizeof(struct PIDstruct));
	pid.Kp = PID_Kp;
	pid.Ki = PID_Ki;
	pid.Kd = PID_Kd;
	pid.output_max = PID_MAX_OUTPUT;		// saturation filter max value
	pid.output_min = PID_MIN_OUTPUT;		// saturation filter min value
	pid.state = PID_ON;
}

void pid_reset()
{
	pid.output = 0;
	pid.integral = PID_INITIAL_INTEGRAL;
	pid.prev_error = 0;
}

double pid_calculate(double setpoint,double temperature)
{
	if (pid.state == PID_OFF) { return (pid.output_min);}

	pid.error = setpoint - temperature;		// current error term

	// perform integration only if error is GT epsilon, and with anti-windup
	if ((fabs(pid.error) > PID_EPSILON) && (pid.output < pid.output_max)) {	
		pid.integral += (pid.error * PID_DT);
	}
	// compute derivative and output
	pid.derivative = (pid.error - pid.prev_error) / PID_DT;
	pid.output = pid.Kp * pid.error + pid.Ki * pid.integral + pid.Kd * pid.derivative;

	// fix min amd max outputs (saturation filter)
	if(pid.output > pid.output_max) { pid.output = pid.output_max; } else
	if(pid.output < pid.output_min) { pid.output = pid.output_min; }
	pid.prev_error = pid.error;

	return pid.output;
}

/**** Temperature Sensor and Functions ****/
/*
 * sensor_init()	 		- initialize temperature sensor
 * sensor_on()	 			- turn temperature sensor on
 * sensor_off()	 			- turn temperature sensor off
 * sensor_start_reading()	- start a temperature reading
 * sensor_get_temperature()	- return latest temperature reading or LESS _THAN_ZERO
 * sensor_get_state()		- return current sensor state
 * sensor_get_code()		- return latest sensor code
 * sensor_callback() 		- perform sensor sampling / reading
 */

void sensor_init()
{
	memset(&sensor, 0, sizeof(Sensor));
	sensor.temperature = ABSOLUTE_ZERO;
	sensor.sample_variance_max = SENSOR_SAMPLE_VARIANCE_MAX;
	sensor.reading_variance_max = SENSOR_READING_VARIANCE_MAX;
	sensor.disconnect_temperature = SENSOR_DISCONNECTED_TEMPERATURE;
	sensor.no_power_temperature = SENSOR_NO_POWER_TEMPERATURE;
}

void sensor_on()
{
	sensor.state = SENSOR_NO_DATA;
}

void sensor_off()
{
	sensor.state = SENSOR_OFF;
}

void sensor_start_reading() 
{ 
	sensor.sample_idx = 0;
	sensor.code = SENSOR_TAKING_READING;
}

uint8_t sensor_get_state() { return (sensor.state);}
uint8_t sensor_get_code() { return (sensor.code);}

double sensor_get_temperature() 
{ 
	if (sensor.state == SENSOR_HAS_DATA) { 
		return (sensor.temperature);
	} else {
		return (LESS_THAN_ZERO);	// an impossible temperature value
	}
}

/*
 * sensor_callback() - perform tick-timer sensor functions
 *
 *	Sensor_callback() reads in an array of sensor readings then processes the 
 *	array for a clean reading. The function uses the standard deviation of the 
 *	sample set to clean up the reading or to reject the reading as being flawed.
 *
 *	It's set up to collect 9 samples at 10 ms intervals to serve a 100ms heater 
 *	loop. Each sampling interval must be requested explicitly by calling 
 *	sensor_start_sample(). It does not free-run.
 */

void sensor_callback()
{
	// cases where you don't execute the callback:
	if ((sensor.state == SENSOR_OFF) || (sensor.code != SENSOR_TAKING_READING)) {
		return;
	}

	// get a sample and return if still in the reading period
	sensor.sample[sensor.sample_idx] = _sensor_sample(ADC_CHANNEL);
	if ((++sensor.sample_idx) < SENSOR_SAMPLES) { return; }

	// process the array to clean up samples
	double mean;
	sensor.std_dev = std_dev(sensor.sample, SENSOR_SAMPLES, &mean);
	if (sensor.std_dev > sensor.reading_variance_max) {
		sensor.state = SENSOR_ERROR;
		sensor.code = SENSOR_ERROR_BAD_READINGS;
		return;
	}

	// reject the outlier samples and re-compute the average
	sensor.samples = 0;
	sensor.temperature = 0;
	for (uint8_t i=0; i<SENSOR_SAMPLES; i++) {
		if (fabs(sensor.sample[i] - mean) < (sensor.sample_variance_max * sensor.std_dev)) {
			sensor.temperature += sensor.sample[i];
			sensor.samples++;
		}
	}
	sensor.temperature /= sensor.samples;// calculate mean temp w/o the outliers
	sensor.state = SENSOR_HAS_DATA;
	sensor.code = SENSOR_IDLE;			// we are done. Flip it back to idle

	// process the exception cases
	if (sensor.temperature > SENSOR_DISCONNECTED_TEMPERATURE) {
		sensor.state = SENSOR_ERROR;
		sensor.code = SENSOR_ERROR_DISCONNECTED;
	} else if (sensor.temperature < SENSOR_NO_POWER_TEMPERATURE) {
		sensor.state = SENSOR_ERROR;
		sensor.code = SENSOR_ERROR_NO_POWER;
	}
}

/*
 * _sensor_sample() - take a sample and reject samples showing excessive variance
 *
 *	Returns temperature sample if within variance bounds
 *	Returns ABSOLUTE_ZERO if it cannot get a sample within variance
 *	Retries sampling if variance is exceeded - reject spurious readings
 *	To start a new sampling period set 'new_period' true
 *
 * Temperature calculation math
 *
 *	This setup is using B&K TP-29 K-type test probe (Mouser part #615-TP29, $9.50 ea) 
 *	coupled to an Analog Devices AD597 (available from Digikey)
 *
 *	This combination is very linear between 100 - 300 deg-C outputting 7.4 mV per degree
 *	The ADC uses a 5v reference (the 1st major source of error), and 10 bit conversion
 *
 *	The sample value returned by the ADC is computed by ADCvalue = (1024 / Vref)
 *	The temperature derived from this is:
 *
 *		y = mx + b
 *		temp = adc_value * slope + offset
 *
 *		slope = (adc2 - adc1) / (temp2 - temp1)
 *		slope = 0.686645508							// from measurements
 *
 *		b = temp - (adc_value * slope)
 *		b = -4.062500								// from measurements
 *
 *		temp = (adc_value * 1.456355556) - -120.7135972
 */

static inline double _sensor_sample(uint8_t adc_channel)
{
#ifdef __TEST
	double random_gain = 5;
	double random_variation = ((double)(rand() - RAND_MAX/2) / RAND_MAX/2) * random_gain;
	double reading = 60 + random_variation;
	return (((double)reading * SENSOR_SLOPE) + SENSOR_OFFSET);	// useful for testing the math
#else
	return (((double)adc_read() * SENSOR_SLOPE) + SENSOR_OFFSET);
#endif
}

/**** ADC - Analog to Digital Converter for thermocouple reader ****/
/*
 * adc_init() - initialize ADC. See tinyg_tc.h for settings used
 * adc_read() - returns a single ADC reading (raw). See __sensor_sample notes for more
 *
 *	There's a weird bug where somethimes the first conversion returns zero. 
 *	I need to fund out why this is happening and stop it.
 *	In the mean time there is a do-while loop in the read function.
 */
void adc_init(uint8_t channel)
{
	PRR &= ~PRADC_bm;					// Enable the ADC in the power reduction register (system.h)
	ADMUX  = (ADC_REFS | channel);		// setup ADC Vref and channel
	ADCSRA = (ADC_ENABLE | ADC_PRESCALE);// Enable ADC (bit 7) & set prescaler

	ADMUX &= 0xF0;						// clobber the channel
	ADMUX |= 0x0F & ADC_CHANNEL;		// set the channel
	DIDR0 = (1<<channel);				// disable digital input
}

uint16_t adc_read()
{
	do {
		ADCSRA |= ADC_START_CONVERSION; // start the conversion
		while (ADCSRA && (1<<ADIF) == 0);// wait about 100 uSec
		ADCSRA |= (1<<ADIF);			// clear the conversion flag
	} while (ADC == 0);
	return (ADC);
}

/**** PWM - Pulse Width Modulation Functions ****/
/*
 * pwm_init() - initialize RTC timers and data
 *
 * 	Configure timer 2 for extruder heater PWM
 *	Mode: 8 bit Fast PWM Fast w/OCR2A setting PWM freq (TOP value)
 *		  and OCR2B setting the duty cycle as a fraction of OCR2A seeting
 */
void pwm_init(void)
{
	PRR &= ~PRTIM2_bm;					// Enable Timer2 in the power reduction register (system.h)
	TCCR2A  = PWM_INVERTED;				// alternative is PWM_NONINVERTED
	TCCR2A |= 0b00000011;				// Waveform generation set to MODE 7 - here...
	TCCR2B  = 0b00001000;				// ...continued here
	TCCR2B |= PWM_PRESCALE_SET;			// set clock and prescaler
	TIMSK1 = 0; 						// disable PWM interrupts
	OCR2A = 0;							// clear PWM frequency (TOP value)
	OCR2B = 0;							// clear PWM duty cycle as % of TOP value
	device.pwm_freq = 0;
}

void pwm_on(double freq, double duty)
{
	pwm_init();
	pwm_set_freq(freq);
	pwm_set_duty(duty);
}

void pwm_off(void)
{
	pwm_on(0,0);
}

/*
 * pwm_set_freq() - set PWM channel frequency
 *
 *	At current settings the range is from about 500 Hz to about 6000 Hz  
 */

uint8_t pwm_set_freq(double freq)
{
	device.pwm_freq = F_CPU / PWM_PRESCALE / freq;
	if (device.pwm_freq < PWM_MIN_RES) { 
		OCR2A = PWM_MIN_RES;
	} else if (device.pwm_freq >= PWM_MAX_RES) { 
		OCR2A = PWM_MAX_RES;
	} else { 
		OCR2A = (uint8_t)device.pwm_freq;
	}
	return (SC_OK);
}

/*
 * pwm_set_duty() - set PWM channel duty cycle 
 *
 *	Setting duty cycle between 0 and 100 enables PWM channel
 *	Setting duty cycle to 0 disables the PWM channel with output low
 *	Setting duty cycle to 100 disables the PWM channel with output high
 *
 *	The frequency must have been set previously.
 *
 *	Since I can't seem to get the output pin to work in non-inverted mode
 *	it's done in software in this routine.
 */

uint8_t pwm_set_duty(double duty)
{
	if (duty < 0.01) {				// anything approaching 0% 
		OCR2B = 255;
	} else if (duty > 99.9) { 		// anything approaching 100%
		OCR2B = 0;
	} else {
		OCR2B = (uint8_t)(OCR2A * (1-(duty/100)));
	}
	OCR2A = (uint8_t)device.pwm_freq;
	return (SC_OK);
}

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

/**** LED Functions ****
 * led_on()
 * led_off()
 * led_toggle()
 */

void led_on(void) 
{
	LED_PORT &= ~(LED_PIN);
}

void led_off(void) 
{
	LED_PORT |= LED_PIN;
}

void led_toggle(void) 
{
	if (LED_PORT && LED_PIN) {
		led_on();
	} else {
		led_off();
	}
}


//###########################################################################
//##### UNIT TESTS ##########################################################
//###########################################################################

#ifdef __UNIT_TEST_TC

#define SETPOINT 200

void device_unit_tests()
{

// PID tests


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

// PWM tests
/*
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
*/
// exception cases

}

#endif // __UNIT_TEST_TC

