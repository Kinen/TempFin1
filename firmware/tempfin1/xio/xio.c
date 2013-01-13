/*
 * xio.c - eXtended IO devices - common code file
 * Part of Kinen project
 *
 * Copyright (c) 2010 - 2013 Alden S. Hart Jr.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* ----- XIO - eXtended Device IO System ----
 *
 * XIO provides common access to native and derived xmega devices (see table below) 
 * XIO devices are compatible with avr-gcc stdio and also provide some special functions 
 * that are not found in stdio.
 *
 * Stdio support:
 *	- http://www.nongnu.org/avr-libc/user-manual/group__avr__stdio.html
 * 	- Stdio compatible putc() and getc() functions provided for each device
 *	- This enables fgets, printf, scanf, and other stdio functions
 * 	- Full support for formatted printing is provided (including floats)
 * 	- Assignment of a default device to stdin, stdout & stderr is provided 
 *	- printf() and printf_P() send to stdout, so use fprintf() to stderr
 *		for things that should't go over RS485 in SLAVE mode 
 *
 * Facilities provided beyond stdio:
 *	- Supported devices include:
 *		- USB (derived from USART)
 *		- RS485 (derived from USART)
 *		- SPI devices and slave channels
 *		- Program memory "files" (read only)
 *	- Stdio FILE streams are managed as bindings to the above devices
 *	- Additional functions provided include:
 *		- open() - initialize parameters, addresses and flags
 *		- gets() - non-blocking input line reader - extends fgets
 *		- ctrl() - ioctl-like knockoff for setting device parameters (flags)
 *		- signal handling: interrupt on: feedhold, cycle_start, ctrl-x software reset
 *		- interrupt buffered RX and TX functions 
 *		- XON/XOFF software flow control
 */
/* ----- XIO - Some Internals ----
 *
 * XIO layers are: (1) xio virtual device (root), (2) xio device type, (3) xio devices
 *
 * The virtual device has the following methods:
 *	xio_init() - initialize the entire xio system
 *	xio_open() - open a device indicated by the XIO_DEV number
 *	xio_ctrl() - set control flags for XIO_DEV device
 *	xio_gets() - get a string from the XIO_DEV device (non blocking line reader)
 *	xio_getc() - read a character from the XIO_DEV device (not stdio compatible)
 *	xio_putc() - write a character to the XIO_DEV device (not stdio compatible)
 *  xio_set_baud() - set baud rates for devices for which this is meaningful
 *
 * The device type layer currently knows about USARTS, SPI, and File devices. Methods are:
 *	xio_init_<type>() - initializes the devices of that type
 *
 * The device layer currently supports: USB, RS485, SPI channels, PGM file reading. methods:
 *	xio_open<device>() - set up the device for use or reset the device
 *	xio_ctrl<device>() - change device flag controls
 *	xio_gets<device>() - get a string from the device (non-blocking)
 *	xio_getc<device>() - read a character from the device (stdio compatible)
 *	xio_putc<device>() - write a character to the device (stdio compatible)
 *
 * The virtual level uses XIO_DEV_xxx numeric device IDs for reference. 
 * Lower layers are called using the device structure pointer xioDev *d
 * The stdio compatible functions use pointers to the stdio FILE structs.
 */
/* ---- Efficiency Hack ----
 *
 * Device and extended structs are usually referenced via their pointers. E.g:
 *
 *	  xioDev *d = &ds[dev];						// setup device struct ptr
 *    xioUsart *dx = (xioUsart *)d->x; 			// setup USART struct ptr
 *
 * In some cases a static reference is used for time critical regions like raw 
 * character IO. This is measurably faster even under -Os. For example:
 *
 *    #define USB (ds[dev])						// USB device struct accessor
 *    #define USBu ((xioUsart *)(ds[dev].x))	// USB extended struct accessor
 */

#include <string.h>					// for memset()
#include <stdbool.h>
#include <stdio.h>					// precursor for xio.h
#include <avr/pgmspace.h>			// precursor for xio.h

#include "xio.h"					// all device includes are nested here

/*
 * xio_init() - initialize entire xio sub-system
 */
void xio_init()
{
	// setup device types
	xio_init_usart();
	xio_init_spi();
//	xio_init_file();

	// open individual devices (file device opens occur at time-of-use)
	xio_open(XIO_DEV_USART, 0,USART_FLAGS);
	xio_open(XIO_DEV_SPI, 0, SPI_FLAGS);

	// setup std devices for printf/fprintf to work
	xio_set_stdin(XIO_DEV_USART);
	xio_set_stdout(XIO_DEV_USART);
	xio_set_stderr(XIO_DEV_SPI);
}

/*
 * xio_open_generic() - generic (and partial) open function for any device
 *
 *	This binds the main fucntions and sets up the stdio FILE structure
 *	udata is used to point back to the device struct so it can be gotten 
 *	from getc() and putc() functions. 
 *
 *	Requires device specific open() to be run afterward to complete the setup
 */
