/* TI99 4/4a joystick to USB
 * Copyright (C) 2020 Francis-Olivier Gradel, B.Eng.
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
 *
 */
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "usbconfig.h"
#include "TI99.h"

static char TI99StyleInit(void);
static void TI99StyleUpdate(void);
static char TI99StyleChanged(char id);
static char TI99StyleBuildReport(unsigned char *reportBuffer, char id);

static unsigned char last_update_state=0;
static unsigned char last_reported_state=0;

static char TI99StyleInit(void)
{

	/* PIN1 = PB0   = (I,0) nc
	 * PIN2 = PB1   = (I,0) Joy2 select GND
	 * PIN3 = PB2   = (I,1) UP
	 * PIN4 = PB3   = (I,1) FIRE
	 * PIN5 = PB4   = (I,1) LEFT
	 * PIN6 = PC0&2 = (I,0) nc
	 * PIN7 = PC1&3 = (O,0) Joy1 select GND
	 * PIN8 = PB5   = (I,1) DOWN
	 * PIN9 = PD7   = (I,1) RIGHT
	 */
	
	DDRB &= ~((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5));
	PORTB |= ((1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5));
	PORTB &= ~((1<<PB0)|(1<<PB1));

	DDRD &= ~(1<<PD7);
	PORTD |= (1<<PD7);

	DDRC |= ((1<<PC3));
	DDRC &= ~((1<<PC0)|(1<<PC2)|(1<<PC1));
	PORTC &= ~((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));

	return 0;
}

static void TI99StyleUpdate(void)
{
	last_update_state = ((PINB&((1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5))) | (PIND&(1<<PD7)));
}

static char TI99StyleChanged(char id)
{
	return (last_update_state != last_reported_state);
}

#define REPORT_SIZE 3

static char TI99StyleBuildReport(unsigned char *reportBuffer, char id)
{
	int x,y;
	unsigned char tmp;
	
	if (reportBuffer)
	{
		y = x = 0x80;

		tmp = last_update_state ^ 0xff;
		
		if (tmp&(1<<PB2)) { y = 0x00; } // UP
		if (tmp&(1<<PB5)) { y = 0xff; } // DOWN
		if (tmp&(1<<PB4)) { x = 0x00; } // LEFT
		if (tmp&(1<<PB7)) { x = 0xff; } // RIGHT

		reportBuffer[0] = x;
		reportBuffer[1] = y;
		reportBuffer[2] = 0;
		if (tmp&(1<<PB3)) reportBuffer[2] |= 0x00000001; // Fire

	}
	last_reported_state = last_update_state;

	return REPORT_SIZE;
}

const char TI99Style_usbHidReportDescriptor[] PROGMEM = {

	0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game Pad)
    0xa1, 0x01,                    // COLLECTION (Application)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //     LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x01,                    //     USAGE_MAXIMUM (Button 1)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x08,                    //     REPORT_COUNT (8)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
	0x09, 0x00,                    //     USAGE (Undefined) // Used to trig bootloader when SET FEATURE
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //     LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0xb2, 0x02, 0x01,              //     FEATURE (Data,Var,Abs,Buf)	
    0xc0,                          //   END_COLLECTION
    0xc0                           // END_COLLECTION

};

#define USBDESCR_DEVICE         1

// This is the same descriptor as in devdesc.c, but the product id is 0x0a99 

const char TI99Style_usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
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

Gamepad TI99StyleJoy = {
	.num_reports			=	1,
	.reportDescriptorSize	=	sizeof(TI99Style_usbHidReportDescriptor),
	.deviceDescriptorSize	=	sizeof(TI99Style_usbDescrDevice),
	.init					=	TI99StyleInit,
	.update					=	TI99StyleUpdate,
	.changed				=	TI99StyleChanged,
	.buildReport			=	TI99StyleBuildReport,
};

Gamepad *TI99StyleGetGamepad(void)
{
	TI99StyleJoy.reportDescriptor = (void*)TI99Style_usbHidReportDescriptor;
	TI99StyleJoy.deviceDescriptor = (void*)TI99Style_usbDescrDevice;

	return &TI99StyleJoy;
}


