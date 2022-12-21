/* "Vectrex" joystick to USB
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
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <string.h>

#include "usbdrv/usbdrv.h"
#include "gamepad.h"

#include "vectrex.h"

#include "devdesc.h"

#include "../bootloader/fuses.h"
#include "../bootloader/bootloader.h"

static uchar *rt_usbHidReportDescriptor=NULL;
static uchar rt_usbHidReportDescriptorSize=0;
static uchar *rt_usbDeviceDescriptor=NULL;
static uchar rt_usbDeviceDescriptorSize=0;

#define MAX_REPORTS	8

char usbDescriptorConfiguration[] = { 0 }; // dummy

uchar my_usbDescriptorConfiguration[] = {    /* USB configuration descriptor */
    9,          /* sizeof(usbDescriptorConfiguration): length of descriptor in bytes */
    USBDESCR_CONFIG,    /* descriptor type */
    18 + 7 * USB_CFG_HAVE_INTRIN_ENDPOINT + 9, 0,
                /* total length of data returned (including inlined descriptors) */
    1,          /* number of interfaces in this configuration */
    1,          /* index of this configuration */
    0,          /* configuration name string index */
#if USB_CFG_IS_SELF_POWERED
    USBATTR_SELFPOWER,  /* attributes */
#else
    USBATTR_BUSPOWER,   /* attributes */
#endif
    USB_CFG_MAX_BUS_POWER/2,            /* max USB current in 2mA units */
/* interface descriptor follows inline: */
    9,          /* sizeof(usbDescrInterface): length of descriptor in bytes */
    USBDESCR_INTERFACE, /* descriptor type */
    0,          /* index of this interface */
    0,          /* alternate setting for this interface */
    USB_CFG_HAVE_INTRIN_ENDPOINT,   /* endpoints excl 0: number of endpoint descriptors to follow */
    USB_CFG_INTERFACE_CLASS,
    USB_CFG_INTERFACE_SUBCLASS,
    USB_CFG_INTERFACE_PROTOCOL,
    0,          /* string index for interface */
    9,          /* sizeof(usbDescrHID): length of descriptor in bytes */
    USBDESCR_HID,   /* descriptor type: HID */
    0x10, 0x01, /* BCD representation of HID version */
    0x21,       /* target country code */
    0x01,       /* number of HID Report (or other HID class) Descriptor infos to follow */
    0x22,       /* descriptor type: report */
    USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH, 0,  /* total length of report descriptor */
#if USB_CFG_HAVE_INTRIN_ENDPOINT    /* endpoint descriptor for endpoint 1 */
    7,          /* sizeof(usbDescrEndpoint) */
    USBDESCR_ENDPOINT,  /* descriptor type = endpoint */
    0x81,       /* IN endpoint number 1 */
    0x03,       /* attrib: Interrupt endpoint */
    8, 0,       /* maximum packet size */
    USB_CFG_INTR_POLL_INTERVAL, /* in ms */
#endif
};

static Gamepad *curGamepad;

/* ----------------------- hardware I/O abstraction ------------------------ */

static void hardwareInit(void)
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

	/* Usb pin are init as outputs */  
	DDRD |= ((1<<PD0)|(1<<PD2));   

	_delay_ms(10);	// 10ms is enough to see the USB disconnection and reconnection

	/* remove USB reset condition */
	DDRD &= ~((1<<PD0)|(1<<PD2));

	/* configure timer 0 for a rate of 12M/(1024 * 256) = 45.78 Hz (~22ms) */
	TCCR0B = (1<<CS02)|(1<<CS00);

	/* configure timer 2 for a rate of 12M/(1024 * 196) = 59.79 Hz (~16.7ms) */
	TCCR2A = (1<<WGM21);
	TCCR2B = (1<<CS22)|(1<<CS21)|(1<<CS20);
	OCR2A = 196; // for 60 hz
}

static uchar    reportBuffer[6];    /* buffer for HID reports */

/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

static uchar    idleRates[MAX_REPORTS];           /* in 4 ms units */

uchar	usbFunctionDescriptor(struct usbRequest *rq)
{
	if ((rq->bmRequestType & USBRQ_TYPE_MASK) != USBRQ_TYPE_STANDARD)
		return 0;

	if (rq->bRequest == USBRQ_GET_DESCRIPTOR)
	{
		// USB spec 9.4.3, high byte is descriptor type
		switch (rq->wValue.bytes[1])
		{
			case USBDESCR_DEVICE:
				usbMsgPtr = rt_usbDeviceDescriptor;		
				return rt_usbDeviceDescriptorSize;
			case USBDESCR_HID_REPORT:
				usbMsgPtr = rt_usbHidReportDescriptor;
				return rt_usbHidReportDescriptorSize;
			case USBDESCR_CONFIG:
				usbMsgPtr = my_usbDescriptorConfiguration;
				return sizeof(my_usbDescriptorConfiguration);
		}
	}

	return 0;
}

static uchar setupBuffer[sizeof(reportBuffer)];

