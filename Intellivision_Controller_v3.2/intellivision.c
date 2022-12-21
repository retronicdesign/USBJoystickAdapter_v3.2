/* Intellivision Flashback ALT (AtGames 2014) controller to USB
 * Copyright (C) 2018 Francis-Olivier Gradel, ing.
 *
 * Code in "intellivision.c" modified by Joe Zbiciak 2018-10-31
 * to handle keypad combinations as raw input.
 *
 * Using V-USB code from OBJECTIVE DEVELOPMENT Software GmbH. http://www.obdev.at/products/vusb *
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
#include "intellivision.h"

static char intellivisionInit(void);
static void intellivisionUpdate(void);
static char intellivisionChanged(char id);
static char intellivisionBuildReport(unsigned char *reportBuffer, char id);

static unsigned char last_update_state=0;
static unsigned char last_reported_state=0;

static char intellivisionInit(void)
{

	/* FUNCTION (MATTEL ORIGINAL)
	 * PB0   = PIN1 IN (ROW1)
	 * PB1   = PIN2 IN (DISC2/ROW2)
	 * PB2   = PIN3 IN (ROW3)
	 * PB3   = PIN4 IN (ROW4)
	 * PC1&3 = PIN5 IN (DISC 1/GND)
	 * PB4   = PIN6 IN (COL1)
	 * PB5   = PIN7 IN (COL2)
	 * PD7   = PIN8 IN (COL3)
	 * PC0&2 = PIN9 IN (DISC BUT)
	 */

	/* FUNCTION (FLASHBACK)
	 * PB0   = PIN1 IN (ROW4)
	 * PB1   = PIN2 IN (COL1)
	 * PB2   = PIN3 OUT(DISC1/GND)
	 * PB3   = PIN4 IN (DISC2/ROW2)
	 * PC1&3 = PIN5 IN (DISC BUT)
	 * PB4   = PIN6 IN (COL3)
	 * PB5   = PIN7 IN (COL2)
	 * PD7   = PIN8 IN (ROW3)
	 * PC0&2 = PIN9 IN (ROW1)
	 */

	DDRB &= ~((1<<PB0)|(1<<PB1)|(1<<PB3)|(1<<PB4)|(1<<PB5));
	DDRB |= ((1<<PB2));
	DDRC &= ~((1<<PC1)|(1<<PC3)|(1<<PC0)|(1<<PC2));
	DDRD &= ~((1<<PD7));

	PORTB |= ((1<<PB0)|(1<<PB1)|(1<<PB3)|(1<<PB4)|(1<<PB5));
	PORTB &= ~((1<<PB2));
	PORTC |= ((1<<PC0)|(1<<PC2)|(1<<PC1)|(1<<PC3));
	PORTD |= ((1<<PD7));
	
	return 0;
}

static void intellivisionUpdate(void)
{
	/* Reorder readings to match original firmware decoding */
	last_update_state = (((PINB&(1<<PB0))<<3)|
						((PINB&(1<<PB1))<<3)|
						((PINB&(1<<PB3))>>2)|
						((PINB&(1<<PB4))<<2)|
						((PINB&(1<<PB5)))|
						((PINC&(1<<PC2))>>2)|
						((PINC&(1<<PC3))<<4)|
						((PIND&(1<<PD7))>>5));
}

static char intellivisionChanged(char id)
{
	return (last_update_state != last_reported_state);
}

#define REPORT_SIZE 5

