/*
 * xio_usart.h - Common USART definitions 
 * Part of Kinen project
 *
 * Copyright (c) 2010 - 2013 Alden S. Hart Jr.
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

#define serial_write(c) xio_putc_usart(c,0)

/******************************************************************************
 * USART DEVICE CONFIGS (applied during device-specific inits)
 ******************************************************************************/

// Buffer sizing
#define BUFFER_T uint_fast8_t					// fast, but limits buffer to 255 char max
#define RX_BUFFER_SIZE (BUFFER_T)128			// BUFFER_T can be 8 bits
#define TX_BUFFER_SIZE (BUFFER_T)128			// BUFFER_T can be 8 bits

#define USART_FLAGS (XIO_BLOCK |  XIO_ECHO | XIO_XOFF | XIO_LINEMODE )


//**** USB device configuration ****
//NOTE: XIO_BLOCK / XIO_NOBLOCK affects reads only. Writes always block. (see xio.h)

//#define USB_BAUD	 XIO_BAUD_115200


/*
#define USB_USART USARTC0						// USB usart
#define USB_RX_ISR_vect USARTC0_RXC_vect	 	// (RX) reception complete IRQ
#define USB_TX_ISR_vect USARTC0_DRE_vect		// (TX) data register empty IRQ

#define USB_PORT PORTC							// port where the USART is located
#define USB_CTS_bp (0)							// CTS - bit position (pin is wired on board)
#define USB_CTS_bm (1<<USB_CTS_bp)				// CTS - bit mask
#define USB_RTS_bp (1)							// RTS - bit position (pin is wired on board)
#define USB_RTS_bm (1<<USB_RTS_bp)				// RTS - bit mask
#define USB_RX_bm (1<<2)						// RX pin bit mask
#define USB_TX_bm (1<<3)						// TX pin bit mask

#define USB_INBITS_bm (USB_CTS_bm | USB_RX_bm)	// input bits
#define USB_OUTBITS_bm (USB_RTS_bm | USB_TX_bm)	// output bits
#define USB_OUTCLR_bm (0)						// outputs init'd to 0
#define USB_OUTSET_bm (USB_RTS_bm | USB_TX_bm)	// outputs init'd to 1
*/

/* 
 * Serial Configuration Settings
 *
 * 	Serial config settings are here because various modules will be opening devices
 *	The BSEL / BSCALE values provided below assume a 32 Mhz clock
 *	Assumes CTRLB CLK2X bit (0x04) is not enabled
 *	These are carried in the bsel and bscale tables in xio_usart.c
 */

// Baud rate configuration
#define	XIO_BAUD_DEFAULT XIO_BAUD_115200

enum xioBAUDRATES {         		// BSEL	  BSCALE
		XIO_BAUD_UNSPECIFIED = 0,	//	0		0	  // use default value
		XIO_BAUD_9600,				//	207		0
		XIO_BAUD_19200,				//	103		0
		XIO_BAUD_38400,				//	51		0
		XIO_BAUD_57600,				//	34		0
		XIO_BAUD_115200,			//	33		(-1<<4)
		XIO_BAUD_230400,			//	31		(-2<<4)
		XIO_BAUD_460800,			//	27		(-3<<4)
		XIO_BAUD_921600,			//	19		(-4<<4)
		XIO_BAUD_500000,			//	1		(1<<4)
		XIO_BAUD_1000000			//	1		0
};

enum xioFCState { 
		FC_DISABLED = 0,			// flo control is disabled
		FC_IN_XON,					// normal, un-flow-controlled state
		FC_IN_XOFF					// flow controlled state
};

/******************************************************************************
 * STRUCTURES 
 ******************************************************************************/
/* 
 * USART extended control structure 
 * Note: As defined this struct won't do buffers larger than 256 chars - 
 *	     or a max of 254 characters usable
 */
typedef struct xioUSART {
	uint8_t fc_char;			 			// flow control character to send
	volatile uint8_t fc_state;				// flow control state

	volatile BUFFER_T rx_buf_tail;			// RX buffer read index
	volatile BUFFER_T rx_buf_head;			// RX buffer write index (written by ISR)
//	volatile BUFFER_T rx_buf_count;			// RX buffer counter for flow control

	volatile BUFFER_T tx_buf_tail;			// TX buffer read index  (written by ISR)
	volatile BUFFER_T tx_buf_head;			// TX buffer write index
//	volatile BUFFER_T tx_buf_count;

//	USART_t *usart;							// xmega USART structure
//	PORT_t	*port;							// corresponding port

	volatile char rx_buf[RX_BUFFER_SIZE];	// (written by ISR)
	volatile char tx_buf[TX_BUFFER_SIZE];
} xioUsart;

/******************************************************************************
 * USART CLASS AND DEVICE FUNCTION PROTOTYPES AND ALIASES
 ******************************************************************************/

void xio_init_usart(void);
FILE *xio_open_usart(const uint8_t dev, const char *addr, const CONTROL_T flags);
void xio_set_baud_usart(xioUsart *dx, const uint32_t baud);
void xio_xoff_usart(xioUsart *dx);
void xio_xon_usart(xioUsart *dx);
int xio_gets_usart(xioDev *d, char *buf, const int size);
int xio_getc_usart(FILE *stream);
int xio_putc_usart(const char c, FILE *stream);

// handy helpers
BUFFER_T xio_get_rx_bufcount_usart(const xioUsart *dx);
BUFFER_T xio_get_tx_bufcount_usart(const xioUsart *dx);
BUFFER_T xio_get_usb_rx_free(void);

void xio_queue_RX_char_usart(const uint8_t dev, const char c);
void xio_queue_RX_string_usart(const uint8_t dev, const char *buf);
void xio_queue_RX_char_usb(const char c);		// simulate char rcvd into RX buffer
void xio_queue_RX_string_usb(const char *buf);	// simulate receving a whole string
void xio_queue_RX_char_rs485(const char c);		// simulate char rcvd into RX buffer
void xio_queue_RX_string_rs485(const char *buf);// simulate rec'ving a whole string

#endif
