/* "Atari" style joystick to USB (Using Interrupts)
 * Copyright (C) 2021 Francis-Olivier Gradel, B.Eng.
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
#include <avr/interrupt.h>
#include <string.h>
#include "usbconfig.h"
#include "ataristyle.h"

static char atariStyleInit(void);
static char atariStyleChanged(char id);
static char atariStyleBuildReport(unsigned char *reportBuffer, char id);

static unsigned char last_update_state=0;
static unsigned char last_reported_state=0;

static char atariStyleInit(void)
{
	/* PB0   = PIN1 = UP 	(I,1) PCINT0
	 * PB1   = PIN2 = DOWN	(I,1) PCINT1
	 * PB2   = PIN3 = LEFT  (I,1) PCINT2
	 * PB3   = PIN4 = RIGHT	(I,1) PCINT3
	 * PC1&3 = PIN5 = BUT3  (I,1) PCINT11
	 * PB4   = PIN6 = BUT1  (I,1) PCINT4
	 * PB5   = PIN7 = VCC 	(O,1)
	 * PD7   = PIN8 = GND	(O,0)
	 * PC0&2 = PIN9 = BUT2	(I,1) PCINT10
	 */
	
	DDRB &= ~((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4));
	DDRB |= (1<<PB5);
	DDRD |= (1<<PD7);

	PORTB |= ((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5));
	PORTD &= ~(1<<PD7);

	DDRC &= ~((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));
	PORTC |= ((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));
	
	PCICR |= ((1<<PCIE0)|(1<<PCIE1)); // Enable interrupts on PCINT0 and 1
	PCMSK0 |= ((1<<PCINT0)|(1<<PCINT1)|(1<<PCINT2)|(1<<PCINT3)|(1<<PCINT4)); // Enable interrupts on PB0 to PB4 change
	PCMSK1 |= ((1<<PCINT10)|(1<<PCINT11)); // Enable interrupts on PC2 and PC3 change

	return 0;
}

ISR(PCINT1_vect,ISR_ALIASOF(PCINT0_vect));

ISR(PCINT0_vect) // Trigged whenever a change occur on the joystick
{
	last_update_state = ((PINB&0x1F) | ((PINC&0x0C)<<3));
}

static char atariStyleChanged(char id)
{
	return (last_update_state != last_reported_state);
}

#define REPORT_SIZE 3

static char atariStyleBuildReport(unsigned char *reportBuffer, char id)
{
	int x,y;
	unsigned char tmp;
	
	if (reportBuffer)
	{
		y = x = 0x80;

		tmp = last_update_state ^ 0xff;
		
		if (tmp&(1<<PB3)) { x = 0xff; }
		if (tmp&(1<<PB2)) { x = 0x00; }
		if (tmp&(1<<PB1)) { y = 0xff; }
		if (tmp&(1<<PB0)) { y = 0x00; }

		reportBuffer[0] = x;
		reportBuffer[1] = y;
		reportBuffer[2] = 0;
		if (tmp&(1<<PB4)) reportBuffer[2] |= 0x01;
		if (tmp&(1<<5)) reportBuffer[2] |= 0x02;
		if (tmp&(1<<6)) reportBuffer[2] |= 0x04;
	}

	last_reported_state = last_update_state;

	return REPORT_SIZE;
}

const char atariStyle_usbHidReportDescriptor[] PROGMEM = {

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
    0x29, 0x03,                    //     USAGE_MAXIMUM (Button 3)
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

const char atariStyle_usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
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

Gamepad atariStyleJoy = {
	.num_reports			=	1,
	.reportDescriptorSize	=	sizeof(atariStyle_usbHidReportDescriptor),
	.deviceDescriptorSize	=	sizeof(atariStyle_usbDescrDevice),
	.init					=	atariStyleInit,
	.changed				=	atariStyleChanged,
	.buildReport			=	atariStyleBuildReport,
};

Gamepad *atariStyleGetGamepad(void)
{
	atariStyleJoy.reportDescriptor = (void*)atariStyle_usbHidReportDescriptor;
	atariStyleJoy.deviceDescriptor = (void*)atariStyle_usbDescrDevice;

	return &atariStyleJoy;
}


