/* Apple][ joystick to USB
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
 * Note: If you have quirky responses, try contact cleaner in your pot! ;o)
 */
#include <avr/io.h>
#include <util/delay.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <string.h>
#include "usbconfig.h"
#include "apple2joy.h"

#define SETUPDELAY 50	// Time to reset the capacitor back to GND
#define DIVIDER 5	// Divider of the read value to match with 0-255.

void mux(char);
void resetport(char);

static char apple2Init(void);
static void apple2Update(void);
static char apple2Changed(char id);
static char apple2BuildReport(unsigned char *reportBuffer, char id);

volatile unsigned int potx,poty;
volatile unsigned int old_potx,old_poty;

static unsigned char button_state;
static unsigned char button_reported_state;

static char apple2Init(void)
{
	/* PIN1 = PB0 = BUT1 (I,0)
	 * PIN2 = PB1 = VCC  (O,1)
	 * PIN3 = PB2 = GND  (O,0)
	 * PIN4 = PB3 = nc
	 * PIN5 = PC1 = POTX (I,0) PC3 = (I,0)
	 * PIN6 = PB4 = nc
	 * PIN7 = PB5 = BUT0 (I,0)
	 * PIN8 = PD7 = POTY (I,0) PD6 = (I,0)
	 * PIN9 = PC0 = nc
	 */
	
	DDRB &= ~((1<<PB0)|(1<<PB5));
	DDRB |= ((1<<PB1)|(1<<PB2));
	PORTB |= ((1<<PB1));
	PORTB &= ~((1<<PB0)|(1<<PB2)|(1<<PB5));

	DDRD &= ~((1<<PD7)|(1<<PD6));
	PORTD &= ~((1<<PD7)|(1<<PD6));

	DDRC &= ~((1<<PC1)|(1<<PC3));
	PORTC &= ~((1<<PC1)|(1<<PC3));

	TCCR1B |= ((1<<CS10)|(1<<CS11));// CPU/64 @ 12MHz = 187,5KHz

	old_potx=potx=0;
	old_poty=poty=0;

	button_state=button_reported_state=0;

	return 0;
}

static void apple2Update(void)
{
	// Read buttons
	button_state=(PINB&((1<<PB5)|(1<<PB0)));

	potx=poty=0;
	// Read X in polling using timer1
	wdt_reset();
	DDRC |= ((1<<(PC1)));		// Force port to ground (discharge capacitor)
	_delay_us(SETUPDELAY);	
	DDRC &= ~((1<<(PC1)));	// Put back port in read mode

	TCNT1=0;					// Reset timer0 value
	TIFR1|=(1<<TOV1);			// Clear overflow flag

	while((!(PINC&(1<<(PC1))))&&!(TIFR1&(1<<TOV1)));	// Wait until pin comes back up. This will correspond to the t=RC 
														// where R is the value of the POT, thus the position.
														// or if overflow flow trigged.
		
	potx=TCNT1;	// Give channel the timer read.
	
	// If disconnected, center paddle
	if(TIFR1&(1<<TOV1))
		potx=127*DIVIDER;
	
	// Read Y in polling using timer1
	wdt_reset();
	DDRD |= ((1<<(PD6)));		// Force port to ground (discharge capacitor)
	_delay_us(SETUPDELAY);
	DDRD &= ~((1<<(PD6)));	// Put back port in read mode

	TCNT1=0;					// Reset timer0 value
	TIFR1|=(1<<TOV1);			// Clear overflow flag

	while((!(PIND&(1<<(PD6))))&&!(TIFR1&(1<<TOV1)));	// Wait until pin comes back up. This will correspond to the t=RC
														// where R is the value of the POT, thus the position.
														// or if overflow flow trigged.
	
	poty=TCNT1;	// Give channel the timer read.

	// If disconnected, center paddle
	if(TIFR1&(1<<TOV1))
		poty=127*DIVIDER;

}

static char apple2Changed(char id)
{
	return ((button_state != button_reported_state)||(old_potx != potx)||(old_poty != poty));		
}

#define REPORT_SIZE 3

static char apple2BuildReport(unsigned char *reportBuffer, char id)
{
	int x,y;
	unsigned char tmp;

	if (reportBuffer)
	{
		// Calculate the channel value relative to a char (0-255)
		x=(potx/DIVIDER);
		y=(poty/DIVIDER);

		// Clipping
		if(x>255)
			x=255;
		else if(x<0)
			x=0;

		if(y>255)
			y=255;
		else if(y<0)
			y=0;

		tmp=button_state;

		reportBuffer[0]=(char)x;
		reportBuffer[1]=(char)y;

		reportBuffer[2] = 0;
		if (tmp&(1<<PB5)) reportBuffer[2] |= 0x01;	
		if (tmp&(1<<PB0)) reportBuffer[2] |= 0x02;
	}

	button_reported_state=button_state;
	old_potx=potx;
	old_poty=poty;

	return REPORT_SIZE;
}

const char apple2_usbHidReportDescriptor[] PROGMEM = {

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

const char apple2_usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
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

Gamepad apple2Joy = {
	.num_reports			=	1,
	.reportDescriptorSize	=	sizeof(apple2_usbHidReportDescriptor),
	.deviceDescriptorSize	=	sizeof(apple2_usbDescrDevice),
	.init					=	apple2Init,
	.update					=	apple2Update,
	.changed				=	apple2Changed,
	.buildReport			=	apple2BuildReport,
};

Gamepad *apple2GetGamepad(void)
{
	apple2Joy.reportDescriptor = (void*)apple2_usbHidReportDescriptor;
	apple2Joy.deviceDescriptor = (void*)apple2_usbDescrDevice;

	return &apple2Joy;
}

