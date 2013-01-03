/*
  serial.c - Low level functions for sending and recieving bytes via the serial port
  Part of Grbl

  Copyright (c) 2009-2011 Simen Svale Skogsrud
  Copyright (c) 2011-2012 Sungeun K. Jeon

  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
*/
/*	This code was initially inspired by the wiring_serial module by 
 *	David A. Mellis which used to be a part of the Arduino project.
 */ 

#ifndef serial_h
#define serial_h

#define BAUD_RATE 9600
#define RX_BUFFER_SIZE 256
#define TX_BUFFER_SIZE 256
#define SERIAL_NO_DATA 0xFF

#ifndef F_CPU
#define F_CPU 8000000
#endif

void serial_init(long baud);
void serial_write(uint8_t data);
uint8_t serial_read();
void serial_reset_read_buffer();	// Reset and empty data in read buffer. Used by e-stop and reset.

#endif
