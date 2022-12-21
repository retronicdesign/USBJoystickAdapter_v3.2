/* Redoctane DDR dance pad to USB
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
#include "RODDR.h"

static char DDRDancePadInit(void);
static void DDRDancePadUpdate(void);
static char DDRDancePadChanged(char id);
static char DDRDancePadBuildReport(unsigned char *reportBuffer, char id);

static unsigned char last_update_state=0;
static unsigned char last_reported_state=0;

static char DDRDancePadInit(void)
{

	/* PB0 = PIN1 = LEFT (IN, 1) 
	 * PB1 = PIN2 = UP (IN, 1)
	 * PB2 = PIN3 = RIGHT (IN, 1)
	 * PB3 = PIN4 = DOWN (IN, 1)
	 * PC3 = PIN5 = nc (IN, 0)
	 * PB4 = PIN6 = nc (IN, 0)
	 * PB5 = PIN7 = BUTTON B (IN, 1)
	 * PD7 = PIN8 = BUTTON A (IN, 1)
	 * PC2 = PIN9 = GND (OUT, 0)
	 */
	
	DDRB &= ~((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5));
	DDRC &= ~((1<<PC1)|(1<<PC3));
	DDRC |= ((1<<PC0)|(1<<PC2));
	DDRD &= ~(1<<PD7);

	PORTB |= ((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB5));
	PORTB &= ~((1<<PB4));
	PORTC &= ~((1<<PC1)|(1<<PC3)|(1<<PC0)|(1<<PC2));
	PORTD |= (1<<PD7);

	return 0;
}

static void DDRDancePadUpdate(void)
{
	last_update_state = ((PINB&0x2F) | ((PIND&0x80)));
}

static char DDRDancePadChanged(char id)
{
	return (last_update_state != last_reported_state);
}

#define REPORT_SIZE 1

static char DDRDancePadBuildReport(unsigned char *reportBuffer, char id)
{
	unsigned char tmp;
	
	if (reportBuffer)
	{
		tmp = last_update_state ^ 0xff;
		reportBuffer[0] = 0;
		if (tmp&(1<<0)) reportBuffer[0] |= 0x01; //left
		if (tmp&(1<<3)) reportBuffer[0] |= 0x02; //down
		if (tmp&(1<<1)) reportBuffer[0] |= 0x04; //up
		if (tmp&(1<<2)) reportBuffer[0] |= 0x08; //right
		if (tmp&(1<<5)) reportBuffer[0] |= 0x40; //B (Back)
		if (tmp&(1<<7)) reportBuffer[0] |= 0x80; //A (Start/Select)
	}
	last_reported_state = last_update_state;

	return REPORT_SIZE;
}

const char DDRDancePad_usbHidReportDescriptor[] PROGMEM = {

	0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game Pad)
    0xa1, 0x01,                    // COLLECTION (Application)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
	0x29, 0x08,                    //     USAGE_MAXIMUM (Button 8)
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

const char DDRDancePad_usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
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

Gamepad DDRDancePadJoy = {
	.num_reports			=	1,
	.reportDescriptorSize	=	sizeof(DDRDancePad_usbHidReportDescriptor),
	.deviceDescriptorSize	=	sizeof(DDRDancePad_usbDescrDevice),
	.init					=	DDRDancePadInit,
	.update					=	DDRDancePadUpdate,
	.changed				=	DDRDancePadChanged,
	.buildReport			=	DDRDancePadBuildReport,
};

Gamepad *DDRDancePadGetGamepad(void)
{
	DDRDancePadJoy.reportDescriptor = (void*)DDRDancePad_usbHidReportDescriptor;
	DDRDancePadJoy.deviceDescriptor = (void*)DDRDancePad_usbDescrDevice;

	return &DDRDancePadJoy;
}


