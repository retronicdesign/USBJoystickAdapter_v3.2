/* Vectrex controller to USB
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
 */
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "usbconfig.h"
#include "vectrex.h"

#define SETUPDELAY 100

#define AXIS_X	0
#define AXIS_Y	1

static char VectrexInit(void);
static void VectrexUpdate(void);
static char VectrexChanged(char id);
static char VectrexBuildReport(unsigned char *reportBuffer, char id);
static void VectrexReadAxis(unsigned int);

volatile unsigned char channel[2];
volatile unsigned char old_channel[2];
volatile int currentchannel;
volatile char flag;

static unsigned char button_state;
static unsigned char button_reported_state;

static char VectrexInit(void)
{
	/* PB0       = PIN1 = BUT1(I,1)
	 * PB1       = PIN2 = BUT2(I,1)
	 * PB2       = PIN3 = BUT3(I,1)
	 * PB3&PD5   = PIN4 = BUT4(I,1)
	 * PC1&PC3   = PIN5 = PC1 = nc(I,0) PC3 = PotWiper(Y)(I,0)
	 * PB4&PC4   = PIN6 = PB4 = nc(I,0) PC4 = PotWiper(X)(I,0)
	 * PB5       = PIN7 = VCC	(O,1) O1
	 * PD7&PD6   = PIN8 = PD7 = GND(O,0), PD6 = GND(PotEnd)(O,0)
	 * PC0&PC2   = PIN9 = GND (O,0) PC2 = GND (O,0)
	 */
	
	DDRB &= ~((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4));
	PORTB |= ((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3));
	PORTB &= ~((1<<PB4));
	DDRB |= (1<<PB5);
	PORTB |= (1<<PB5);

	DDRC &= ~((1<<PC1)|(1<<PC3)|(1<<PC4));
	DDRC |= ((1<<PC0)|(1<<PC2));
	PORTC &= ~((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3)|(1<<PC4));

	DDRD |= ((1<<PD6)|(1<<PD7));
	PORTD &= ~((1<<PD6)|(1<<PD7));

	old_channel[AXIS_X]=channel[AXIS_X]=0;
	old_channel[AXIS_Y]=channel[AXIS_Y]=0;

	button_state=button_reported_state=0;

	ADMUX |= ((1<<REFS0)|(1<<ADLAR));	// AREF=VCC ADLAR=1
	ADCSRA |= (1<<ADEN);	// ADC enable
	ADCSRA |= (1<<ADIF);	// Initialize for the first conversion

	return 0;
}

static void VectrexUpdate(void)
{
	button_state=~(PINB&0x0F); //Read all 4 buttons
	VectrexReadAxis(AXIS_X); // Read x axis value
	VectrexReadAxis(AXIS_Y); // Read y axis value	
}

static char VectrexChanged(char id)
{
	return ((button_state != button_reported_state)||(old_channel[AXIS_X] != channel[AXIS_X])||(old_channel[AXIS_Y] != channel[AXIS_Y]));		
}

#define REPORT_SIZE 3

static char VectrexBuildReport(unsigned char *reportBuffer, char id)
{
	if (reportBuffer)
	{
		reportBuffer[0]=button_state;
		reportBuffer[1]=channel[AXIS_X];
		reportBuffer[2]=255-channel[AXIS_Y];
	}

	button_reported_state=button_state;
	old_channel[AXIS_X]=channel[AXIS_X];
	old_channel[AXIS_Y]=channel[AXIS_Y];

	return REPORT_SIZE;
}

const char Vectrex_usbHidReportDescriptor[] PROGMEM = {

0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game Pad)
    0xa1, 0x01,                    // COLLECTION (Application)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x04,                    //     USAGE_MAXIMUM (Button 4)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x08,                    //     REPORT_COUNT (8)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //     LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
	0x09, 0x00,					   //     USAGE (Undefined) // Used to trig bootloader when SET FEATURE
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

const char Vectrex_usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
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

Gamepad VectrexJoy = {
	.num_reports			=	1,
	.reportDescriptorSize	=	sizeof(Vectrex_usbHidReportDescriptor),
	.deviceDescriptorSize	=	sizeof(Vectrex_usbDescrDevice),
	.init					=	VectrexInit,
	.update					=	VectrexUpdate,
	.changed				=	VectrexChanged,
	.buildReport			=	VectrexBuildReport,
};

Gamepad *VectrexGetGamepad(void)
{
	VectrexJoy.reportDescriptor = (void*)Vectrex_usbHidReportDescriptor;
	VectrexJoy.deviceDescriptor = (void*)Vectrex_usbDescrDevice;

	return &VectrexJoy;
}

void VectrexReadAxis(unsigned int axis)
{
	flag=1;	// Read in process flag
	currentchannel=axis; // Set current channel to be read

	if(axis==AXIS_X)
		ADMUX=3 | ((1<<REFS0)|(1<<ADLAR));
	else
		ADMUX=4 | ((1<<REFS0)|(1<<ADLAR));
				
	_delay_us(SETUPDELAY);	// Wait for level to stabilize

	ADCSRA |= (1<<ADSC);	// Initialize the conversion
	while(!(ADCSRA&(1<<ADIF)));	// Wait for conversion to complete
	ADCSRA |= (1<<ADIF); // Initialize for the next conversion
	channel[axis]=ADCH; // Read analog value
}