static char intellivisionBuildReport(unsigned char *reportBuffer, char id)
{
	int x,y;
	unsigned char tmp,but[2];
	
	if (reportBuffer)
	{
		tmp = (last_update_state ^ 0xff);
		
		//Scan direction disc first
		switch(tmp&0x8F)
		{
			case 0b00000010: x = 0x80; y = 0x00; but[0]=but[1]=0; break; //D1 Disc N
			case 0b10000010: x = 0xC0; y = 0x00; but[0]=but[1]=0; break; //D2 Disc NNE
			case 0b10000110: x = 0xFF; y = 0x00; but[0]=but[1]=0; break; //D3 Disc NE 
			case 0b00000110: x = 0xFF; y = 0x40; but[0]=but[1]=0; break; //D4 Disc ENE
			case 0b00000100: x = 0xFF; y = 0x80; but[0]=but[1]=0; break; //D5 Disc E
			case 0b10000100: x = 0xFF; y = 0xC0; but[0]=but[1]=0; break; //D6 Disc ESE
			case 0b10001100: x = 0xFF; y = 0xFF; but[0]=but[1]=0; break; //D7 Disc SE
			case 0b00001100: x = 0xC0; y = 0xFF; but[0]=but[1]=0; break; //D8 Disc SSE
			case 0b00001000: x = 0x80; y = 0xFF; but[0]=but[1]=0; break; //D9 Disc S
			case 0b10001000: x = 0x40; y = 0xFF; but[0]=but[1]=0; break; //D10 Disc SSW
			case 0b10001001: x = 0x00; y = 0xFF; but[0]=but[1]=0; break; //D11 Disc SW
			case 0b00001001: x = 0x00; y = 0xC0; but[0]=but[1]=0; break; //D12 Disc WSW
			case 0b00000001: x = 0x00; y = 0x80; but[0]=but[1]=0; break; //D13 Disc W
			case 0b10000001: x = 0x00; y = 0x40; but[0]=but[1]=0; break; //D14 Disc WNW
			case 0b10000011: x = 0x00; y = 0x00; but[0]=but[1]=0; break; //D15 Disc NW
			case 0b00000011: x = 0x40; y = 0x00; but[0]=but[1]=0; break; //D16 Disc NNW

			default: y = x = 0x80; //D0 Disc centered
		}

		//Then scan action buttons (3 buttons)
		switch(tmp&0x70)
		{

			case 0b01010000:  but[1]=0b00000000; but[0]=0b00000001; break; //S1 Button 1
			case 0b01100000:  but[1]=0b00000000; but[0]=0b00000010; break; //S2 Button 2
			case 0b00110000:  but[1]=0b00000000; but[0]=0b00000100; break; //S3 Button 3

			default: but[2]=but[1]=but[0]=0; //All buttons depressed
		}

		//Finally, scan keypad (12 buttons)
		switch(tmp)
		{

			case 0b00011000: x = y = 0x80;  but[1]=0b00000000; but[0]=0b00001000; break; //K1 Keypad 1
			case 0b00101000: x = y = 0x80;  but[1]=0b00000000; but[0]=0b00010000; break; //K2 Keypad 2
			case 0b01001000: x = y = 0x80;  but[1]=0b00000000; but[0]=0b00100000; break; //K3 Keypad 3
			case 0b00010100: x = y = 0x80;  but[1]=0b00000000; but[0]=0b01000000; break; //K4 Keypad 4
			case 0b00100100: x = y = 0x80;  but[1]=0b00000000; but[0]=0b10000000; break; //K5 Keypad 5
			case 0b01000100: x = y = 0x80;  but[1]=0b00000001; but[0]=0b00000000; break; //K6 Keypad 6
			case 0b00010010: x = y = 0x80;  but[1]=0b00000010; but[0]=0b00000000; break; //K7 Keypad 7
			case 0b00100010: x = y = 0x80;  but[1]=0b00000100; but[0]=0b00000000; break; //K8 Keypad 8
			case 0b01000010: x = y = 0x80;  but[1]=0b00001000; but[0]=0b00000000; break; //K9 Keypad 9
			case 0b00010001: x = y = 0x80;  but[1]=0b00010000; but[0]=0b00000000; break; //Clear Keypad Clear
			case 0b00100001: x = y = 0x80;  but[1]=0b00100000; but[0]=0b00000000; break; //K0 Keypad 0
			case 0b01000001: x = y = 0x80;  but[1]=0b01000000; but[0]=0b00000000; break; //Enter Keypad Enter

			//Special case, keypad combinations
			case 0b01011010: x = y = 0x80;  but[1]=0b10000000; but[0]=0b00000000; break; //[K1+K9] to Btn 16 (Pause)

			default: but[1]=0; but[0] &= 0x07; //Keypad depressed	
		}

	   /*
 		* [0] X
 		* [1] Y
 		* [2] Btn 1-8
		* [3] Btn 9-16
		* [4] Raw controller input with no interpretation (8 bits)
 		*/

		reportBuffer[0] = x;
		reportBuffer[1] = y;
		reportBuffer[2] = but[0];
		reportBuffer[3] = but[1];
		reportBuffer[4] = tmp;

	}
	last_reported_state = last_update_state;

	return REPORT_SIZE;
}

const char intellivision_usbHidReportDescriptor[] PROGMEM = {

    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game Pad)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //     LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0xc0,                          // END_COLLECTION
    0x05, 0x09,                    // USAGE_PAGE (Button)
    0x19, 1,                       //   USAGE_MINIMUM (Button 1)
	0x29, 24,               	   //   USAGE_MAXIMUM (Button 24)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 1,                  	   //   REPORT_SIZE (1)
    0x95, 24,                      //   REPORT_COUNT (24)
    0x81, 0x02,                    // INPUT (Data,Var,Abs)
	0x09, 0x00,                    //     USAGE (Undefined) // Used to trig bootloader when SET FEATURE
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //     LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0xb2, 0x02, 0x01,              //     FEATURE (Data,Var,Abs,Buf)	
    0xc0                           // END_COLLECTION
};

#define USBDESCR_DEVICE         1

// This is the same descriptor as in devdesc.c, but the product id is 0x0a99 

const char intellivision_usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
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

Gamepad intellivisionJoy = {
	.num_reports			=	1,
	.reportDescriptorSize	=	sizeof(intellivision_usbHidReportDescriptor),
	.deviceDescriptorSize	=	sizeof(intellivision_usbDescrDevice),
	.init					=	intellivisionInit,
	.update					=	intellivisionUpdate,
	.changed				=	intellivisionChanged,
	.buildReport			=	intellivisionBuildReport,
};

Gamepad *intellivisionGetGamepad(void)
{
	intellivisionJoy.reportDescriptor = (void*)intellivision_usbHidReportDescriptor;
	intellivisionJoy.deviceDescriptor = (void*)intellivision_usbDescrDevice;

	return &intellivisionJoy;
}


