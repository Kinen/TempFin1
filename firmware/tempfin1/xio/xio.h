/*
 * xio.h - Xmega IO devices - common header file
 * Part of TinyG project
 *
 * Copyright (c) 2010 - 2013 Alden S. Hart Jr.
 *
 * TinyG is free software: you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, 
 * or (at your option) any later version.
 *
 * TinyG is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with TinyG  If not, see <http://www.gnu.org/licenses/>.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* XIO devices are compatible with avr-gcc stdio, so formatted printing 
 * is supported. To use this sub-system outside of TinyG you may need 
 * some defines in tinyg.h. See notes at end of this file for more details.
 */
/* Note: anything that includes xio.h first needs the following:
 * 	#include <stdio.h>				// needed for FILE def'n
 *	#include <stdbool.h>			// needed for true and false 
 *	#include <avr/pgmspace.h>		// defines prog_char, PSTR
 */
/* Note: This file contains load of sub-includes near the middle
 *	#include "xio_file.h"
 *	#include "xio_usart.h"
 *	#include "xio_spi.h"
 *	#include "xio_signals.h"
 *	(possibly more)
 */

#ifndef xio_h
#define xio_h

/*************************************************************************
 *	Device configurations
 *************************************************************************/
// Pre-allocated XIO devices (configured devices)
// Unused devices are commented out. All this needs to line up.

enum xioDev {			// TYPE:	DEVICE:
	XIO_DEV_USART,		// USART	USART device
	XIO_DEV_SPI,		// SPI		SPI device
	XIO_DEV_COUNT		// total device count (must be last entry)
};
// If your change these ^, check these v

#define XIO_DEV_USART_COUNT 	1 				// # of USART devices
#define XIO_DEV_USART_OFFSET	0				// offset for computing indices

#define XIO_DEV_SPI_COUNT 		1 				// # of SPI devices
#define XIO_DEV_SPI_OFFSET		XIO_DEV_USART_COUNT	// offset for computing indicies

//#define XIO_DEV_FILE_COUNT		1				// # of FILE devices
//#define XIO_DEV_FILE_OFFSET		(XIO_DEV_USART_COUNT + XIO_DEV_SPI_COUNT) // index into FILES

/******************************************************************************
 * Device structures
 *
 * Each device has 3 structs. The generic device struct is declared below.
 * It embeds a stdio stream struct "FILE". The FILE struct uses the udata
 * field to back-reference the generic struct so getc & putc can get at it.
 * Lastly there's an 'x' struct which contains data specific to each dev type.
 *
 * The generic open() function sets up the generic struct and the FILE stream. 
 * the device opens() set up the extended struct and bind it ot the generic.
 ******************************************************************************/
// NOTE" "FILE *" is another way of saying "struct __file *"

#define CONTROL_T uint16_t

typedef struct xioDEVICE {							// common device struct (one per dev)
	// references and self references
	uint8_t dev;							// self referential device number
	FILE file;								// stdio FILE stream structure
	void *x;								// extended device struct binding (static)

	// function bindings
	FILE *(*x_open)(const uint8_t dev, const char *addr, const CONTROL_T flags);
	int (*x_ctrl)(struct xioDEVICE *d, const CONTROL_T flags);		// set device control flags
	int (*x_gets)(struct xioDEVICE *d, char *buf, const int size);	// non-blocking line reader
	int (*x_getc)(FILE *);					// read char (stdio compatible)
	int (*x_putc)(char, FILE *);			// write char (stdio compatible)
	void (*fc_func)(struct xioDEVICE *d);	// flow control callback function

	// device configuration flags
	uint8_t flag_block;
	uint8_t flag_echo;
//	uint8_t flag_crlf;
//	uint8_t flag_ignorecr;
//	uint8_t flag_ignorelf;
	uint8_t flag_linemode;
//	uint8_t flag_xoff;						// xon/xoff enabled

	// private working data and runtime flags
	int size;								// text buffer length (dynamic)
	uint8_t len;							// chars read so far (buf array index)
	uint8_t signal;							// signal value
	uint8_t flag_in_line;					// used as a state variable for line reads
	uint8_t flag_eol;						// end of line detected
	uint8_t flag_eof;						// end of file detected
	char *buf;								// text buffer binding (can be dynamic)
} xioDev;

