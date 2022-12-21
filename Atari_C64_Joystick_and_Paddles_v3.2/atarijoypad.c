/* "Atari" joystick and paddles to USB
 * Copyright (C) 2021 Francis-Olivier Gradel, B.Eng.
 *
 * Automatically detects if a joystick or paddles are connected to the adapter.
 * If paddles are connected, X axis is paddle 1, Y axis is paddle 2.
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
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <string.h>
#include "usbconfig.h"
#include "atarijoypad.h"

#define SETUPDELAY 50	// Time to reset the capacitor back to GND
#define DIVIDER 1		// Divider of the read value to match with 0-255 (Atari Paddles 1Mohm)
//#define DIVIDER 0	// Divider of the read value to match with 0-255 (C64 Paddles 460Kohm)

static char atariJoyPadInit(void);
static void atariJoyPadUpdate(void);
static char atariJoyPadChanged(char id);
static char atariJoyPadBuildReport(unsigned char *reportBuffer, char id);

volatile unsigned int channel[2];
volatile unsigned int old_channel[2];
volatile unsigned char current_channel;

volatile unsigned char last_update_state;
volatile unsigned char last_reported_state;

static char atariJoyPadInit(void)
{
	/* PB0   = PIN1 = UP 	   (I,1)
	 * PB1   = PIN2 = DOWN	   (I,1)
	 * PB2   = PIN3 = LEFT  + PADDLE BUT0 (I,1)
	 * PB3   = PIN4 = RIGHT + PADDLE BUT1 (I,1)
	 * PC1&3 = PIN5 = POT1     (I,0)
	 * PB4   = PIN6 = JOY BUT  (I,1)
	 * PB5   = PIN7 = VCC 	   (O,1)
	 * PD7   = PIN8 = GND	   (O,0)
	 * PC0&2 = PIN9 = POT0     (I,0)
	 */
	
	DDRB &= ~((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4));
	DDRB |= (1<<PB5);
	PORTB |= ((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5));

	DDRD |= ((1<<PD7));
	PORTD &= ~(1<<PD7);

	DDRC &= ~((1<<PC2)|(1<<PC3));
	PORTC &= ~((1<<PC2)|(1<<PC3));

	DDRC &= ~((1<<PC0)|(1<<PC1));
	PORTC &= ~((1<<PC0)|(1<<PC1));
	
	ADCSRB |= (1<<ACME);	// Comparator negative input on ADC MUX.
	ADCSRA &= ~(1<<ADEN);	// ADC off, Comparator on.
	ACSR |= ((1<<ACBG)|(1<<ACIC)|(1<<ACIS1)); // Comparator positive input on BANDGAP. 
	
	TCCR1B |= ((1<<CS12));// CPU/256 @ 12MHz = 46.875khz (21.33uS/bit)

	old_channel[0]=channel[0]=0;
	old_channel[1]=channel[1]=0;
	current_channel=0;

	last_update_state=last_reported_state=0;

	return 0;
}

ISR(ANALOG_COMP_vect)
{
	channel[current_channel]=ICR1;		// Put triggered timer value in corresponding channel value
	ACSR &= ~(1<<ACIE); // Interrupt disable on comparator
}

static void atariJoyPadUpdate(void)
{
	// Read buttons
	last_update_state = ((PINB&0x1F));
	
	//Update Pots
	current_channel=(current_channel+1)%2;
	ADMUX = current_channel;
	DDRC |= (1<<(PC0+current_channel)); // Force ground
	_delay_us(SETUPDELAY);
	DDRC &= ~(1<<(PC0+current_channel)); // Release ground
	TCNT1=0; // Reset timer1	
	ACSR |= (1<<ACI); // Clear Interrupt Flag. This is needed when paddles are disconnected
	ACSR |= ((1<<ACIE)); // Interrupt enable on comparator and clear flag
}

static char atariJoyPadChanged(char id)
{
	return ((last_update_state != last_reported_state)||(old_channel[0] != channel[0])||(old_channel[1] != channel[1]));;
}

#define REPORT_SIZE 3

static char atariJoyPadBuildReport(unsigned char *reportBuffer, char id)
{
	int x,y;
	unsigned char tmp;
	
	if (reportBuffer)
	{
		tmp = last_update_state ^ 0xff; // All buttons cleared
		
		if(channel[0]>0x300||channel[1]>0x300) // Paddles out of range so joystick connected
		{
			y = x = 0x80;
		
			if (tmp&(1<<PB3)) { x = 0xff; }
			if (tmp&(1<<PB2)) { x = 0x00; }
			if (tmp&(1<<PB1)) { y = 0xff; }
			if (tmp&(1<<PB0)) { y = 0x00; }
		
			reportBuffer[0] = x;
			reportBuffer[1] = y;

			reportBuffer[2] = 0;
			if (tmp&(1<<PB4)) reportBuffer[2] |= 0x01;						
		}
		else // Paddles connected
		{
				// Calculate the channel value relative to a char (0-255)
				x=((channel[0])>>DIVIDER);
				y=((channel[1])>>DIVIDER);

				// Clipping
				if(x>255)
				x=255;

				if(y>255)
				y=255;

				reportBuffer[0] = 255-x;
				reportBuffer[1] = 255-y;
					
				// Buttons
				reportBuffer[2] = 0;
				if (tmp&(1<<PB2)) reportBuffer[2] |= 0x01;
				if (tmp&(1<<PB3)) reportBuffer[2] |= 0x02;
		}
	}

	last_reported_state = last_update_state;
	
	old_channel[0]=channel[0];
	old_channel[1]=channel[1];

	return REPORT_SIZE;
}

const char atariJoyPad_usbHidReportDescriptor[] PROGMEM = {

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

const char atariJoyPad_usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
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

Gamepad atariJoyPadJoy = {
	.num_reports			=	1,
	.reportDescriptorSize	=	sizeof(atariJoyPad_usbHidReportDescriptor),
	.deviceDescriptorSize	=	sizeof(atariJoyPad_usbDescrDevice),
	.init					=	atariJoyPadInit,
	.update					=	atariJoyPadUpdate,
	.changed				=	atariJoyPadChanged,
	.buildReport			=	atariJoyPadBuildReport,
};

Gamepad *atariJoyPadGetGamepad(void)
{
	atariJoyPadJoy.reportDescriptor = (void*)atariJoyPad_usbHidReportDescriptor;
	atariJoyPadJoy.deviceDescriptor = (void*)atariJoyPad_usbDescrDevice;

	return &atariJoyPadJoy;
}