uchar	usbFunctionSetup(uchar data[8])
{
	usbRequest_t    *rq = (void *)data;
	int i;

	usbMsgPtr = setupBuffer;
	
	if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */
		switch (rq->bRequest)
		{
			case USBRQ_HID_GET_REPORT:
				/* wValue: ReportType (highbyte), ReportID (lowbyte) */
				return curGamepad->buildReport(setupBuffer, rq->wValue.bytes[0]);

			case USBRQ_HID_SET_REPORT:
				return USB_NO_MSG;  /* use usbFunctionWrite() to receive data from host */

			case USBRQ_HID_GET_IDLE:
				if (rq->wValue.bytes[0] > 0 && rq->wValue.bytes[0] <= MAX_REPORTS) {
					usbMsgPtr = idleRates + (rq->wValue.bytes[0] - 1);
					return 1;
				}
				break;

			case USBRQ_HID_SET_IDLE:
				if (rq->wValue.bytes[0]==0) {
					for (i=0; i<MAX_REPORTS; i++)
						idleRates[i] = rq->wValue.bytes[1];
				}
				else {
					if (rq->wValue.bytes[0] > 0 && rq->wValue.bytes[0] <= MAX_REPORTS) {
						idleRates[rq->wValue.bytes[0]-1] = rq->wValue.bytes[1];
					}
				}
				break;
		}
	}
	return 0;
}

/* usbFunctionWrite() is called when the host sends a chunk of data to the
 * device. For more information see the documentation in usbdrv/usbdrv.h.
 */
uchar   usbFunctionWrite(uchar *data, uchar len)
{
	if(data[0]==0x5A)
		jumptobootloader=1;
    return len;
}

__attribute__ ((OS_main)) int main(void)
{
	char must_report = 0, first_run = 1;
	uchar idleCounters[MAX_REPORTS];
	int i;

	jumptobootloader=0;

	memset(idleCounters, 0, MAX_REPORTS);
	memset(idleRates, 0, MAX_REPORTS); // infinity

	curGamepad = VectrexGetGamepad();

	// configure report descriptor according to
	// the current gamepad
	rt_usbHidReportDescriptor = curGamepad->reportDescriptor;
	rt_usbHidReportDescriptorSize = curGamepad->reportDescriptorSize;

	if (curGamepad->deviceDescriptor != 0)
	{
		rt_usbDeviceDescriptor = (void*)curGamepad->deviceDescriptor;
		rt_usbDeviceDescriptorSize = curGamepad->deviceDescriptorSize;
	}
	else
	{
		// use descriptor from devdesc.c
		//
		rt_usbDeviceDescriptor = (void*)usbDescrDevice;
		rt_usbDeviceDescriptorSize = getUsbDescrDevice_size();
	}

	// patch the config descriptor with the HID report descriptor size
	my_usbDescriptorConfiguration[25] = rt_usbHidReportDescriptorSize;

	wdt_enable(WDTO_2S);
	hardwareInit();
	set_sleep_mode(SLEEP_MODE_IDLE);

	curGamepad->init();
	
	usbInit();
	sei();
	
	for(;;){	/* main event loop */
		wdt_reset();
		if(jumptobootloader)
		{
			cli(); // Clear interrupts
			/* magic boot key in memory to invoke reflashing 0x013B-0x013C = BEEF */
			unsigned int *BootKey=(unsigned int*)0x013b;
			*BootKey=0xBEEF;

			/* USB disconnect */  
			DDRD |= ((1<<PD0)|(1<<PD2));
			for(;;); // Let wdt reset the CPU
		}

		// this must be called at each 50 ms or less
		usbPoll();

		if (first_run) {
			curGamepad->update();
			first_run = 0;
		}

		/* Try to report at the granularity requested by
		 * the host */
		if(TIFR0 & (1<<TOV0))  /* 22 ms timer */
		{
			TIFR0 = 1<<TOV0;
			for (i=0; i<curGamepad->num_reports; i++) 
			{
				if(idleRates[i] != 0)
				{
					if(idleCounters[i] > 4){
						idleCounters[i] -= 5;   /* 22 ms in units of 4 ms */
					}else{
						// reset the counter and schedule a report for this
						idleCounters[i] = idleRates[i];
						must_report |= (1<<i);
					}
				}
			}
		}

		/* Read the controller periodically*/
		if ((TIFR2 & (1<<OCF2A)) )
		{
			TIFR2 = 1<<OCF2A;

			// Ok, the timer tells us it is time to update
			// the controller status. 
			//
			// But wait! USB triggers interrupts at a rate of approx. 1ms. 
			// Waiting until an interrupt has just been serviced before attempting
			// to update the controller prevents USB interrupt servicing 
			// delays from messing with the timing in the controller update 
			// function. 

			curGamepad->update();

			/* Check what will have to be reported */
			for (i=0; i<curGamepad->num_reports; i++) {
				if (curGamepad->changed(i+1)) {
					must_report |= (1<<i);
				}
			}
		}
			
		if(must_report)
		{
			for (i=0; i<curGamepad->num_reports; i++)
			{
				if ((must_report & (1<<i)) == 0)
					continue;

				if (usbInterruptIsReady())
				{
					char len;

					len = curGamepad->buildReport(reportBuffer, i+1);
					usbSetInterrupt(reportBuffer, len);

					while (!usbInterruptIsReady())
					{
						usbPoll();
						wdt_reset();
					}
				}
			}
				
			must_report = 0;

		}
	}
}