typedef FILE *(*x_open)(const uint8_t dev, const char *addr, const CONTROL_T flags);
typedef int (*x_ctrl)(xioDev *d, const CONTROL_T flags);
typedef int (*x_gets)(xioDev *d, char *buf, const int size);
typedef int (*x_getc)(FILE *);
typedef int (*x_putc)(char, FILE *);
typedef void (*fc_func)(xioDev *d);

/*************************************************************************
 *	Sub-Includes and static allocations
 *************************************************************************/
// Put all sub-includes here so only xio.h is needed elsewhere
#include "xio_usart.h"
#include "xio_spi.h"
//#include "xio_file.h"
#include "xio_signals.h"

// Static structure allocations
xioDev 		ds[XIO_DEV_COUNT];			// allocate top-level dev structs
xioUsart 	us[XIO_DEV_USART_COUNT];	// USART extended IO structs
xioSpi 		sp[XIO_DEV_SPI_COUNT];		// SPI extended IO structs
//xioFile 	fs[XIO_DEV_FILE_COUNT];		// FILE extended IO structs
xioSignals	sig;						// signal flags
extern struct controllerSingleton tg;	// needed by init() for default source

/*************************************************************************
 *	Function Prototypes
 *************************************************************************/

// public functions (virtual class) 
void xio_init(void);
FILE *xio_open(const uint8_t dev, const char *addr, const CONTROL_T flags);
int xio_ctrl(const uint8_t dev, const CONTROL_T flags);
int xio_gets(const uint8_t dev, char *buf, const int size);
int xio_getc(const uint8_t dev);
int xio_putc(const uint8_t dev, const char c);
int xio_set_baud(const uint8_t dev, const uint8_t baud_rate);

// generic functions (private, but at virtual level)
int xio_ctrl_generic(xioDev *d, const CONTROL_T flags);
void xio_open_generic(uint8_t dev, x_open x_open, x_ctrl x_ctrl, x_gets x_gets, x_getc x_getc, x_putc x_putc, fc_func fc_func);
void xio_fc_null(xioDev *d);			// NULL flow control callback
//void xio_fc_usart(xioDev *d);			// XON/XOFF flow control callback

// std devices
void xio_init_stdio(void);				// set std devs & do startup prompt
void xio_set_stdin(const uint8_t dev);
void xio_set_stdout(const uint8_t dev);
void xio_set_stderr(const uint8_t dev);

/*************************************************************************
 * SUPPORTING DEFINTIONS - SHOULD NOT NEED TO CHANGE
 *************************************************************************/
/*
 * xio control flag values
 *
 * if using 32 bits must cast 1 to uint32_t for bit evaluations to work correctly
 * #define XIO_BLOCK	((uint32_t)1<<1)		// 32 bit example. Change CONTROL_T to uint32_t
 */

#define XIO_BLOCK		((uint16_t)1<<0)		// enable blocking reads
#define XIO_NOBLOCK		((uint16_t)1<<1)		// disable blocking reads
#define XIO_XOFF 		((uint16_t)1<<2)		// enable XON/OFF flow control
#define XIO_NOXOFF 		((uint16_t)1<<3)		// disable XON/XOFF flow control
#define XIO_ECHO		((uint16_t)1<<4)		// echo reads from device to stdio
#define XIO_NOECHO		((uint16_t)1<<5)		// disable echo
#define XIO_CRLF		((uint16_t)1<<6)		// convert <LF> to <CR><LF> on writes
#define XIO_NOCRLF		((uint16_t)1<<7)		// do not convert <LF> to <CR><LF> on writes
#define XIO_IGNORECR	((uint16_t)1<<8)		// ignore <CR> on reads
#define XIO_NOIGNORECR	((uint16_t)1<<9)		// don't ignore <CR> on reads
#define XIO_IGNORELF	((uint16_t)1<<10)		// ignore <LF> on reads
#define XIO_NOIGNORELF	((uint16_t)1<<11)		// don't ignore <LF> on reads
#define XIO_LINEMODE	((uint16_t)1<<12)		// special <CR><LF> read handling
#define XIO_NOLINEMODE	((uint16_t)1<<13)		// no special <CR><LF> read handling

