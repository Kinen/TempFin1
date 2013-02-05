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

#define BUILD_NUMBER 		005.01			// Reorganized modules
#define VERSION_NUMBER		0.1				// firmware major version
#define HARDWARE_VERSION	0.1				// board revision number

/****** DEVELOPMENT SETTINGS ******/

//#define __CANNED_STARTUP					// run any canned startup moves
//#define __DISABLE_PERSISTENCE				// disable EEPROM writes for faster simulation
//#define __SUPPRESS_STARTUP_MESSAGES 		// what it says
//#define __TF1_UNIT_TESTS					// uncomment to compile and run TempFin1 unit tests
											// uncomment __XIO_UNIT_TESTS in xio.h if you need those


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