void xio_open_generic(uint8_t dev, x_open x_open, x_ctrl x_ctrl, x_gets x_gets, x_getc x_getc, x_putc x_putc, fc_func fc_func)
{
	xioDev *d = &ds[dev];
	memset (d, 0, sizeof(xioDev));
	d->dev = dev;

	// bind functions to device structure
	d->x_open = x_open;
	d->x_ctrl = x_ctrl;
	d->x_gets = x_gets;
	d->x_getc = x_getc;		// you don't need to bind these unless you are going to use them directly
	d->x_putc = x_putc;		// they are bound into the fdev stream struct
	d->fc_func = fc_func;	// flow control function or null FC function

	// setup the stdio FILE struct and link udata back to the device struct
	fdev_setup_stream(&d->file, x_putc, x_getc, _FDEV_SETUP_RW);
	fdev_set_udata(&d->file, d);		// reference self for udata 
}

/* 
 * PUBLIC ENTRY POINTS - acces the functions via the XIO_DEV number
 * xio_open() - open function 
 * xio_gets() - entry point for non-blocking get line function
 * xio_getc() - entry point for getc (not stdio compatible)
 * xio_putc() - entry point for putc (not stdio compatible)
 *
 * It might be prudent to run an assertion such as below, but we trust the callers:
 * 	if (dev < XIO_DEV_COUNT) blah blah blah
 *	else  return (_FDEV_ERR);	// XIO_NO_SUCH_DEVICE
 */
FILE *xio_open(uint8_t dev, const char *addr, CONTROL_T flags)
{
	return (ds[dev].x_open(dev, addr, flags));
}

int xio_gets(const uint8_t dev, char *buf, const int size) 
{
	return (ds[dev].x_gets(&ds[dev], buf, size));
}

int xio_getc(const uint8_t dev) 
{ 
	return (ds[dev].x_getc(&ds[dev].file)); 
}

int xio_putc(const uint8_t dev, const char c)
{
	return (ds[dev].x_putc(c, &ds[dev].file)); 
}

/*
 * xio_ctrl() - PUBLIC set control flags (top-level XIO_DEV access)
 * xio_ctrl_generic() - PRIVATE but generic set-control-flags
 */
int xio_ctrl(const uint8_t dev, const CONTROL_T flags)
{
	return (xio_ctrl_generic(&ds[dev], flags));
}

#define SETFLAG(t,f) if ((flags & t) != 0) { d->f = true; }
#define CLRFLAG(t,f) if ((flags & t) != 0) { d->f = false; }

int xio_ctrl_generic(xioDev *d, const CONTROL_T flags)
{
	SETFLAG(XIO_BLOCK,		flag_block);
	CLRFLAG(XIO_NOBLOCK,	flag_block);
//	SETFLAG(XIO_XOFF,		flag_xoff);
//	CLRFLAG(XIO_NOXOFF,		flag_xoff);
	SETFLAG(XIO_ECHO,		flag_echo);
	CLRFLAG(XIO_NOECHO,		flag_echo);
//	SETFLAG(XIO_CRLF,		flag_crlf);
//	CLRFLAG(XIO_NOCRLF,		flag_crlf);
//	SETFLAG(XIO_IGNORECR,	flag_ignorecr);
//	CLRFLAG(XIO_NOIGNORECR,	flag_ignorecr);
//	SETFLAG(XIO_IGNORELF,	flag_ignorelf);
//	CLRFLAG(XIO_NOIGNORELF,	flag_ignorelf);
	SETFLAG(XIO_LINEMODE,	flag_linemode);
	CLRFLAG(XIO_NOLINEMODE,	flag_linemode);
	return (XIO_OK);
}

/*
 * xio_set_baud() - PUBLIC entry to set baud rate
 *	Currently this only works on USART devices
 */
int xio_set_baud(const uint8_t dev, const uint8_t baud)
{
	xioUsart *dx = (xioUsart *)&us[dev - XIO_DEV_USART_OFFSET];
	xio_set_baud_usart(dx, baud);
	return (XIO_OK);
}

/*
 * xio_fc_null() - flow control null function
 */
void xio_fc_null(xioDev *d)
{
	return;
}

/*
 * xio_set_stdin()  - set stdin from device number
 * xio_set_stdout() - set stdout from device number
 * xio_set_stderr() - set stderr from device number
 */

void xio_set_stdin(const uint8_t dev)  { stdin  = &ds[dev].file; }
void xio_set_stdout(const uint8_t dev) { stdout = &ds[dev].file; }
void xio_set_stderr(const uint8_t dev) { stderr = &ds[dev].file; }


//########################################################################

#ifdef __UNIT_TESTS
#ifdef __UNIT_TEST_XIO

/*
 * xio_tests() - a collection of tests for xio
 */

void xio_unit_tests()
{
	FILE * fdev;

	fdev = xio_open(XIO_DEV_SPI1, 0, SPI_FLAGS);
	while (true) {
		xio_putc_spi(0x55, fdev);
	}
	
//	fdev = xio_open(XIO_DEV_USB, 0, USB_FLAGS);
//	xio_getc_usart(fdev);
	
/*
	fdev = xio_open(XIO_DEV_PGM, 0, PGM_FLAGS);
//	xio_puts_pgm("ABCDEFGHIJKLMNOP\n", fdev);
	xio_putc_pgm('A', fdev);
	xio_putc_pgm('B', fdev);
	xio_putc_pgm('C', fdev);
	xio_getc_pgm(fdev);
	xio_getc_pgm(fdev);
	xio_getc_pgm(fdev);
*/
}

#endif
#endif