/*
 * Generic XIO signals and error conditions. 
 * See signals.h for application specific signal defs and routines.
 */

enum xioSignals {
	// generic signals
	XIO_SIG_OK,				// OK
	XIO_SIG_EAGAIN,			// would block
	XIO_SIG_EOL,			// end-of-line encountered (string has data)
	XIO_SIG_EOF,			// end-of-file encountered (string has no data)
	XIO_SIG_OVERRUN,		// buffer overrun
	XIO_SIG_RESET,			// cancel operation immediately
	XIO_SIG_DELETE,			// backspace or delete character (BS, DEL)
	XIO_SIG_BELL			// BELL character (BEL, ^g)

	// application signals
//	XIO_SIG_FEEDHOLD,		// pause operation
//	XIO_SIG_CYCLE_START,	// start or resume operation

};

/* Some useful ASCII definitions */

#define NUL (char)0x00		//  ASCII NUL char (0) (not "NULL" which is a pointer)
#define STX (char)0x02		// ^b - STX
#define ETX (char)0x03		// ^c - ETX
#define ENQ (char)0x05		// ^e - ENQuire
#define BEL (char)0x07		// ^g - BEL
#define BS  (char)0x08		// ^h - backspace 
#define TAB (char)0x09		// ^i - character
#define LF	(char)0x0A		// ^j - line feed
#define VT	(char)0x0B		// ^k - kill stop
#define CR	(char)0x0D		// ^m - carriage return
#define XON (char)0x11		// ^q - DC1, XON, resume
#define XOFF (char)0x13		// ^s - DC3, XOFF, pause
#define CAN (char)0x18		// ^x - Cancel, abort
#define ESC (char)0x1B		// ^[ - ESC(ape)
#define DEL (char)0x7F		//  DEL(ete)

/* Signal character mappings */

#define CHAR_RESET CAN
#define CHAR_FEEDHOLD (char)'!'
#define CHAR_CYCLE_START (char)'~'

/* XIO return codes
 * These codes are the "inner nest" for the TG_ return codes. 
 * The first N TG codes correspond directly to these codes.
 * This eases using XIO by itself (without tinyg) and simplifes using
 * tinyg codes with no mapping when used together. This comes at the cost 
 * of making sure these lists are aligned. TG_should be based on this list.
 */

enum xioCodes {
	XIO_OK = 0,				// OK - ALWAYS ZERO
	XIO_ERR,				// generic error return (errors start here)
	XIO_EAGAIN,				// function would block here (must be called again)
	XIO_NOOP,				// function had no-operation	
	XIO_COMPLETE,			// operation complete
	XIO_TERMINATE,			// operation terminated (gracefully)
	XIO_RESET,				// operation reset (ungraceful)
	XIO_EOL,				// function returned end-of-line
	XIO_EOF,				// function returned end-of-file 
	XIO_FILE_NOT_OPEN,		// file is not open
	XIO_FILE_SIZE_EXCEEDED, // maximum file size exceeded
	XIO_NO_SUCH_DEVICE,		// illegal or unavailable device
	XIO_BUFFER_EMPTY,		// more of a statement of fact than an error code
	XIO_BUFFER_FULL_FATAL,
	XIO_BUFFER_FULL_NON_FATAL,
	XIO_INITIALIZING		// system initializing, not ready for use
};
#define XIO_ERRNO_MAX XIO_BUFFER_FULL_NON_FATAL



