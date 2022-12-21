/* Coleco Gemini joystick to USB
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
 */
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <string.h>
#include "usbconfig.h"
#include "ColecoGemini.h"

#define SETUPDELAY	5
#define DIVIDER 64 // 1M pot
#define TIMEOUT	65000

static char ColecoGeminiInit(void);
static void ColecoGeminiUpdate(void);
static char ColecoGeminiChanged(char id);
static char ColecoGeminiBuildReport(unsigned char *reportBuffer, char id);
static void ColecoGeminiReadPot(void);

volatile unsigned int pot,old_pot;
volatile char flag;

static unsigned char last_update_state=0;
static unsigned char last_reported_state=0;

static char ColecoGeminiInit(void)
{

	/* PIN1 = PB0 = (I,1) UP
	 * PIN2 = PB1 = (I,1) DOWN
	 * PIN3 = PB2 = (I,1) LEFT
	 * PIN4 = PB3 = (I,1) RIGHT
	 * PIN5 = PC1 = (I,0) POT, PC3 = (I,0) nc
	 * PIN6 = PB4 = (I,1) BUTTON
	 * PIN7 = PB5 = (O,1) VCC
	 * PIN8 = PD7 = (O,0) GND
	 * PIN9 = PC0 = (I,0) nc, PC2 = (I,0) nc
	 */
	
	DDRB |= (1<<PB5);
	DDRB &= ~((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4));
	PORTB |= ((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5));

	DDRD |= (1<<PD7);
	PORTD &= ~(1<<PD7);

	DDRC &= ~((1<<PC1)|(1<<PC3)|(1<<PC0)|(1<<PC2));
	PORTC &= ~((1<<PC1)|(1<<PC3)|(1<<PC0)|(1<<PC2));

	old_pot=pot=0;

	ADCSRB |= (1<<ACME);	// Comparator negative input on ADC MUX.
	ADCSRA &= ~(1<<ADEN);	// ADC off, Comparator on.
	ACSR |= ((1<<ACBG)|(1<<ACIE)|(1<<ACIC)|(1<<ACIS1)); // Comparator positive input on BANDGAP. Interrupt enable on caparator
	ADMUX=1; // Channel 1 selected (PC1)

	TCCR1B |= ((1<<WGM12)|(1<<CS11));	// Timer1: CTC,XTAL/8

	return 0;
}

static void ColecoGeminiUpdate(void)
{
	unsigned int i=0;

	last_update_state = ((PINB&((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4))));
	ColecoGeminiReadPot();

	while(i<TIMEOUT && flag)
		i++;
}

static char ColecoGeminiChanged(char id)
{
	return ((last_update_state != last_reported_state) || (pot != old_pot) );
}

#define REPORT_SIZE 4

static char ColecoGeminiBuildReport(unsigned char *reportBuffer, char id)
{
	int x,y,z;
	unsigned char tmp;
	
	if (reportBuffer)
	{
		y = x = 0x80;

		// Re-rangeing the pot values between 0-255
		z=(pot/DIVIDER);

		// Clip maximum value to 255
		if(z>255)
			z=255;
		
		// Inverting pot values (cw to ccw)
		z=255-z;

		tmp = last_update_state ^ 0xff;

		if (tmp&(1<<PB0)) { y = 0x00; } // UP	
		if (tmp&(1<<PB1)) { y = 0xff; } // DOWN	
		if (tmp&(1<<PB2)) { x = 0x00; } // LEFT
		if (tmp&(1<<PB3)) { x = 0xff; } // RIGHT

		reportBuffer[0] = x;
		reportBuffer[1] = y;
		reportBuffer[2] = z;
		reportBuffer[3] = 0;
		if (tmp&(1<<PB4)) reportBuffer[3] |= 0b00000001; // FIRE

	}
	last_reported_state = last_update_state;

	return REPORT_SIZE;
}

const char ColecoGemini_usbHidReportDescriptor[] PROGMEM = {

	0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game Pad)
    0xa1, 0x01,                    // COLLECTION (Application)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x09, 0x32,                    //     USAGE (Z)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //     LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x03,                    //     REPORT_COUNT (3)
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

const char ColecoGemini_usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
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

Gamepad ColecoGeminiJoy = {
	.num_reports			=	1,
	.reportDescriptorSize	=	sizeof(ColecoGemini_usbHidReportDescriptor),
	.deviceDescriptorSize	=	sizeof(ColecoGemini_usbDescrDevice),
	.init					=	ColecoGeminiInit,
	.update					=	ColecoGeminiUpdate,
	.changed				=	ColecoGeminiChanged,
	.buildReport			=	ColecoGeminiBuildReport,
};

Gamepad *ColecoGeminiGetGamepad(void)
{
	ColecoGeminiJoy.reportDescriptor = (void*)ColecoGemini_usbHidReportDescriptor;
	ColecoGeminiJoy.deviceDescriptor = (void*)ColecoGemini_usbDescrDevice;

	return &ColecoGeminiJoy;
}

ISR(ANALOG_COMP_vect)
{
	pot=ICR1;		// Store triggered timer value 
	flag=0;			// Lower reading in process flag.
}

void ColecoGeminiReadPot(void)
{
	flag=1;	// Read in process flag
	DDRC |= (1<<PC1);	// Force port to ground
	_delay_us(SETUPDELAY);	// Wait for capacitor to be discharged
	DDRC &= ~(1<<PC1);	// Put back port in read mode (floating);
	TCNT1=0;// Clear counter value
}

