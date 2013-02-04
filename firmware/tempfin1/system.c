/*
 * system.c - general hardware device and support functions
 * Part of Kinen project
 *
 * Copyright (c) 2012 - 2013 Alden S. Hart Jr.
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
#include <stddef.h>
#include <stdbool.h>
#include <avr/pgmspace.h> 
#include <avr/interrupt.h>
//#include <avr/io.h>
//#include <math.h>

#include "kinen.h"
#include "system.h"
#include "sensor.h"
#include "heater.h"

/**** sys_init() - lowest level hardware init ****/

void sys_init() 
{
	PRR = 0xFF;					// turn off all peripherals. Each device needs to enble itself

	DDRB = 0x00;				// initialize all ports as inputs. Each device sets its own outputs
	DDRC = 0x00;
	DDRD = 0x00;
}

// Atmega328P data direction defines: 0=input pin, 1=output pin
// These defines therefore only specify output pins
/*
#define PORTB_DIR	(SPI_MISO)			// setup for on-board SPI to work
#define PORTC_DIR	(0)					// no output bits on C
#define PORTD_DIR	(LED_PIN | PWM_OUTB)// set LED and PWM bits as outputs
*/


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
	DDRD |= PWM_OUTB;					// set PWM bit to output
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
 * led_init()
 * led_on()
 * led_off()
 * led_toggle()
 */

void led_init()
{
	DDRD |= PWM_OUTB;			// set PWM bit to output
	led_off();					// put off the red light [~Sting, 1978]
}

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


