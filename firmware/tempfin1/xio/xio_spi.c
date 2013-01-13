/*
 * xio_spi.c	- General purpose SPI device driver for xmega family
 * 				- works with avr-gcc stdio library
 *
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
/* ---- SPI Protocol ----
 *
 * The SPI master/slave protocol is designed to be as simple as possible. 
 * In short, the master transmits whenever it wants to and the slave returns
 * the next character in its output buffer whenever there's an SPI transfer.
 * No flow control is needed as the master initiates and drives all transfers.
 *
 * More details:
 *
 *	- A "message" is a line of text. Examples of messages are requests from the 
 *		master to a slave, responses to these requests, and asynchronous messages 
 *		(from a slave) that are not tied to a request.
 *
 *		Messages are terminated with a newline (aka NL, LF, line-feed). The 
 *		terminating NL is considered part of the message and should be transmitted.
 *
 *		If multiple NLs are transmitted each trailing NL is interpreted as a blank
 *		message. This is generally not good practice - so watch it.
 *
 *		Carriage return (CR) is not recognized as a newline. A CR in a message is
 *		treated as any other non-special ASCII character.
 *
 *		NULs (0x00) are not transmitted in either direction (e.g. string terminations).
 *		Depending on the master or slave internals, it may convert internal string 
 *		terminating NULs to NLs for transmission. 
 *
 *	- A slave is always in RX state - it must always be able to receive message data (MOSI).
 *
 *	- All SPI transmissions are initiated by the master and are 8 bits long. As the 
 *		slave is receiving the byte on MOSI it should be returning the next character 
 *		in its output buffer on MISO. Note that there is no inherent correlation between
 *		the char (or message) being received from the master and transmitted from the
 *		slave. It's just IO.
 *
 *		If the slave has no data to send it should return ETX (0x03) on MISO. This is 
 *		useful to distinghuish between an "empty" slave and a non-responsive SPI slave or
 *		unpopulated Kinen socket - which would return NULs or possibly 0xFFs.
 *
 *	- The master may poll for message data from the slave by sending STX chars to
 *		the slave. The slave discards all STXs and simply returns output data on these
 *		transfers. Presumably the master would stop polling once it receives an ETX 
 *		from the slave.
 */
/* ---- Low level SPI stuff ----
 *
 *	Uses Mode3, MSB first. See Atmel Xmega A 8077.doc, page 231
 */
#include <stdio.h>						// precursor for xio.h
#include <stdbool.h>					// true and false
#include <string.h>						// for memset
#include <avr/pgmspace.h>				// precursor for xio.h
#include <avr/interrupt.h>
#include <avr/sleep.h>					// needed for blocking TX

#include "xio.h"						// includes for all devices are in here

// statics
static int _gets_helper(xioDev *d, xioSpi *dx);
static char _xfer_char(xioSpi *dx, char c_out);

/******************************************************************************
 * SPI CONFIGURATION RECORDS
 ******************************************************************************/

struct cfgSPI {
	x_open x_open;			// see xio.h for typedefs
	x_ctrl x_ctrl;
	x_gets x_gets;
	x_getc x_getc;
	x_putc x_putc;
	fc_func fc_func;

	// initialization values
	uint8_t spcr; 			// initialization value for SPI configuration register
	uint8_t outbits; 		// bits to set as outputsx in PORTB
};

static struct cfgSPI const cfgSpi[] PROGMEM = {
{
	xio_open_spi,			// open function
	xio_ctrl_generic, 		// ctrl function
	xio_gets_spi,			// get string function
	xio_getc_spi,			// stdio getc function
	xio_putc_spi,			// stdio putc function
	xio_fc_null,			// flow control callback

	(1<<SPIE | 1<<SPE | 1<<SPIE | 1<<SPE),	// mode 3 operation
//	(1<<SPIE | 1<<SPE)						// mode 0 operation
	(1<<DDB4)				// Set SCK, MOSI, SS to input, MISO to output
}
};

// fast accessors
#define SPI ds[XIO_DEV_SPI]							// device struct accessor
#define SPIx sp[XIO_DEV_SPI - XIO_DEV_SPI_OFFSET]	// SPI extended struct accessor

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/*
 *	xio_init_spi() - init entire SPI system
 */
