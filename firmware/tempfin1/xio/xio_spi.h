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
 * SPI DEVICE CONFIGS AND STRUCTURES
 ******************************************************************************/

//#define SPI_MODE		(1<<SPIE | 1<<SPE)						// mode 0 operation / slave
#define SPI_MODE		(1<<SPIE | 1<<SPE | 1<<CPOL | 1<<CPHA)	// mode 3 operation / slave
#define SPI_OUTBITS		(1<<DDB4)			// Set SCK, MOSI, SS to input, MISO to output
#define SPI_XIO_FLAGS 	(XIO_LINEMODE)

#define ubuf_t uint_fast8_t					// fast, but limits buffer to 255 char max
#define SPI_RX_BUFFER_SIZE (ubuf_t)DEFAULT_BUFFER_SIZE
#define SPI_TX_BUFFER_SIZE (ubuf_t)255

// these structs must be the same as xioBuf. Only the buf array size can be different.
typedef struct xioSpiRX {
	buffer_t size;							// initialize to SPI_RX_BUFFER_SIZE-1
	buffer_t rd;							// read index
	buffer_t wr;							// write index (written by ISR)
	char buf[SPI_RX_BUFFER_SIZE];
} xioSpiRX_t;

typedef struct xioSpiTX {
	buffer_t size;
	buffer_t rd;							// read index
	buffer_t wr;							// write index (written by ISR)
	char buf[SPI_TX_BUFFER_SIZE];
} xioSpiTX_t;

/******************************************************************************
 * SPI FUNCTION PROTOTYPES AND ALIASES
 ******************************************************************************/

xioDev_t *xio_init_spi(uint8_t dev);
FILE *xio_open_spi(const uint8_t dev, const char *addr, const flags_t flags);
int xio_gets_spi(xioDev_t *d, char *buf, const int size);
int xio_putc_spi(const char c, FILE *stream);
int xio_getc_spi(FILE *stream);

#endif // xio_spi_h
