/*
 * tempfin1.h - Kinen temperature controller example 
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
/* Special thanks to Adam Mayer and the Replicator project for heater guidance
 */
#ifndef tempfin1_h
#define tempfin1_h

#define BUILD_NUMBER 		003.01			// SPI development
#define VERSION_NUMBER		0.1				// firmware major version
#define HARDWARE_VERSION	0.1				// board revision number

/****** DEVELOPMENT SETTINGS ******/

//#define __CANNED_STARTUP					// run any canned startup moves
//#define __DISABLE_PERSISTENCE				// disable EEPROM writes for faster simulation
//#define __SUPPRESS_STARTUP_MESSAGES 		// what it says
//#define __TF1_UNIT_TESTS					// uncomment to compile and run TempFin1 unit tests
											// uncomment __XIO_UNIT_TESTS in xio.h if you need those

/******************************************************************************
 * PARAMETERS AND SETTINGS
 ******************************************************************************/

/**** Heater default parameters ***/

#define HEATER_TICK_SECONDS 		0.1		// 100 ms
#define HEATER_HYSTERESIS 			10		// number of successive readings before declaring heater at-temp or out of regulation
#define HEATER_AMBIENT_TEMPERATURE	40		// detect heater not heating if readings stay below this temp
#define HEATER_OVERHEAT_TEMPERATURE 300		// heater is above max temperature if over this temp. Should shut down
#define HEATER_AMBIENT_TIMEOUT 		90		// time to allow heater to heat above ambinet temperature (seconds)
#define HEATER_REGULATION_RANGE 	3		// +/- degrees to consider heater in regulation
#define HEATER_REGULATION_TIMEOUT 	300		// time to allow heater to come to temp (seconds)
#define HEATER_BAD_READING_MAX 		5		// maximum successive bad readings before shutting down

enum tcHeaterState {						// heater state machine
	HEATER_OFF = 0,							// heater turned OFF or never turned on - transitions to HEATING
	HEATER_SHUTDOWN,						// heater has been shut down - transitions to HEATING
	HEATER_HEATING,							// heating up from OFF or SHUTDOWN - transitions to REGULATED or SHUTDOWN
	HEATER_REGULATED						// heater is at setpoint and in regulation - transitions to OFF or SHUTDOWN
};

enum tcHeaterCode {
	HEATER_OK = 0,							// heater is OK - no errors reported
	HEATER_AMBIENT_TIMED_OUT,				// heater failed to get past ambient temperature
	HEATER_REGULATION_TIMED_OUT,			// heater heated but failed to achieve regulation before timeout
	HEATER_OVERHEATED,						// heater exceeded maximum temperature cutoff value
	HEATER_SENSOR_ERROR						// heater encountered a fatal sensor error
};

/**** PID default parameters ***/

#define PID_DT 				HEATER_TICK_SECONDS	// time constant for PID computation
#define PID_EPSILON 		0.1				// error term precision
#define PID_MAX_OUTPUT 		100				// saturation filter max PWM percent
#define PID_MIN_OUTPUT 		0				// saturation filter min PWM percent

#define PID_Kp 				5.00			// proportional gain term
#define PID_Ki 				0.1 			// integral gain term
#define PID_Kd 				0.5				// derivative gain term
#define PID_INITIAL_INTEGRAL 200			// initial integral value to speed things along

// some starting values from the example code
//#define PID_Kp 0.1						// proportional gain term
//#define PID_Ki 0.005						// integral gain term
//#define PID_Kd 0.01						// derivative gain term

enum tcPIDState {							// PID state machine
	PID_OFF = 0,							// PID is off
	PID_ON
};

/**** Sensor default parameters ***/

#define SENSOR_SAMPLES 					20		// number of sensor samples to take for each reading period
#define SENSOR_SAMPLE_VARIANCE_MAX 		1.1		// number of standard deviations from mean to reject a sample
#define SENSOR_READING_VARIANCE_MAX 	20		// reject entire reading if std_dev exceeds this amount
#define SENSOR_NO_POWER_TEMPERATURE 	-2		// detect thermocouple amplifier disconnected if readings stay below this temp
#define SENSOR_DISCONNECTED_TEMPERATURE 400		// sensor is DISCONNECTED if over this temp (works w/ both 5v and 3v refs)
#define SENSOR_TICK_SECONDS 			0.01	// 10 ms

#define SENSOR_SLOPE 		0.489616568		// derived from AD597 chart between 80 deg-C and 300 deg-C
#define SENSOR_OFFSET 		-0.419325433	// derived from AD597 chart between 80 deg-C and 300 deg-C

#define SURFACE_OF_THE_SUN 	5505			// termperature at the surface of the sun in Celcius
#define HOTTER_THAN_THE_SUN 10000			// a temperature that is hotter than the surface of the sun
#define ABSOLUTE_ZERO 		-273.15			// Celcius
#define LESS_THAN_ZERO 		-274			// Celcius - a value the thermocouple sensor cannot output