void xio_init_spi(void)
{
	for (uint8_t i=0; i<XIO_DEV_SPI_COUNT; i++) {
		xio_open_generic(XIO_DEV_SPI_OFFSET + i,
						(x_open)pgm_read_word(&cfgSpi[i].x_open),
						(x_ctrl)pgm_read_word(&cfgSpi[i].x_ctrl),
						(x_gets)pgm_read_word(&cfgSpi[i].x_gets),
						(x_getc)pgm_read_word(&cfgSpi[i].x_getc),
						(x_putc)pgm_read_word(&cfgSpi[i].x_putc),
						(fc_func)pgm_read_word(&cfgSpi[i].fc_func));
	}
}

/*
 *	xio_open_spi() - open a specific SPI device
 */
FILE *xio_open_spi(const uint8_t dev, const char *addr, const CONTROL_T flags)
{
	xioDev *d = &ds[dev];						// setup device struct pointer
	uint8_t idx = dev - XIO_DEV_SPI_OFFSET;
	d->x = &sp[idx];							// setup extended struct pointer
	xioSpi *dx = (xioSpi *)d->x;

	memset (dx, 0, sizeof(xioSpi));				// clear all values
	xio_ctrl_generic(d, flags);					// setup flags

	// setup internal RX/TX control buffers
	dx->rx_buf_head = 1;		// can't use location 0 in circular buffer
	dx->rx_buf_tail = 1;
	dx->tx_buf_head = 1;
	dx->tx_buf_tail = 1;

	PRR &= ~PRSPI_bm;			// Enable SPI in the power reduction register (system.h)
	SPCR |= (uint8_t)pgm_read_byte(&cfgSpi[idx].spcr);
	DDRB |= (uint8_t)pgm_read_byte(&cfgSpi[idx].outbits);

	return (&d->file);							// return FILE reference
}

/* 
 *	xio_gets_spi() - read a complete line from an SPI device
 * _gets_helper() 	 - non-blocking character getter for gets
 *
 *	Retains line context across calls - so it can be called multiple times.
 *	Reads as many characters as it can until any of the following is true:
 *
 *	  - Encounters newline indicating a complete line (returns XIO_OK)
 *	  - Encounters and empty buffer and SPI returns ETX indicating there is no data to xfer (return XIO_EAGAIN)
 *	  - Read would cause output buffer overflow (return XIO_BUFFER_FULL)
 *
 *	Note: LINEMODE flag in device struct is ignored. It's ALWAYS LINEMODE here.
 *	Note: CRs are not recognized as NL chars - this function assumes slaves never send CRs. 
 */
int xio_gets_spi(xioDev *d, char *buf, const int size)
{
	xioSpi *dx = (xioSpi *)d->x;				// get SPI device struct pointer

	// first time thru initializations
	if (d->flag_in_line == false) {
		d->flag_in_line = true;					// yes, we are busy getting a line
		d->len = 0;								// zero buffer
		d->buf = buf;
		d->size = size;
		d->signal = XIO_SIG_OK;					// reset signal register
	}

	// process chars until done or blocked
	while (true) {
		switch (_gets_helper(d,dx)) {
			case (XIO_BUFFER_EMPTY): 
				return (XIO_EAGAIN); 			// empty condition
			case (XIO_BUFFER_FULL_NON_FATAL): 
				return (XIO_BUFFER_FULL_NON_FATAL);// overrun err
			case (XIO_EOL): 
				return (XIO_OK);				// got complete line
//			case (XIO_EAGAIN): 
//				break;							// break the switch; loop for next character
		}
	}
	return (XIO_ERR);							// never supposed to get here
}

static int _gets_helper(xioDev *d, xioSpi *dx)
{
	if (d->len >= d->size) {					// handle buffer overruns
		d->buf[d->size] = NUL;					// terminate line (d->size is zero based)
		d->signal = XIO_SIG_EOL;
		return (XIO_BUFFER_FULL_NON_FATAL);
	}
	char c = xio_getc_spi(&d->file);			// get a character or ETX

	if (c == LF) {								// end-of-line condition
		d->buf[(d->len)++] = NUL;
		d->signal = XIO_SIG_EOL;
		d->flag_in_line = false;				// clear in-line state (reset)
		return (XIO_EOL);						// return for end-of-line
	}
	if (c == ETX) {
		return (XIO_BUFFER_EMPTY);				// nothing more to read
	}
	d->buf[(d->len)++] = c;						// write character to buffer
	return (XIO_EAGAIN);
}

/*
 * xio_getc_spi() - stdio compatible character RX routine for 328 slave
 *
 *	This function is always non-blocking or it would create a deadlock.
 */
