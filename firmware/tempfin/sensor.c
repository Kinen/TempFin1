/*
 * sensor.c - Kinen temperature controller - sensor functions
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

#include "kinen.h"
#include "system.h"
#include "sensor.h"
#include "util.h"

//#include "tempfin1.h"
//#include "report.h"
//#include "xio/xio.h"

static inline double _sensor_sample(uint8_t adc_channel);

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
	memset(&sensor, 0, sizeof(sensor_t));
	sensor.temperature = ABSOLUTE_ZERO;
	sensor.sample_variance_max = SENSOR_SAMPLE_VARIANCE_MAX;
	sensor.reading_variance_max = SENSOR_READING_VARIANCE_MAX;
	sensor.disconnect_temperature = SENSOR_DISCONNECTED_TEMPERATURE;
	sensor.no_power_temperature = SENSOR_NO_POWER_TEMPERATURE;
	// note: there are no bits to set to outputs in this initialization
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