/* ASCII characters used by Gcode or otherwise unavailable for special use. 
    See NIST sections 3.3.2.2, 3.3.2.3 and Appendix E for Gcode uses.
    See http://www.json.org/ for JSON notation

    hex	    char    name        used by:
    ----    ----    ----------  --------------------
    0x00    NUL	    null        everything
    0x01    SOH     ctrl-A
    0x02    STX     ctrl-B      Kinen SPI protocol
    0x03    ETX     ctrl-C      Kinen SPI protocol
    0x04    EOT     ctrl-D
    0x05    ENQ     ctrl-E
    0x06    ACK     ctrl-F
    0x07    BEL     ctrl-G
    0x08    BS      ctrl-H
    0x09    HT      ctrl-I
    0x0A    LF      ctrl-J
    0x0B    VT      ctrl-K
    0x0C    FF      ctrl-L
    0x0D    CR      ctrl-M
    0x0E    SO      ctrl-N
    0x0F    SI      ctrl-O
    0x10    DLE     ctrl-P
    0x11    DC1     ctrl-Q      XOFF
    0x12    DC2     ctrl-R		
    0x13    DC3     ctrl-S      XON
    0x14    DC4     ctrl-T		
    0x15    NAK     ctrl-U
    0x16    SYN     ctrl-V
    0x17    ETB     ctrl-W    
    0x18    CAN     ctrl-X      TinyG / grbl software reset
    0x19    EM      ctrl-Y
    0x1A    SUB     ctrl-Z
    0x1B    ESC     ctrl-[
    0x1C    FS      ctrl-\
    0x1D    GS      ctrl-]
    0x1E    RS      ctrl-^
    0x1F    US      ctrl-_

    0x20    <space>             Gcode blocks
    0x21    !       excl point  TinyG feedhold
    0x22    "       quote       JSON notation
    0x23    #       number      Gcode parameter prefix
    0x24    $       dollar      TinyG / grbl out-of-cycle settings prefix
    0x25    &       ampersand   universal symbol for logical AND (not used here)    
    0x26    %       percent		
    0x27    '       single quote	
    0x28    (       open paren  Gcode comments
    0x29    )       close paren Gcode comments
    0x2A    *       asterisk    Gcode expressions
    0x2B    +       plus        Gcode numbers, parameters and expressions
    0x2C    ,       comma       JSON notation
    0x2D    -       minus       Gcode numbers, parameters and expressions
    0x2E    .       period      Gcode numbers, parameters and expressions
    0x2F    /       fwd slash   Gcode expressions & block delete char
    0x3A    :       colon       JSON notation
    0x3B    ;       semicolon
    0x3C    <       less than   Gcode expressions
    0x3D    =       equals      Gcode expressions
    0x3E    >       greaterthan Gcode expressions
    0x3F    ?       question mk TinyG / grbl query
    0x40    @       at symbol	

    0x5B    [       open bracketGcode expressions
    0x5C    \       backslash   JSON notation (escape)
    0x5D    ]       close brack Gcode expressions
    0x5E    ^       caret       Reserved for TinyG in-cycle command prefix
    0x5F    _       underscore

    0x60    `       grave accnt	
    0x7B    {       open curly  JSON notation
    0x7C    |       pipe        universal symbol for logical OR (not used here)
    0x7D    }       close curly JSON notation
    0x7E    ~       tilde       TinyG cycle start
    0x7F    DEL	
*/

#define __UNIT_TEST_XIO			// include and run xio unit tests
#ifdef __UNIT_TEST_XIO
void xio_unit_tests(void);
#define	XIO_UNITS xio_unit_tests();
#else
#define	XIO_UNITS
#endif // __UNIT_TEST_XIO

#endif
