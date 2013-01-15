/*
 * kinen_core.h - Kinen core program definitions
 * Part of Kinen Motion Control Project
 *
 * Copyright (c) 2012 Alden S. Hart Jr.
 *
 * The Kinen Motion Control System is licensed under the OSHW 1.0 license
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef kinen_h
#define kinen_h

// function prototypes
void kinen_init(void);


// Kinen definitions

// Kinen Device Types	(this might be best in a separate kinen_defs.h file)

#define DEVICE_TYPE_NULL 0
#define DEVICE_TYPE_DUMB_STEPPER_CONTROLLER 1
#define DEVICE_TYPE_SMART_STEPPER_CONTROLLER 2
#define DEVICE_TYPE_EXTRUDER_CONTROLLER 3
#define DEVICE_TYPE_TEMPERATURE_CONTROLLER 4

// Status Codes

#define	SC_OK 0							// function completed OK
#define	SC_ERROR 1						// generic error return (EPERM)
#define	SC_EAGAIN 2						// function would block here (call again)
#define	SC_NOOP 3						// function had no-operation
#define	SC_COMPLETE 4					// operation is complete
#define SC_TERMINATE 5					// operation terminated (gracefully)
#define SC_ABORT 6						// operaation aborted
#define	SC_EOL 7						// function returned end-of-line
#define	SC_EOF 8						// function returned end-of-file 
#define	SC_FILE_NOT_OPEN 9
#define	SC_FILE_SIZE_EXCEEDED 10
#define	SC_NO_SUCH_DEVICE 11
#define	SC_BUFFER_EMPTY 12
#define	SC_BUFFER_FULL_FATAL 13 
#define	SC_BUFFER_FULL_NON_FATAL 14

// System errors (HTTP 500's if you will)
#define	SC_INTERNAL_ERROR 20			// unrecoverable internal error
#define	SC_INTERNAL_RANGE_ERROR 21		// number range other than by user input
#define	SC_FLOATING_POINT_ERROR 22		// number conversion error
#define	SC_DIVIDE_BY_ZERO 23
#define	SC_INVALID_ADDRESS 24			// address not in range
#define SC_READ_ONLY_ADDRESS 25			// tried to write tot a read-only location

// Input errors (HTTP 400's, if you will)
#define	SC_UNRECOGNIZED_COMMAND 40		// parser didn't recognize the command
#define	SC_EXPECTED_COMMAND_LETTER 41	// malformed line to parser
#define	SC_BAD_NUMBER_FORMAT 42			// number format error
#define	SC_INPUT_EXCEEDS_MAX_LENGTH 43	// input string is too long 
#define	SC_INPUT_VALUE_TOO_SMALL 44		// input error: value is under minimum
#define	SC_INPUT_VALUE_TOO_LARGE 45		// input error: value is over maximum
#define	SC_INPUT_VALUE_RANGE_ERROR 46	// input error: value is out-of-range
#define	SC_INPUT_VALUE_UNSUPPORTED 47	// input error: value is not supported
#define	SC_JSON_SYNTAX_ERROR 48			// JSON string is not well formed
#define	SC_JSON_TOO_MANY_PAIRS 49		// JSON string or has too many JSON pairs
#define	SC_NO_BUFFER_SPACE 50			// Buffer pool is full and cannot perform this operation

#endif
