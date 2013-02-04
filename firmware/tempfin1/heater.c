/*
 * heater.c - Kinen temperature controller - heater file
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
#include <avr/pgmspace.h>
#include <math.h>
//#include <avr/io.h>
//#include <avr/interrupt.h>

#include "kinen.h"
#include "system.h"
#include "heater.h"
#include "sensor.h"
#include "report.h"

//#include "tempfin1.h"
//#include "util.h"
//#include "xio/xio.h"

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
	memset(&heater, 0, sizeof(heater_t));
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

