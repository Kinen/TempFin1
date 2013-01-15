/*
 * kinen_slave_328p.c - Kine device slave driver for Atmega328P 
 * Part of Kinen Motion Control Project
 *
 * Copyright (c) 2012 - 2013 Alden S. Hart Jr.
 *
 * Kinen is licensed under the OSHW 1.0 license
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
#include <string.h>				// for memset
#include <avr/interrupt.h>

#include "kinen_core.h"
#include "kinen_slave_328p.h"
#include "tempfin1.h"			// main device file

/*
 * kinen_slave_init() - setup atmega SPI peripheral to be the OCB slave 
 */
void kinen_slave_init(void)
{
	return;
}