int xio_getc_spi(FILE *stream)
{
//	xioSpi *dx = ((xioDev *)stream->udata)->x;	// cache SPI device struct pointer

	// handle the RX buffer empty case
	if (SPIx.rx_buf_head == SPIx.rx_buf_tail) {	// RX buffer empty
		return(_FDEV_ERR);
	}

	// handle the case where the RX buffer has data
	if (--(SPIx.rx_buf_tail) == 0) {				// advance RX tail (RXQ read ptr)
		SPIx.rx_buf_tail = SPI_RX_BUFFER_SIZE-1;	// -1 is OBOE avoidance
	}
	return(SPIx.rx_buf[SPIx.rx_buf_tail]);			// return that char from RX buf
}

/* 
 * SPI Slave RX Interrupt() - interrupts on byte received
 *
 * Uses a 2 phase state machine to toggle back and forth between ADDR and DATA bytes
 */
ISR(SPI_STC_vect)
{
/*
	// receive address byte
	if (ki_slave.phase == KINEN_ADDR) {
		ki_slave.phase = KINEN_DATA;	// advance phase
		ki_slave.addr = SPDR;		// read and save the address byte
		if (ki_command == KINEN_WRITE) { // write is simple...
			SPDR = KINEN_OK_BYTE;			// already saved addr, now return an OK
		} else {
			if (ki_slave.addr < KINEN_COMMON_MAX) {	// handle OCB address space
				SPDR = ki.array[ki_slave.addr];
			} else {								// handle device address space
				if ((ki_status = device_read_byte(ki_slave.addr, &ki_slave.data)) == SC_OK) {
					SPDR = ki_slave.data;
				} else {
					SPDR = KINEN_ERR_BYTE;
				}
			}
		}

	// receive data byte
	} else {
		ki_slave.phase = KINEN_ADDR;	// advance phase
		ki_slave.data = SPDR;		// read and save the data byte
		if (ki_command == KINEN_WRITE) {
			if (ki_slave.addr < KINEN_COMMON_MAX) {
				ki_status = _slave_write_byte(ki_slave.addr, ki_slave.data);
			} else {
				ki_status = device_write_byte(ki_slave.addr, ki_slave.data);
			}
		}
	}
*/
}

/*
 * xio_putc_spi() - stdio compatible character TX routine for 328 SPI slave
 *
 *	Writes a character into the TX buffer for pickup by the Master.
 */
int xio_putc_spi(const char c, FILE *stream)
{
	xioSpi *dx = ((xioDev *)stream->udata)->x;	// cache SPI device struct pointer

	if ((--(dx->tx_buf_head)) == 0) { 			// adv buffer head with wrap
		dx->tx_buf_head = SPI_TX_BUFFER_SIZE-1;
	}
	if (dx->tx_buf_head != dx->tx_buf_tail) {	// buffer is not full
		dx->tx_buf[dx->tx_buf_head] = c;		// write char to buffer
	} else { 									// buffer-full - toss the incoming character
		if ((++(dx->tx_buf_head)) > SPI_TX_BUFFER_SIZE-1) {	// reset the head
			dx->tx_buf_head = 1;
		}
		((xioDev *)stream->udata)->signal = XIO_SIG_OVERRUN; // signal a buffer overflow
		return (_FDEV_ERR);
	}
	return (XIO_OK);
}

/*
#define xfer_bit(mask, c_out, c_in) \
	dx->data_port->OUTCLR = SPI_SCK_bm; \
	if ((c_out & mask) == 0) { dx->data_port->OUTCLR = SPI_MOSI_bm; } \
	else { dx->data_port->OUTSET = SPI_MOSI_bm; } \
	if (dx->data_port->IN & SPI_MISO_bm) c_in |= (mask); \
	dx->data_port->OUTSET = SPI_SCK_bm;	

static char _xfer_char(xioSpi *dx, char c_out)
{
	char c_in = 0;

	dx->ssel_port->OUTCLR = dx->ssbit;			// drive slave select lo (active)
	xfer_bit(0x80, c_out, c_in);
	xfer_bit(0x40, c_out, c_in);
	xfer_bit(0x20, c_out, c_in);
	xfer_bit(0x10, c_out, c_in);
	xfer_bit(0x08, c_out, c_in);
	xfer_bit(0x04, c_out, c_in);
	xfer_bit(0x02, c_out, c_in);
	xfer_bit(0x01, c_out, c_in);
	dx->ssel_port->OUTSET = dx->ssbit;

	return (c_in);
}
*/

