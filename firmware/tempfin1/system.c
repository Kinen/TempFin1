/*
 * system.c - general hardware support functions
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

#include <stdio.h>
#include <stddef.h> 
#include <avr\pgmspace.h> 

//#include "tinyg.h"
#include "system.h"
//#include "xmega/xmega_init.h"

/*
 * sys_init() - lowest level hardware init
 */

void sys_init() 
{
//	xmega_init();		// set system clock
//	sys_port_bindings(TINYG_HARDWARE_VERSION);
}
