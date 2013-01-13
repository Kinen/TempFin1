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

void sys_init(void);					// master hardware init

/*** Clock settings ***/
#undef F_CPU							// set for delays
#define F_CPU 16000000UL				// should always precede <avr/delay.h>

// Clock Crystal Config. Pick one:
//#define __CLOCK_INTERNAL_8MHZ TRUE	// use internal oscillator
//#define __CLOCK_EXTERNAL_8MHZ	TRUE	// uses PLL to provide 32 MHz system clock
#define __CLOCK_EXTERNAL_16MHZ TRUE		// uses PLL to provide 32 MHz system clock

/*** Power reduction register mappings ***/
// you shouldn't need to change this
#define PRADC_bm 	(1<<PRADC)
#define PRUSART0_bm	(1<<PRUSART0)
#define PRSPI_bm 	(1<<PRSPI)
#define PRTIM1_bm	(1<<PRTIM1)
#define PRTIM0_bm	(1<<PRTIM0)
#define PRTIM2_bm	(1<<PRTIM2)
#define PRTWI_bm	(1<<PRTWI)

#endif
