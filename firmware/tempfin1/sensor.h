/*
 * sensor.h - Kinen temperature controller - temperature sensor functions
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
#ifndef sensor_h
#define sensor_h

/******************************************************************************
 * PARAMETERS AND SETTINGS
 ******************************************************************************/

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

/******************************************************************************
 * STRUCTURES 
 ******************************************************************************/
// I prefer these to be static in the .c file but the scope needs to be 
// global to allow the report.c functions to get at the variables

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
} sensor_t;
sensor_t sensor;				// allocate one sensor channel

/******************************************************************************
 * FUNCTION PROTOTYPES
 ******************************************************************************/

void sensor_init(void);
void sensor_on(void);
void sensor_off(void);
void sensor_start_reading(void);
uint8_t sensor_get_state(void);
uint8_t sensor_get_code(void);
double sensor_get_temperature(void);
void sensor_callback(void);

#endif
