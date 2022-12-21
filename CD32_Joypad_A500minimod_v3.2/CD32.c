/* CD32 joypad to USB A500 mini THEGamepad mimic
 * Copyright (C) 2022 Francis-Olivier Gradel, B.Eng.
 *
 * Using V-USB code from OBJECTIVE DEVELOPMENT Software GmbH. http://www.obdev.at/products/vusb/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The author may be contacted at info@retronicdesign.com
 */
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "usbconfig.h"
#include "CD32.h"

static char CD32Init(void);
static void CD32Update(void);
static char CD32Changed(char id);
static char CD32BuildReport(unsigned char *reportBuffer, char id);

static unsigned int last_update_state=0;
static unsigned int last_reported_state=0;

static char CD32Init(void)
{
	/* PB0   = PIN1 = UP 	(I,1)
	 * PB1   = PIN2 = DOWN	(I,1)
	 * PB2   = PIN3 = LEFT  (I,1)
	 * PB3   = PIN4 = RIGHT	(I,1)
	 * PC1&3 = PIN5 = LOAD  (IN, 1, in case of std joystick, discarded though)
	 * PB4   = PIN6 = BUT RED  (I,1)
	 * PB5   = PIN7 = VCC 	(O,1)
	 * PD7   = PIN8 = GND	(O,0)
	 * PC0&2 = PIN9 = BUT BLUE 	(I,1)
	 */
	
	DDRB &= ~((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4));
	PORTB |= ((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4));

	DDRC &= ~((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));
	PORTC |= ((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));

	DDRB |= (1<<PB5);
	PORTB |= (1<<PB5);

	DDRD |= (1<<PD7);
	PORTD &= ~(1<<PD7);

	return 0;
}

static void CD32Update(void)
{
	unsigned char button;

	/* Normal Mode ***********************/
	/* PB0 = UP (IN, 1)
	 * PB1 = DOWN (IN, 1)
	 * PB2 = LEFT (IN, 1)
	 * PB3 = RIGHT (IN, 1)
	 * PB4 = BUT RED (IN, 1)
	 * PC2 = BUT BLUE (IN, 1)
	 * PC3 = LOAD  (IN, 1, in case of std joystick, discarded though)
	 */
	
	DDRB &= ~((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4));
	PORTB |= ((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4));

	DDRC &= ~((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));
	PORTC |= ((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));

	_delay_us(100);

	last_update_state = 0x0000 | (unsigned int)(PINB&0x1F) | (unsigned int)((PINC&0x04)<<3);
	/* last_update state format:
	 * 
	 * 15 14    13         12          11    10     9   8    7 6 5    4   3     2    1    0
	 * 0  PAUSE LEFT_FRONT RIGHT_FRONT GREEN YELLOW RED BLUE X X BLUE RED RIGHT LEFT DOWN UP
	 */

	/* Scanning Mode *********************/
	/* PB0 = UP (IN, 1)
	 * PB1 = DOWN (IN, 1)
	 * PB2 = LEFT (IN,1 )
	 * PB3 = RIGHT (IN, 1)
	 * PB4 = CLK (OUT, 1)
	 * PC2 = DATA (IN, 0)
	 * PC3 = SHIFT (OUT, 0) 
	 */
	
	DDRB &= ~((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3));
	DDRC &= ~((1<<PC2));

	DDRB |= (1<<PB4);
	DDRC |= (1<<PC3);

	PORTB |= ((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4)); // CLK=1
	PORTC &= ~((1<<PC2)|(1<<PC3)); // SHIFT = 0, DATA = HI-Z

	for(button=0;button<7;button++)
	{
		_delay_us(100);
		PORTB &= ~(1<<PB4);	// CLK=0
		_delay_us(100);
		last_update_state |= (((PINC&(1<<PC2))?1:0)<<(button+8));
		PORTB |= (1<<PB4);	// CLK=1
	}
}

static char CD32Changed(char id)
{
	return (last_update_state != last_reported_state);
}

#define REPORT_SIZE 3

