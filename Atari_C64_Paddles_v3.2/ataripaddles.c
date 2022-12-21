/* "Atari" paddles to USB (Timer polling version)
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
 * Note: If you have quirky responses, try contact cleaner in your pot! ;o)
 */
#include <avr/io.h>
#include <util/delay.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <string.h>
#include "usbconfig.h"
#include "ataripaddles.h"

#define SETUPDELAY 50	// Time to reset the capacitor back to GND
#define DIVIDER 25		// Divider of the read value to match with 0-255 (Atari Paddles 1Mohm)
//#define DIVIDER 12	// Divider of the read value to match with 0-255 (C64 Paddles 460Kohm)

void mux(char);
void resetport(char);

static char atariPaddlesInit(void);
static void atariPaddlesUpdate(void);
static char atariPaddlesChanged(char id);
static char atariPaddlesBuildReport(unsigned char *reportBuffer, char id);

volatile unsigned int channel[2];
volatile unsigned int old_channel[2];

static unsigned char button_state;
static unsigned char button_reported_state;

static char atariPaddlesInit(void)
{
	/* PIN1 = PB0 = nc
	 * PIN2 = PB1 = nc
	 * PIN3 = PB2 = BUT0 (I,1)
	 * PIN4 = PB3 = BUT1 (I,1)
	 * PIN5 = PC1 = POT1 (I,0) PC3 = (I,0)
	 * PIN6 = PB4 = nc
	 * PIN7 = PB5 = VCC  (O,1)
	 * PIN8 = PD7 = GND  (O,0)
	 * PIN9 = PC0 = POT0 (I,0) PC2 = (I,0)
	 */
	
	DDRB &= ~((1<<PB2)|(1<<PB3));
	DDRB |= (1<<PB5);
	PORTB |= ((1<<PB2)|(1<<PB3)|(1<<PB5));

	DDRD |= ((1<<PD7));
	PORTD &= ~(1<<PD7);

	DDRC &= ~((1<<PC2)|(1<<PC3));
	PORTC &= ~((1<<PC2)|(1<<PC3));

	DDRC &= ~((1<<PC0)|(1<<PC1));
	PORTC &= ~((1<<PC0)|(1<<PC1));

	TCCR1B |= ((1<<CS10)|(1<<CS11));// CPU/64 @ 12MHz = 187,5KHz

	old_channel[0]=channel[0]=0;
	old_channel[1]=channel[1]=0;

	button_state=button_reported_state=0;

	return 0;
}

static void atariPaddlesUpdate(void)
{
	// Read buttons
	button_state=(PINB&((1<<PB2)|(1<<PB3)));

	// Read paddles in polling using timer1
	channel[0]=channel[1]=0;

	for(int i=0;i<2;i++)
	{
		wdt_reset();
		DDRC |= ((1<<(PC0+i)));		// Force port to ground (discharge capacitor)
		_delay_us(SETUPDELAY);	
		DDRC &= ~((1<<(PC0+i)));	// Put back port in read mode

		TCNT1=0;					// Reset timer0 value
		TIFR1|=(1<<TOV1);			// Clear overflow flag

		while((!(PINC&(1<<(PC0+i))))&&!(TIFR1&(1<<TOV1)));	// Wait until pin comes back up. This will correspond to the t=RC 
															// where R is the value of the POT, thus the position.
															// or if overflow flow trigged.
		
		channel[i]=TCNT1;	// Give channel the timer read.
		
		// If disconnected, center paddle
		if(TIFR1&(1<<TOV1))
			channel[i]=127*DIVIDER;
	}
}

static char atariPaddlesChanged(char id)
{
	return ((button_state != button_reported_state)||(old_channel[0] != channel[0])||(old_channel[1] != channel[1]));		
}

#define REPORT_SIZE 3

static char atariPaddlesBuildReport(unsigned char *reportBuffer, char id)
{
	int x,y;
	unsigned char tmp;

	if (reportBuffer)
	{
		// Calculate the channel value relative to a char (0-255)
		x=((channel[0])/DIVIDER);
		y=((channel[1])/DIVIDER);

		// Clipping
		if(x>255)
			x=255;
		else if(x<0)
			x=0;

		if(y>255)
			y=255;
		else if(y<0)
			y=0;

		tmp=~button_state;

		reportBuffer[0]=255-(char)x; // Invert value
		reportBuffer[1]=255-(char)y; // Invert value

		reportBuffer[2] = 0;
		if (tmp&(1<<PB2)) reportBuffer[2] |= 0x01;	
		if (tmp&(1<<PB3)) reportBuffer[2] |= 0x02;
	}

	button_reported_state=button_state;
	old_channel[0]=channel[0];
	old_channel[1]=channel[1];

	return REPORT_SIZE;
}

const char atariPaddles_usbHidReportDescriptor[] PROGMEM = {

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
    0x29, 0x02,                    //     USAGE_MAXIMUM (Button 2)
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

const char atariPaddles_usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
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

Gamepad atariPaddlesJoy = {
	.num_reports			=	1,
	.reportDescriptorSize	=	sizeof(atariPaddles_usbHidReportDescriptor),
	.deviceDescriptorSize	=	sizeof(atariPaddles_usbDescrDevice),
	.init					=	atariPaddlesInit,
	.update					=	atariPaddlesUpdate,
	.changed				=	atariPaddlesChanged,
	.buildReport			=	atariPaddlesBuildReport,
};

Gamepad *atariPaddlesGetGamepad(void)
{
	atariPaddlesJoy.reportDescriptor = (void*)atariPaddles_usbHidReportDescriptor;
	atariPaddlesJoy.deviceDescriptor = (void*)atariPaddles_usbDescrDevice;

	return &atariPaddlesJoy;
}

