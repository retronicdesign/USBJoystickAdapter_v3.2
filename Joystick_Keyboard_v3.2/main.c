/* Atari, C64, Amiga Joystick interprets as keyboard keypad directions to USB
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

const char usbHidReportDescriptor[] PROGMEM = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)(Key Codes)
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)(224)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)(231)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs) ; Modifier byte
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs) ; Reserved byte
    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
    0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs) ; LED report
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x03,                    //   REPORT_SIZE (3)
    0x91, 0x03,                    //   OUTPUT (Cnst,Var,Abs) ; LED report padding
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)(Key Codes)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))(0)
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)(101)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
	0x09, 0x00,                    //   USAGE (Undefined) // Used to trig bootloader when SET FEATURE
	0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
	0x75, 0x08,                    //   REPORT_SIZE (8)
	0x95, 0x01,                    //   REPORT_COUNT (1)
	0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
    0xc0                           // END_COLLECTION
};

typedef struct {
	uint8_t modifier;
	uint8_t reserved;
	uint8_t keycode[6];
} keyboard_report_t;

static keyboard_report_t keyboard_report; // sent to PC
volatile static uchar LED_state = 0xff; // received from PC
static uchar idleRate; // repeat rate for keyboards

usbMsgLen_t usbFunctionSetup(uchar data[8]) {
    usbRequest_t *rq = (void *)data;

    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {
        switch(rq->bRequest) {
        case USBRQ_HID_GET_REPORT: // send "no keys pressed" if asked here
            // wValue: ReportType (highbyte), ReportID (lowbyte)
            usbMsgPtr = (usbMsgPtr_t)&keyboard_report; // we only have this one
            keyboard_report.modifier = 0;
            keyboard_report.keycode[0] = 0;
            return sizeof(keyboard_report);
		case USBRQ_HID_SET_REPORT: // if wLength == 1, should be LED state
            return (rq->wLength.word == 1) ? USB_NO_MSG : 0;
        case USBRQ_HID_GET_IDLE: // send idle rate to PC as required by spec
            usbMsgPtr = (usbMsgPtr_t)&idleRate;
            return 1;
        case USBRQ_HID_SET_IDLE: // save idle rate as required by spec
            idleRate = rq->wValue.bytes[1];
            return 0;
        }
    }
    
    return 0; // by default don't return any data
}

usbMsgLen_t usbFunctionWrite(uint8_t * data, uchar len) {
	if (data[0] == LED_state)
        return 1;
    else if(data[0]==0x5A)
		jumptobootloader=1;
	else
        LED_state = data[0];
	
	return 1; // Data read, not expecting more
}

#define KEY_KP1 0x59 // Keypad 1 and End
#define KEY_KP2 0x5a // Keypad 2 and Down Arrow
#define KEY_KP3 0x5b // Keypad 3 and PageDn
#define KEY_KP4 0x5c // Keypad 4 and Left Arrow
#define KEY_KP5 0x5d // Keypad 5
#define KEY_KP6 0x5e // Keypad 6 and Right Arrow
#define KEY_KP7 0x5f // Keypad 7 and Home
#define KEY_KP8 0x60 // Keypad 8 and Up Arrow
#define KEY_KP9 0x61 // Keypad 9 and Page Up
#define KEY_KP0 0x62 // Keypad 0 and Insert

/*
JoyBuf
R	L	D	U
3	2	1	0
0	0	0	0	=	0x00 = 0x00 (Center or nothing pressed)
0	0	0	1	=	0x01 = 0x60 (Up, KP8)
0	0	1	0	=	0x02 = 0x5A (Down, KP2)
0	0	1	1	=	0x03 = 0xFF (Impossible)
0	1	0	0	=	0x04 = 0x5C (Left, KP4)
0	1	0	1	=	0x05 = 0x5F (Up-Left, KP7, Home)
0	1	1	0	=	0x06 = 0x59 (Down-Left, KP1, End)
0	1	1	1	=	0x07 = 0xFF (impossible)
1	0	0	0	=	0x08 = 0x5E (Right, KP6)
1	0	0	1	=	0x09 = 0x61 (Up-Right, KP9, PgUp)
1	0	1	0	=	0x0A = 0x5B (Down-Right, KP3, PgDown)
x	x	x	x	= 0xFF (Impossible)

4 = button	0	1	= 0x0B = 0x5D (KP5)		
*/

// Keymap is located at 0x6E00 in flash memory
uchar key_map_flash[]  __attribute__((used, section(".keymap"))) = {0,KEY_KP8,KEY_KP2,0xff,KEY_KP4,KEY_KP7,KEY_KP1,0xff,KEY_KP6,KEY_KP9,KEY_KP3,KEY_KP5};

int main() {
	uchar i;
	uchar joybuf,reported_joybuf;
	uchar key_map[sizeof(key_map_flash)];

	/* PB0   = PIN1 = UP 	(I,1)
	 * PB1   = PIN2 = DOWN	(I,1)
	 * PB2   = PIN3 = LEFT  (I,1)
	 * PB3   = PIN4 = RIGHT	(I,1)
	 * PC1&3 = PIN5 = nc	(I,0)
	 * PB4   = PIN6 = BUT1  (I,1)
	 * PB5   = PIN7 = VCC 	(O,1)
	 * PD7   = PIN8 = GND	(O,0)
	 * PC0&2 = PIN9 = nc	(I,0)
	 */
	
	DDRB &= ~((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4));
	DDRB |= (1<<PB5);
	DDRD |= (1<<PD7);

	PORTB |= ((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5));
	PORTD &= ~(1<<PD7);
	
	jumptobootloader=0;
	
	reported_joybuf=0;
	joybuf=((~PINB)&0x1F);
	
    for(i=0; i<sizeof(keyboard_report); i++) // clear report initially
        ((uchar *)&keyboard_report)[i] = 0;
		
	for(i=0;i<sizeof(key_map_flash);i++) // Copy keymap in flash into ram
		((uchar *)&key_map)[i] = pgm_read_byte(key_map_flash+i);
    
    wdt_enable(WDTO_1S); // enable 1s watchdog timer

    usbInit();
	
    usbDeviceDisconnect(); // enforce re-enumeration
    for(i = 0; i<250; i++) { // wait 500 ms
        wdt_reset(); // keep the watchdog happy
        _delay_ms(2);
    }
    usbDeviceConnect();
	
    TCCR0B |= (1 << CS01); // timer 0 at clk/8 will generate randomness
    
    sei(); // Enable interrupts after re-enumeration
	
    while(1) {
        wdt_reset(); // keep the watchdog happy
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
        	
		joybuf=((~PINB)&0x1F);
		
		if(usbInterruptIsReady() && joybuf!=reported_joybuf)
		{
			//Joystick part
			if((joybuf&0x0f)!=(reported_joybuf&0x0f))
			{
				if((key_map[(joybuf&0x0f)]!=0xff)&&((joybuf&0x0f)<0x0b))
					keyboard_report.keycode[0]=key_map[joybuf&0x0f];
			}
			//Button part
			if((joybuf&0x10)!=(reported_joybuf&0x10))
			{
				if(joybuf&0x10)
					keyboard_report.keycode[1]=key_map[0x0b];
				else
					keyboard_report.keycode[1]=0;
			}
			
			keyboard_report.modifier = 0;
			usbSetInterrupt((void *)&keyboard_report, sizeof(keyboard_report));

			reported_joybuf=joybuf;
		}
    }
	
    return 0;
}
