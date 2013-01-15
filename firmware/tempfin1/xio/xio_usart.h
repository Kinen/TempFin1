/*
 * xio_usart.h - Common USART definitions 
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

#ifndef xio_usart_h
#define xio_usart_h

/******************************************************************************
 * USART DEVICE CONFIGS (applied during device-specific inits)
 ******************************************************************************/

// Buffer sizing
#define BUFFER_T uint_fast8_t					// fast, but limits buffer to 255 char max
#define RX_BUFFER_SIZE (BUFFER_T)64				// BUFFER_T can be 8 bits
#define TX_BUFFER_SIZE (BUFFER_T)64				// BUFFER_T can be 8 bits

#define USART_FLAGS (XIO_BLOCK |  XIO_ECHO | XIO_XOFF | XIO_LINEMODE )

/******************************************************************************
 * STRUCTURES 
 ******************************************************************************/
/* 
 * USART extended control structure 
 * Note: As defined this struct won't do buffers larger than 256 chars - 
 *	     or a max of 254 characters usable
 */
typedef struct xioUSART {
	volatile BUFFER_T rx_buf_tail;			// RX buffer read index
	volatile BUFFER_T rx_buf_head;			// RX buffer write index (written by ISR)
	volatile BUFFER_T tx_buf_tail;			// TX buffer read index  (written by ISR)
	volatile BUFFER_T tx_buf_head;			// TX buffer write index

	volatile char rx_buf[RX_BUFFER_SIZE];	// (written by ISR)
	volatile char tx_buf[TX_BUFFER_SIZE];
} xioUsart;

/******************************************************************************
 * USART CLASS AND DEVICE FUNCTION PROTOTYPES AND ALIASES
 ******************************************************************************/

void xio_init_usart(void);
FILE *xio_open_usart(const uint8_t dev, const char *addr, const CONTROL_T flags);
void xio_set_baud_usart(xioUsart *dx, const uint32_t baud);
int xio_gets_usart(xioDev *d, char *buf, const int size);
int xio_getc_usart(FILE *stream);
int xio_putc_usart(const char c, FILE *stream);
void xio_queue_RX_char_usart(const uint8_t dev, const char c);
void xio_queue_RX_string_usart(const uint8_t dev, const char *buf);

#endif
