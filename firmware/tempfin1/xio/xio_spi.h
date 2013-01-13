/*
 * xio_spi.h	- General purpose SPI device driver for xmega family
 * 				- works with avr-gcc stdio library
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

#ifndef xio_spi_h
#define xio_spi_h

/******************************************************************************
 * SPI DEVICE CONFIGS (applied during device-specific inits)
 ******************************************************************************/

// Buffer sizing
#define SPIBUF_T uint_fast8_t				// fast, but limits SPI buffers to 255 char max
#define SPI_RX_BUFFER_SIZE (SPIBUF_T)64		// BUFFER_T can be 8 bits
#define SPI_TX_BUFFER_SIZE (SPIBUF_T)64		// BUFFER_T can be 8 bits

#define SPI_FLAGS (XIO_BLOCK |  XIO_ECHO | XIO_LINEMODE)

/******************************************************************************
 * STRUCTURES 
 ******************************************************************************/
/* 
 * SPI extended control structure 
 * Note: As defined this struct won't do buffers larger than 256 chars - 
 *	     or a max of 254 characters usable
 */
typedef struct xioSPI {
	volatile BUFFER_T rx_buf_tail;				// RX buffer read index
	volatile BUFFER_T rx_buf_head;				// RX buffer write index (written by ISR)
	volatile BUFFER_T tx_buf_tail;				// TX buffer read index  (written by ISR)
	volatile BUFFER_T tx_buf_head;				// TX buffer write index

	volatile char rx_buf[SPI_RX_BUFFER_SIZE];	// (may be written by an ISR)
	volatile char tx_buf[SPI_TX_BUFFER_SIZE];	// (may be written by an ISR)
} xioSpi;

/******************************************************************************
 * SPI FUNCTION PROTOTYPES AND ALIASES
 ******************************************************************************/

void xio_init_spi(void);
FILE *xio_open_spi(const uint8_t dev, const char *addr, const CONTROL_T flags);
int xio_gets_spi(xioDev *d, char *buf, const int size);
int xio_putc_spi(const char c, FILE *stream);
int xio_getc_spi(FILE *stream);

#endif
