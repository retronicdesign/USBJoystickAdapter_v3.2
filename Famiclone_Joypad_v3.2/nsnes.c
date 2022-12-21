/* Famiclone joypad (DB9 NES) to USB
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
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "gamepad.h"
#include "usbconfig.h"
#include "nsnes.h"

	/* PIN1 = PB0 = nc (I,0)
	 * PIN2 = PB1 = DATA (I,0)
	 * PIN3 = PB2 = LATCH/STOBE (O,0)
	 * PIN4 = PB3 = CLOCK (O,0)
	 * PIN5 = PC1&3 = nc (I,0)
	 * PIN6 = PB4 = VCC (O,1)
	 * PIN7 = PB5 = nc  (I,0)
	 * PIN8 = PD7 = GND (O,0)
	 * PIN9 = PC0&2 = nc (I,0)
	 */

/******** IO port definitions **************/
/* STOBE/LATCH = PIN3 = PB2 */
#define SNES_LATCH_DDR	DDRB
#define SNES_LATCH_PORT	PORTB
#define SNES_LATCH_BIT	(1<<PB2)

/* CLOCK = PIN4 = PB3 */
#define SNES_CLOCK_DDR	DDRB
#define SNES_CLOCK_PORT	PORTB
#define SNES_CLOCK_BIT	(1<<PB3)

/* DATA0 = PIN2 = PB1 */
#define SNES_DATA_PORT	PORTB
#define SNES_DATA_DDR	DDRB
#define SNES_DATA_PIN	PINB
#define SNES_DATA_BIT	(1<<PB1)	/* controller 1 */

/********* IO port manipulation macros **********/
#define SNES_LATCH_LOW()	do { SNES_LATCH_PORT &= ~(SNES_LATCH_BIT); } while(0)
#define SNES_LATCH_HIGH()	do { SNES_LATCH_PORT |= SNES_LATCH_BIT; } while(0)
#define SNES_CLOCK_LOW()	do { SNES_CLOCK_PORT &= ~(SNES_CLOCK_BIT); } while(0)
#define SNES_CLOCK_HIGH()	do { SNES_CLOCK_PORT |= SNES_CLOCK_BIT; } while(0)

#define SNES_GET_DATA()	(SNES_DATA_PIN & SNES_DATA_BIT)

/*********** prototypes *************/
static char nsnesInit(void);
static void nsnesUpdate(void);
static char nsnesChanged(char report_id);
static char nsnesBuildReport(unsigned char *reportBuffer, char id);

// the most recent bytes we fetched from the controller
static unsigned int last_update_state=0;

// the most recently reported bytes
static unsigned int last_reported_state=0;

static char nsnesInit(void)
{
	// clock and latch as output
	SNES_LATCH_DDR |= SNES_LATCH_BIT;
	SNES_CLOCK_DDR |= SNES_CLOCK_BIT;
	
	// data as input
	SNES_DATA_DDR &= ~(SNES_DATA_BIT);
	// enable pullup. This should prevent random toggling of pins
	// when no controller is connected.
	SNES_DATA_PORT |= (SNES_DATA_BIT);

	// clock is normally high
	SNES_CLOCK_PORT |= SNES_CLOCK_BIT;

	// LATCH is Active HIGH
	SNES_LATCH_PORT &= ~(SNES_LATCH_BIT);
	
	return 0;
}

/*
       Clock Cycle     Button Reported
        ===========     ===============
        0               B
        1               Y
        2               Select
        3               Start
        4               Up on joypad
        5               Down on joypad
        6               Left on joypad
        7               Right on joypad
        8               A
        9               X
        10              L
        11              R
        12              none (always high)
        13              none (always high)
        14              none (always high)
        15              none (always high)
 
*/

static void nsnesUpdate(void)
{
	int i;
	unsigned int tmp=0;

	SNES_LATCH_HIGH();
	_delay_us(12);
	SNES_LATCH_LOW();

	for (i=0; i<16; i++)
	{
		_delay_us(6);
		SNES_CLOCK_LOW();
		
		if (!SNES_GET_DATA()) { tmp |= (1<<i); }

		_delay_us(6);
		SNES_CLOCK_HIGH();
	}
	last_update_state = tmp;
}

static char nsnesChanged(char id)
{
	return (last_update_state != last_reported_state);
}

#define REPORT_SIZE 3

static char nsnesBuildReport(unsigned char *reportBuffer, char id)
{
	int x,y;
	unsigned int tmp;
	
	if (reportBuffer)
	{
		y = x = 0x80;

		tmp = last_update_state;// ^ 0xffff;
		
		if (tmp&(1<<4)) { y = 0x00; }//Up
		if (tmp&(1<<5)) { y = 0xff; }//Down
		if (tmp&(1<<6)) { x = 0x00; }//Left
		if (tmp&(1<<7)) { x = 0xff; }//Right

		reportBuffer[0] = x;
		reportBuffer[1] = y;
		reportBuffer[2] = (tmp&0x0F);
	}
	last_reported_state = last_update_state;

	return REPORT_SIZE;
}

const char nsnes_usbHidReportDescriptor[] PROGMEM = {

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
    0x29, 0x04,                    //     USAGE_MAXIMUM (Button 8)
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

const char nsnes_usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
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

Gamepad nsnesGamepad = {
	.num_reports			= 1,
	.reportDescriptorSize 	= sizeof(nsnes_usbHidReportDescriptor),
	.deviceDescriptorSize 	= sizeof(nsnes_usbDescrDevice),
	.init					= nsnesInit,
	.update					= nsnesUpdate,
	.changed				= nsnesChanged,
	.buildReport			= nsnesBuildReport
};

Gamepad *nsnesGetGamepad(void)
{
	nsnesGamepad.reportDescriptor = (void*)nsnes_usbHidReportDescriptor;
	nsnesGamepad.deviceDescriptor = (void*)nsnes_usbDescrDevice;

	return &nsnesGamepad;
}

