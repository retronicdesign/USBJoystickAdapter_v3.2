/* Atari CX22 Trackball to USB (interrupt version)
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
#include <avr/wdt.h>
#include <avr/interrupt.h>  /* for sei() */
#include <util/delay.h>     /* for _delay_ms() */
#include <avr/pgmspace.h>   /* required by usbdrv.h */
#include "usbdrv.h"

#include "../bootloader/fuses.h"
#include "../bootloader/bootloader.h"

unsigned char jumptobootloader;

static void AtariC22TrackballInit(void);
static void UpdateReportBuffer(void);

static unsigned char mouse;
static unsigned char old_mouse;
static char mouse_dx;
static char mouse_dy;

/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

PROGMEM const char usbHidReportDescriptor[66] = { /* USB report descriptor, size must match usbconfig.h */
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xA1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM
    0x29, 0x03,                    //     USAGE_MAXIMUM
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x05,                    //     REPORT_SIZE (5)
    0x81, 0x03,                    //     INPUT (Const,Var,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x09, 0x38,                    //     USAGE (Wheel)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7F,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)
	0x09, 0x00,                    //     USAGE (Undefined) // Used to trig bootloader when SET FEATURE
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //     LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0xb2, 0x02, 0x01,              //     FEATURE (Data,Var,Abs,Buf)	
    0xC0,                          //   END_COLLECTION
    0xC0,                          // END COLLECTION
};
/* This is the same report descriptor as seen in a Logitech mouse. The data
 * described by this descriptor consists of 4 bytes:
 *      .  .  .  .  . B2 B1 B0 .... one byte with mouse button states
 *     X7 X6 X5 X4 X3 X2 X1 X0 .... 8 bit signed relative coordinate x
 *     Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 .... 8 bit signed relative coordinate y
 *     W7 W6 W5 W4 W3 W2 W1 W0 .... 8 bit signed relative coordinate wheel
 */
typedef struct{
    uchar   buttonMask;
    char    dx;
    char    dy;
    char    dWheel;
}report_t;

static report_t reportBuffer;
static uchar    idleRate;   /* repeat rate for keyboards, never used for mice */

/* ------------------------------------------------------------------------- */

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;

    /* The following requests are never used. But since they are required by
     * the specification, we implement them in this example.
     */
    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */
        if(rq->bRequest == USBRQ_HID_GET_REPORT){  /* wValue: ReportType (highbyte), ReportID (lowbyte) */
            /* we only have one report type, so don't look at wValue */
            usbMsgPtr = (usbMsgPtr_t)&reportBuffer;
            return sizeof(reportBuffer);
		}else if(rq->bRequest == USBRQ_HID_SET_REPORT){  
			return USB_NO_MSG;  /* use usbFunctionWrite() to receive data from host */
        }else if(rq->bRequest == USBRQ_HID_GET_IDLE){
            usbMsgPtr = (usbMsgPtr_t)&idleRate;
            return 1;
        }else if(rq->bRequest == USBRQ_HID_SET_IDLE){
            idleRate = rq->wValue.bytes[1];
        }
    }
	
    return 0;   /* default for not implemented requests: return no data back to host */
}

uchar   usbFunctionWrite(uchar *data, uchar len)
{
	if(data[0]==0x5A)
		jumptobootloader=1;
	return len;
}

/* ------------------------------------------------------------------------- */

__attribute__ ((OS_main)) int main(void)
{
    wdt_enable(WDTO_2S);
    /* Even if you don't use the watchdog, turn it off here. On newer devices,
     * the status of the watchdog (on/off, period) is PRESERVED OVER RESET!
     */
    /* RESET status: all port bits are inputs without pull-up.
     * That's the way we need D+ and D-. Therefore we don't need any
     * additional hardware initialization.
     */
	jumptobootloader=0;
	AtariC22TrackballInit();
    usbInit();
    usbDeviceDisconnect();  /* enforce re-enumeration, do this while interrupts are disabled! */
    _delay_ms(10);	// 10ms is enough to see the USB disconnection and reconnection
    usbDeviceConnect();
    sei();
    for(;;){                /* main event loop */
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
        usbPoll();
        if(usbInterruptIsReady()){
            /* called after every poll of the interrupt endpoint */
		   UpdateReportBuffer();
            usbSetInterrupt((void *)&reportBuffer, sizeof(reportBuffer));
        }
    }
}

/* ------------------------------------------------------------------------- */

static void AtariC22TrackballInit(void)
{

#define MOUSE_Xdir	PB0
#define MOUSE_Xmov 	PB1
#define MOUSE_Ydir	PB2
#define MOUSE_Ymov	PB3
#define MOUSE_BUT1	PB4

	/* PIN1 = PB0 = Xdir
	 * PIN2 = PB1 = Xmov
	 * PIN3 = PB2 = Ydir
	 * PIN4 = PB3 = Ymox
	 * PIN5 = PC3 = na
	 * PIN6 = PB4 = BUT1
	 * PIN7 = PB5 = VCC
	 * PIN8 = PB7 = GND
	 * PIN9 = PC2 = na
	 */
	
	DDRB &= ~((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4));
	PORTB |= ((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4));

	DDRC &= ~((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));
	PORTC &= ~((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));

	DDRB |= (1<<PB5);
	PORTB |= (1<<PB5);

	DDRD |= (1<<PD7);
	PORTD &= ~(1<<PD7);
	
	PCICR |= (1<<PCIE0); // Enable interrupts on PCINT0:7 (PB0-PB7)
	PCMSK0 |= ((1<<PCINT1)|(1<<PCINT3)|(1<<PCINT4)); // Enable interrupts on PB1,PB3,PB4, Xmov,Ymov and button change

	mouse_dx = mouse_dy = 0;	// Not moving
	
	old_mouse = mouse = ~PINB;	// Initial read
}

ISR(PCINT0_vect) // Trigged whenever a bit change on trackball displacement
{
	mouse = ~PINB;	// Read port

	// Falling edge of state changing Xmov pin, update mouse movement accordingly
	if((mouse & (1<<MOUSE_Xmov))!=(old_mouse & (1<<MOUSE_Xmov))&&(!(mouse & (1<<MOUSE_Xmov))))
		mouse_dx+=(MOUSE_Xdir?1:-1);
	
	// Falling edge of state changing Ymov pin, update mouse movement accordingly
	if((mouse & (1<<MOUSE_Ymov))!=(old_mouse & (1<<MOUSE_Ymov))&&(!(mouse & (1<<MOUSE_Ymov))))
		mouse_dy+=(MOUSE_Ydir?1:-1);

	old_mouse = mouse;	// Keep previous value of the port for quadrature calculation.
}

static void UpdateReportBuffer(void)
{
	// Send up to date delta displacements that happened during the USB polling interval.
	reportBuffer.dx = mouse_dx;
	mouse_dx=0;

	reportBuffer.dy = mouse_dy;
	mouse_dy=0;

	// Button Format (3 bits): MSB BUT3 BUT2 BUT1 LSB (Only one button here)
	reportBuffer.buttonMask = ((mouse&(1<<MOUSE_BUT1))>>4);	// Update Button status
}
