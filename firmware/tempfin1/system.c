/*
 * system.c - general hardware support functions
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
#include <avr\pgmspace.h> 

#include "system.h"

/*
 * sys_init() - lowest level hardware init
 */

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
