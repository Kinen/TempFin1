/*
 * xio_spi.h	- General purpose SPI device driver for xmega family
 * 				- works with avr-gcc stdio library
 *
 * Part of TinyG project
 *
 * Copyright (c) 2013 Alden S. Hart Jr.
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

#ifndef xio_spi_h
#define xio_spi_h

/******************************************************************************
 * SPI DEVICE CONFIGS (applied during device-specific inits)
 ******************************************************************************/

// SPI global accessor defines
#define SPI1 ds[XIO_DEV_SPI1]				// device struct accessor
#define SPI1u sp[XIO_DEV_SPI1 - XIO_DEV_SPI_OFFSET]	// usart extended struct accessor

#define SPI2 ds[XIO_DEV_SPI2]				// device struct accessor
#define SPI2u sp[XIO_DEV_SPI2 - XIO_DEV_SPI_OFFSET]	// usart extended struct accessor

// Buffer sizing
#define SPIBUF_T uint_fast8_t				// fast, but limits SPI buffers to 255 char max
#define SPI_RX_BUFFER_SIZE (SPIBUF_T)64		// BUFFER_T can be 8 bits
#define SPI_TX_BUFFER_SIZE (SPIBUF_T)64		// BUFFER_T can be 8 bits

// Alternates for larger buffers - mostly for debugging
//#define SPIBUF_T uint16_t					// slower, but supports larger buffers
//#define SPI_RX_BUFFER_SIZE (SPIBUF_T)512	// BUFFER_T must be 16 bits if >255
//#define SPI_TX_BUFFER_SIZE (SPIBUF_T)512	// BUFFER_T must be 16 bits if >255
//#define SPI_RX_BUFFER_SIZE (SPIBUF_T)1024	// 2048 is the practical upper limit
//#define SPI_TX_BUFFER_SIZE (SPIBUF_T)1024	// 2048 is practical upper limit given RAM


//**** SPI device configuration ****
//NOTE: XIO_BLOCK / XIO_NOBLOCK affects reads only. Writes always block. (see xio.h)

#define SPI_FLAGS (XIO_BLOCK |  XIO_ECHO | XIO_LINEMODE)

#define BIT_BANG 		0					// use this value if no USART is being used
#define SPI_USART 		BIT_BANG			// USB usart or BIT_BANG value
#define SPI_RX_ISR_vect	BIT_BANG		 	// (RX) reception complete IRQ
#define SPI_TX_ISR_vect	BIT_BANG			// (TX) data register empty IRQ

//#define SPI_USART USARTC1					// USB usart
//#define SPI_RX_ISR_vect USARTC0_RXC_vect 	// (RX) reception complete IRQ
//#define SPI_TX_ISR_vect USARTC0_DRE_vect	// (TX) data register empty IRQ

// The bit mappings for SCK / MISO / MOSI / SS1 map to the xmega SPI device pinouts
#define SPI_DATA_PORT PORTC					// port for SPI data lines
#define SPI_SCK_bp  	7					// SCK - clock bit position (pin is wired on board)
#define SPI_MISO_bp 	6					// MISO - bit position (pin is wired on board)
#define SPI_MOSI_bp 	5					// MOSI - bit position (pin is wired on board)
#define SPI_SS1_PORT	SPI_DATA_PORT		// slave select assignments
#define SPI_SS1_bp  	4					// SS1 - slave select #1
// additional slave selects
#define SPI_SS2_PORT	PORTB
#define SPI_SS2_bp  	3					// SS1 - slave select #2

#define SPI_MOSI_bm 	(1<<SPI_MOSI_bp)	// bit masks for the above
#define SPI_MISO_bm 	(1<<SPI_MISO_bp)
#define SPI_SCK_bm 		(1<<SPI_SCK_bp)
#define SPI_SS1_bm 		(1<<SPI_SS1_bp)
#define SPI_SS2_bm 		(1<<SPI_SS2_bp)

#define SPI_INBITS_bm 	(SPI_MISO_bm)
#define SPI_OUTBITS_bm 	(SPI_MOSI_bm | SPI_SCK_bm | SPI_SS1_bm | SPI_SS2_bm)
#define SPI_OUTCLR_bm 	(0)					// outputs init'd to 0
#define SPI_OUTSET_bm 	(SPI_OUTBITS_bm)		// outputs init'd to 1


/******************************************************************************
 * STRUCTURES 
 ******************************************************************************/
/* 
 * SPI extended control structure 
 * Note: As defined this struct won't do buffers larger than 256 chars - 
 *	     or a max of 254 characters usable
 */

typedef struct xioSPI {
//	USART_t *usart;					// USART used for SPI (unless it's bit banged)
//	PORT_t *data_port;				// port used for data transmission (MOSI, MOSI, SCK)
//	PORT_t *ssel_port;				// port used for slave select
	uint8_t ssbit;					// slave select bit used for this device

	volatile BUFFER_T rx_buf_tail;	// RX buffer read index
	volatile BUFFER_T rx_buf_head;	// RX buffer write index (written by ISR)

	volatile char rx_buf[SPI_RX_BUFFER_SIZE];	// (may be written by an ISR)
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