static char CD32BuildReport(unsigned char *reportBuffer, char id)
{
	int x,y;
	unsigned int tmp;
	
	if (reportBuffer)
	{
		y = x = 0x80;

		tmp = ~last_update_state;
		
		if (tmp&(1<<PB3)) { x = 0xff; }
		if (tmp&(1<<PB2)) { x = 0x00; }
		if (tmp&(1<<PB1)) { y = 0xff; }
		if (tmp&(1<<PB0)) { y = 0x00; }

		reportBuffer[0] = x;
		reportBuffer[1] = y;
		reportBuffer[2] = 0;
		if (tmp&(1<<4)) reportBuffer[2] |= (1<<2);
		if (tmp&(1<<5)) reportBuffer[2] |= (1<<1);

		/* Dual detection for red and blue */
		if (tmp&(1<<9)) reportBuffer[2] |= (1<<2);
		if (tmp&(1<<8)) reportBuffer[2] |= (1<<1);

		if (tmp&(1<<10)) reportBuffer[2] |= (1<<0);
		if (tmp&(1<<11)) reportBuffer[2] |= (1<<3);
		if (tmp&(1<<12)) reportBuffer[2] |= (1<<5);
		if (tmp&(1<<13)) reportBuffer[2] |= (1<<4);
		if (tmp&(1<<14)) reportBuffer[2] |= (1<<7);
	}
	last_reported_state = last_update_state;

	return REPORT_SIZE;
}

const char CD32_usbHidReportDescriptor[] PROGMEM = {
	0x05, 0x01,			// USAGE_PAGE (Generic Desktop)
    0x09, 0x04,			// USAGE (Joystick)
    0xa1, 0x01,			//	COLLECTION (Application)
    0x09, 0x01,			//		USAGE (Pointer)
    0xa1, 0x00,			//		COLLECTION (Physical)
	0x09, 0x30,			//			USAGE (X)
    0x09, 0x31,			//			USAGE (Y)
    0x15, 0x00,			//			LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,	//			LOGICAL_MAXIMUM (255)
    0x75, 0x08,			//			REPORT_SIZE (8)
    0x95, 0x02,			//			REPORT_COUNT (2)
    0x81, 0x02,			//			INPUT (Data,Var,Abs)
    0x05, 0x09,			//			USAGE_PAGE (Button)
    0x19, 1,			//   		USAGE_MINIMUM (Button 1)
    0x29, 8,			//   		USAGE_MAXIMUM (Button 8)
    0x15, 0x00,			//   		LOGICAL_MINIMUM (0)
    0x25, 0x01,			//   		LOGICAL_MAXIMUM (1)
    0x75, 1,			// 			REPORT_SIZE (1)
    0x95, 8,			//			REPORT_COUNT (8)
    0x81, 0x02,			//			INPUT (Data,Var,Abs)
	0x09, 0x00,         //          USAGE (Undefined) // Used to trig bootloader when SET FEATURE
    0x15, 0x00,         //          LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,   //          LOGICAL_MAXIMUM (255)
    0x75, 0x08,         //          REPORT_SIZE (8)
    0x95, 0x01,         //          REPORT_COUNT (1)
    0xb2, 0x02, 0x01,   //          FEATURE (Data,Var,Abs,Buf)	
	0xc0,				//		END_COLLECTION
    0xc0,				// END_COLLECTION
};

#define USBDESCR_DEVICE         1

// This is the same descriptor as in devdesc.c, but the product id is 0x0a99 

const char CD32_usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
    18,         /* sizeof(usbDescrDevice): length of descriptor in bytes */
    USBDESCR_DEVICE,    /* descriptor type */
    0x01, 0x01, /* USB version supported */
    USB_CFG_DEVICE_CLASS,
    USB_CFG_DEVICE_SUBCLASS,
    0,          /* protocol */
    8,          /* max packet size */
    USB_CFG_VENDOR_ID,  /* 2 bytes */
    USB_CFG_DEVICE_ID,  /* 2 bytes */
    USB_CFG_DEVICE_VERSION, /* 2 bytes */
    USB_CFG_VENDOR_NAME_LEN != 0 ? 1 : 0,         /* manufacturer string index */
    USB_CFG_DEVICE_NAME_LEN != 0 ? 2 : 0,        /* product string index */
    USB_CFG_SERIAL_NUMBER_LEN != 0 ? 3 : 0,  /* serial number string index */
    1,          /* number of configurations */
};

Gamepad CD32Joy = {
	.num_reports			=	1,
	.reportDescriptorSize	=	sizeof(CD32_usbHidReportDescriptor),
	.deviceDescriptorSize	=	sizeof(CD32_usbDescrDevice),
	.init					=	CD32Init,
	.update					=	CD32Update,
	.changed				=	CD32Changed,
	.buildReport			=	CD32BuildReport,
};

Gamepad *CD32GetGamepad(void)
{
	CD32Joy.reportDescriptor = (void*)CD32_usbHidReportDescriptor;
	CD32Joy.deviceDescriptor = (void*)CD32_usbDescrDevice;

	return &CD32Joy;
}


