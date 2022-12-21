/* Fairchild Channel F joystick to USB
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
#include "Fairchild.h"

static char FairchildFInit(void);
static void FairchildFUpdate(void);
static char FairchildFChanged(char id);
static char FairchildFBuildReport(unsigned char *reportBuffer, char id);

static unsigned char last_update_state=0;
static unsigned char last_reported_state=0;

static char FairchildFInit(void)
{

	/* PIN1 = PB0 = (I,1) TWIST LEFT
	 * PIN2 = PB1 = (I,1) TWIST RIGHT
	 * PIN3 = PB2 = (I,1) PULL UP
	 * PIN4 = PB3 = (I,1) PUSH DOWN
	 * PIN5 = PC1 = (I,0) nc, PC3 = (I,1) RIGHT
	 * PIN6 = PB4 = (I,1) UP
	 * PIN7 = PB5 = (I,1) DOWN
	 * PIN8 = PD7 = (I,1) LEFT
	 * PIN9 = PC0 = (I,0) nc, PC2 = (O,0) GND
	 */
	
	DDRB &= ~((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5));
	PORTB |= ((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5));

	DDRC &= ~((1<<PC0)|(1<<PC1)|(1<<PC3));
	DDRC |= (1<<PC2);
	PORTC &= ~((1<<PC0)|(1<<PC2)|(1<<PC1));
	PORTC |= (1<<PC3);

	DDRD &= ~(1<<PD7);
	PORTD |= (1<<PD7);

	return 0;
}

static void FairchildFUpdate(void)
{
	last_update_state = ((PINB&((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5))) | ((PINC&(1<<PC3))<<3) | (PIND&(1<<PD7)));
}

static char FairchildFChanged(char id)
{
	return (last_update_state != last_reported_state);
}

#define REPORT_SIZE 4

static char FairchildFBuildReport(unsigned char *reportBuffer, char id)
{
	int x,y,xx,yy;
	unsigned char tmp;
	
	if (reportBuffer)
	{
		y = x = yy = xx = 0x80;
		

		tmp = last_update_state ^ 0xff;
		
		if (tmp&(1<<6)) { y = 0x00; } // UP	
		if (tmp&(1<<4)) { y = 0xff; } // DOWN	
		if (tmp&(1<<5)) { x = 0x00; } // LEFT
		if (tmp&(1<<7)) { x = 0xff; } // RIGHT

		if (tmp&(1<<PB2)) { yy = 0x00; } // PULL UP	
		if (tmp&(1<<PB3)) { yy = 0xff; } // PUSH DOWN	
		if (tmp&(1<<PB0)) { xx = 0x00; } // TWIST CCW
		if (tmp&(1<<PB1)) { xx = 0xff; } // TWIST CW

		reportBuffer[0] = x;
		reportBuffer[1] = y;
		reportBuffer[2] = xx;
		reportBuffer[3] = yy;

	}
	last_reported_state = last_update_state;

	return REPORT_SIZE;
}

const char FairchildF_usbHidReportDescriptor[] PROGMEM = {

	0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game Pad)
    0xa1, 0x01,                    // COLLECTION (Application)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x09, 0x32,                    //     USAGE (Z)
    0x09, 0x33,                    //     USAGE (Rx)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //     LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x04,                    //     REPORT_COUNT (4)
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

const char FairchildF_usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
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

Gamepad FairchildFJoy = {
	.num_reports			=	1,
	.reportDescriptorSize	=	sizeof(FairchildF_usbHidReportDescriptor),
	.deviceDescriptorSize	=	sizeof(FairchildF_usbDescrDevice),
	.init					=	FairchildFInit,
	.update					=	FairchildFUpdate,
	.changed				=	FairchildFChanged,
	.buildReport			=	FairchildFBuildReport,
};

Gamepad *FairchildFGetGamepad(void)
{
	FairchildFJoy.reportDescriptor = (void*)FairchildF_usbHidReportDescriptor;
	FairchildFJoy.deviceDescriptor = (void*)FairchildF_usbDescrDevice;

	return &FairchildFJoy;
}


