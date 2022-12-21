/* "Macintosh" / Apple 2 mouse with gender changer to USB (interrupt version)
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

static void MacMouseInit(void);
static void UpdateReportBuffer(void);

static unsigned char mouse;
static unsigned char old_mouse;
static char mouse_dx;
static char mouse_dy;
static int quad_x, quad_y;

char QEM [16] = {0,1,-1,2,-1,0,2,1,1,2,0,-1,2,-1,1,0};               // Quadrature Encoder Matrix
/* QEM explanation:
 *
 * Quadrature from an Mac mouse is made of two 90 degree out of phase signals that corresponds to
 * two perforated wheels driven by the mouse ball. The perforated weels are hiding or showing IR LED
 * to IR detectors on the other side that generate these signals. There are two pairs of these 
 * signals, two for the horizontal and two for the vertical.
 *
 * Here is an example of a signal of a mouse going up:
 *          ________            ________            ________            ____
 *         /        \          /        \          /        \          /
 * V  ____/          \________/          \________/          \________/
 *              ________            ________            ________
 *             /        \          /        \          /        \
 * VQ ________/          \________/          \________/          \__________
 *
 * Here is an example of a signal of a mouse going down:
 *
 *          ________            ________            ________            ____
 *         /        \          /        \          /        \          /
 * V  ____/          \________/          \________/          \________/
 * 
 * VQ ________            ________            ________            ________
 *            \          /        \          /        \          /
 *             \________/          \________/          \________/
 *
 * Note on these two example the diffence in phase between V and VQ for up and down.
 *
 * Using these generated waves, we can determine by software the delta displacement of the mouse.
 * The following table is generated by combining these signals in two 2-bit value, V, VQ, V' and VQ'.
 *
 *        Actual readed value (V-VQ 2-bit combinasion)
 *        0   1   2   3
 *     ----------------
 *   0 |  0   1  -1   X
 *   
 *   1 | -1   0   X   1
 *
 *   2 |  1   X   0  -1
 *  
 *   3 |  X  -1   1   0
 *
 *   Previous readed value (V-VQ 2-bit combinasion)
 *
 * Note that this can be done for H-HQ in the exact same way.
 */

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
	MacMouseInit();
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

static void MacMouseInit(void)
{

#define MOUSE_V		2
#define MOUSE_H 	4
#define MOUSE_VQ	7
#define MOUSE_HQ	3
#define MOUSE_BUT	6

	/* PB0   = PIN1 = GND (O,0)
	 * PB1   = PIN2 = VCC (O,1)
	 * PB2   = PIN3 = GND (O,0)
	 * PB3   = PIN4 = H   (I,1)
	 * PC1&3 = PIN5 = HQ  (I,1)
	 * PB4   = PIN6 = nc  (I,0)
	 * PB5   = PIN7 = BUT (I,1)
	 * PD7   = PIN8 = VQ  (I,1)
	 * PC0&2 = PIN9 = V   (I,1)
	 */

	DDRB &= ~((1<<PB3)|(1<<PB4)|(1<<PB5));
	DDRB |= ((1<<PB0)|(1<<PB1)|(1<<PB2));
	PORTB &= ~((1<<PB0)|(1<<PB2)|(1<<PB4));
	PORTB |= ((1<<PB1)|(1<<PB3)|(1<<PB5));

	DDRC &= ~((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));
	PORTC |= ((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));

	DDRD &= ~(1<<PD7);
	PORTD |= (1<<PD7);

	PCICR |= ((1<<PCIE0)|(1<<PCIE1)|(1<<PCIE2)); // Enable interrupts on PCINT0,1 and 2
	PCMSK0 |= ((1<<PCINT3)|(1<<PCINT5)); // Enable interrupts on PB3 and PB5 change
	PCMSK1 |= ((1<<PCINT10)|(1<<PCINT11)); // Enable interrupts on PC2 and PC3 change
	PCMSK2 |= (1<<PCINT23); // Enable interrupts on PD7 change

	mouse_dx = mouse_dy = 0;	// Not moving
	
	//MSB VQ BUT x H HQ V x x LSB
	old_mouse = mouse = (((~PINB)&0x28)<<1) | ((~PINC)&0x0C) | ((~PIND)&0x80);	// Initial read
}

ISR(PCINT1_vect,ISR_ALIASOF(PCINT0_vect));
ISR(PCINT2_vect,ISR_ALIASOF(PCINT0_vect));

ISR(PCINT0_vect) // Trigged whenever a bit change on mouse reading
{
	mouse = (((~PINB)&0x28)<<1) | ((~PINC)&0x0C) | ((~PIND)&0x80);	// Read port

	// Apply delta displacement from quadrature generated by the mouse, in x and y.
	// Quad Format (4 bits): MSB OldHQ OldH ActualHQ ActualH LSB
	quad_x=((mouse&(1<<MOUSE_H))?1:0)|((mouse&(1<<MOUSE_HQ))?2:0)|((old_mouse&(1<<MOUSE_H))?4:0)|((old_mouse&(1<<MOUSE_HQ))?8:0);
	mouse_dx += QEM[quad_x];

	// Quad Format (4 bits): MSB OldVQ OldV ActualVQ ActualV LSB
	quad_y=((mouse&(1<<MOUSE_V))?1:0)|((mouse&(1<<MOUSE_VQ))?2:0)|((old_mouse&(1<<MOUSE_V))?4:0)|((old_mouse&(1<<MOUSE_VQ))?8:0);
	mouse_dy += QEM[quad_y];

	old_mouse = mouse;	// Keep previous value of the port for quadrature calculation.
}

static void UpdateReportBuffer(void)
{
	// Send up to date delta displacements that happenend during the USB polling interval.
	reportBuffer.dx = mouse_dx;
	mouse_dx=0;

	reportBuffer.dy = mouse_dy;
	mouse_dy=0;

	// Button Format (3 bits): MSB BUT3 BUT2 BUT1 LSB
	reportBuffer.buttonMask = (mouse&(1<<MOUSE_BUT))?1:0;	// Update Button status
}
