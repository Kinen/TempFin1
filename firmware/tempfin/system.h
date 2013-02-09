/*
 * system.h - system hardware device configuration values 
 * Part of Kinen project
 *
 * Copyright (c) 2013 Alden S. Hart Jr.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef system_h
#define system_h

/******************************************************************************
 * PARAMETERS AND SETTINGS
 ******************************************************************************/

/*** Clock settings ***/
#undef F_CPU							// set for delays
#define F_CPU 16000000UL				// should always precede <avr/delay.h>

// Clock Crystal Config. Pick one:
//#define __CLOCK_INTERNAL_8MHZ TRUE	// use internal oscillator
//#define __CLOCK_EXTERNAL_8MHZ	TRUE	// uses PLL to provide 32 MHz system clock
#define __CLOCK_EXTERNAL_16MHZ TRUE		// uses PLL to provide 32 MHz system clock

/*** Power reduction register mappings ***/
// you shouldn't need to change this
#define PRADC_bm 			(1<<PRADC)
#define PRUSART0_bm			(1<<PRUSART0)
#define PRSPI_bm 			(1<<PRSPI)
#define PRTIM1_bm			(1<<PRTIM1)
#define PRTIM0_bm			(1<<PRTIM0)
#define PRTIM2_bm			(1<<PRTIM2)
#define PRTWI_bm			(1<<PRTWI)

/**** Lower-level device mappings and constants (for atmega328P) ****/

#define PWM_PORT			PORTD			// Pulse width modulation port
#define PWM_OUTB			(1<<PIND3)		// OC2B timer output bit
#define PWM_TIMER			TCNT2			// Pulse width modulation timer
#define PWM_NONINVERTED		0xC0			// OC2A non-inverted mode, OC2B non-inverted mode
#define PWM_INVERTED 		0xF0			// OC2A inverted mode, OC2B inverted mode
#define PWM_PRESCALE 		64				// corresponds to TCCR2B |= 0b00000100;
#define PWM_PRESCALE_SET 	4				// 2=8x, 3=32x, 4=64x, 5=128x, 6=256x
#define PWM_MIN_RES 		20				// minimum allowable resolution (20 = 5% duty cycle resolution)
#define PWM_MAX_RES 		255				// maximum supported resolution
#define PWM_F_CPU			F_CPU			// 8 Mhz, nominally (internal RC oscillator)
#define PWM_F_MAX			(F_CPU / PWM_PRESCALE / PWM_MIN_RES)
#define PWM_F_MIN			(F_CPU / PWM_PRESCALE / 256)
#define PWM_FREQUENCY 		1000			// set PWM operating frequency

#define PWM2_PORT			PORTD			// secondary PWM channel (on Timer 0)
#define PWM2_OUT2B			(1<<PIND5)		// OC0B timer output bit

#define ADC_PORT			PORTC			// Analog to digital converter channels
#define ADC_CHANNEL 		0				// ADC channel 0 / single-ended in this application (write to ADMUX)
#define ADC_REFS			0b01000000		// AVcc external 5v reference (write to ADMUX)
#define ADC_ENABLE			(1<<ADEN)		// write this to ADCSRA to enable the ADC
#define ADC_START_CONVERSION (1<<ADSC)	// write to ADCSRA to start conversion
#define ADC_PRESCALE 		6				// 6=64x which is ~125KHz at 8Mhz clock
#define ADC_PRECISION 		1024			// change this if you go to 8 bit precision
#define ADC_VREF 			5.00			// change this if the circuit changes. 3v would be about optimal

#define TICK_TIMER			TCNT0			// Tickclock timer
#define TICK_MODE			0x02			// CTC mode 		(TCCR0A value)
#define TICK_PRESCALER		0x03			// 64x prescaler  (TCCR0B value)
#define TICK_COUNT			125				// gets 8 Mhz/64 to 1000 Hz.

#define LED_PORT			PORTD			// LED port
#define LED_PIN				(1<<PIND2)		// LED indicator

/******************************************************************************
 * STRUCTURES 
 ******************************************************************************/

typedef struct DeviceStruct {	// hardware devices that are part of the chip
	uint8_t tick_flag;			// true = the timer interrupt fired
	uint8_t tick_10ms_count;	// 10ms down counter
	uint8_t tick_100ms_count;	// 100ms down counter
	uint8_t tick_1sec_count;	// 1 second down counter
	double pwm_freq;			// save it for stopping and starting PWM
} device_t;
device_t device;				// Device is always a singleton (there is only one device)

/******************************************************************************
 * FUNCTION PROTOTYPES
 ******************************************************************************/

void sys_init(void);					// master hardware init

void adc_init(uint8_t channel);
uint16_t adc_read(void);

void pwm_init(void);
void pwm_on(double freq, double duty);
void pwm_off(void);
uint8_t pwm_set_freq(double freq);
uint8_t pwm_set_duty(double duty);

void tick_init(void);
uint8_t tick_callback(void);
void tick_1ms(void);
void tick_10ms(void);
void tick_100ms(void);
void tick_1sec(void);

void led_init(void);
void led_on(void);
void led_off(void);
void led_toggle(void);

#endif