enum tcSensorState {						// main state machine
	SENSOR_OFF = 0,							// sensor is off or uninitialized
	SENSOR_NO_DATA,							// interim state before first reading is complete
	SENSOR_ERROR,							// a sensor error occurred. Don't use the data
	SENSOR_HAS_DATA							// sensor has valid data
};

enum tcSensorCode {							// success and failure codes
	SENSOR_IDLE = 0,						// sensor is idling
	SENSOR_TAKING_READING,					// sensor is taking samples for a reading
	SENSOR_ERROR_BAD_READINGS,				// ERROR: too many number of bad readings
	SENSOR_ERROR_DISCONNECTED,				// ERROR: thermocouple detected as disconnected
	SENSOR_ERROR_NO_POWER					// ERROR: detected lack of power to thermocouple amplifier
};

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
// I prefer these to be static in the .c file but the scope needs to be 
// global to allow the report.c functions to get at the variables

typedef struct DeviceStruct {	// hardware devices that are part of the chip
	uint8_t tick_flag;			// true = the timer interrupt fired
	uint8_t tick_10ms_count;	// 10ms down counter
	uint8_t tick_100ms_count;	// 100ms down counter
	uint8_t tick_1sec_count;	// 1 second down counter
	double pwm_freq;			// save it for stopping and starting PWM
} Device;

typedef struct HeaterStruct {
	uint8_t state;				// heater state
	uint8_t code;				// heater code (more information about heater state)
	uint8_t	toggle;
	int8_t hysteresis;			// number of successive readings in or out or regulation before changing state
	uint8_t bad_reading_max;	// sets number of successive bad readings before declaring an error
	uint8_t bad_reading_count;	// count of successive bad readings
	double temperature;			// current heater temperature
	double setpoint;			// set point for regulation
	double regulation_range;	// +/- range to consider heater in regulation
	double regulation_timer;	// time taken so far in a HEATING cycle
	double ambient_timeout;		// timeout beyond which regulation has failed (seconds)
	double regulation_timeout;	// timeout beyond which regulation has failed (seconds)
	double ambient_temperature;	// temperature below which it's ambient temperature (heater failed)
	double overheat_temperature;// overheat temperature (cutoff temperature)
} Heater;

typedef struct PIDstruct {		// PID controller itself
	uint8_t state;				// PID state (actually very simple)
	uint8_t code;				// PID code (more information about PID state)
	double output;				// also used for anti-windup on integral term
	double output_max;			// saturation filter max
	double output_min;			// saturation filter min
	double error;				// current error term
	double prev_error;			// error term from previous pass
	double integral;			// integral term
	double derivative;			// derivative term
//	double dt;					// pid time constant
	double Kp;					// proportional gain
	double Ki;					// integral gain 
	double Kd;					// derivative gain
} PID;

typedef struct SensorStruct {
	uint8_t state;				// sensor state
	uint8_t code;				// sensor return code (more information about state)
	uint8_t sample_idx;			// index into sample array
	uint8_t samples;			// number of samples in final average
	double temperature;			// high confidence temperature reading
	double std_dev;				// standard deviation of sample array
	double sample_variance_max;	// sample deviation above which to reject a sample
	double reading_variance_max;// standard deviation to reject the entire reading
	double disconnect_temperature;	// bogus temperature indicates thermocouple is disconnected
	double no_power_temperature;	// bogus temperature indicates no power to thermocouple amplifier
	double sample[SENSOR_SAMPLES];	// array of sensor samples in a reading
	double test;
} Sensor;

// allocations
Device device;					// Device is always a singleton (there is only one device)
Heater heater;					// allocate one heater...
PID pid;						// ...with one PID channel...
Sensor sensor;					// ...and one sensor channel

/******************************************************************************
 * FUNCTION PROTOTYPES
 ******************************************************************************/

void heater_init(void);
void heater_on(double setpoint);
void heater_off(uint8_t state, uint8_t code);
void heater_callback(void);

void sensor_init(void);
void sensor_on(void);
void sensor_off(void);
void sensor_start_reading(void);
uint8_t sensor_get_state(void);
uint8_t sensor_get_code(void);
double sensor_get_temperature(void);
void sensor_callback(void);

void pid_init();
void pid_reset();
double pid_calculate(double setpoint,double temperature);

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


/******************************************************************************
 * DEFINE UNIT TESTS
 ******************************************************************************/

#ifdef __TF1_UNIT_TESTS
void tf1_unit_tests(void);
#define	TF1_UNIT_TESTS tf1_unit_tests();
#else
#define	TF1_UNIT_TESTS
#endif // __UNIT_TESTS

#endif
