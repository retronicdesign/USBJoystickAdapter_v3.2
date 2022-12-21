/* Name: main.c
 * Project: AVR bootloader HID
 * Author: Christian Starkjohann
 * Creation Date: 2007-03-19
 * Tabsize: 4
 * Copyright: (c) 2007 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: GNU GPL v2 (see License.txt)
 * This Revision: $Id$
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <avr/boot.h>
#include <string.h>
#include <util/delay.h>

static void leaveBootloader() __attribute__((__noreturn__));
unsigned int BootKey __attribute__ ((section (".noinit"))); // 0x013B-0x13C, magic boot key location to invoke bootloader from software

#include "bootloaderconfig.h"
#include "usbdrv.c"

/* ------------------------------------------------------------------------ */

#ifndef ulong
#   define ulong    unsigned long
#endif
#ifndef uint
#   define uint     unsigned int
#endif

#if (FLASHEND) > 0xffff /* we need long addressing */
#   define addr_t           ulong
#else
#   define addr_t           uint
#endif

static addr_t           currentAddress; /* in bytes */
static uchar            offset;         /* data already processed in current transfer */
static uchar            exitMainloop;



const PROGMEM char usbHidReportDescriptor[33] = {
    0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)

    0x85, 0x01,                    //   REPORT_ID (1)
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)

    0x85, 0x02,                    //   REPORT_ID (2)
    0x95, 0x83,                    //   REPORT_COUNT (131)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
    0xc0                           // END_COLLECTION
};

/* allow compatibility with avrusbboot's bootloaderconfig.h: */
#ifdef BOOTLOADER_INIT
#   define bootLoaderInit()         BOOTLOADER_INIT
#endif
#ifdef BOOTLOADER_CONDITION
#   define bootLoaderCondition()    BOOTLOADER_CONDITION
#endif

/* compatibility with ATMega88 and other new devices: */
#ifndef TCCR0
#define TCCR0   TCCR0B
#endif
#ifndef GICR
#define GICR    MCUCR
#endif

static void (*nullVector)(void) __attribute__((__noreturn__));

static void leaveBootloader()
{
    cli();
    boot_rww_enable();
    USB_INTR_ENABLE = 0;
    USB_INTR_CFG = 0;       /* also reset config bits */

    GICR = (1 << IVCE);     /* enable change of interrupt vectors */
    GICR = (0 << IVSEL);    /* move interrupts to application flash section */
/* We must go through a global function pointer variable instead of writing
 *  ((void (*)(void))0)();
 * because the compiler optimizes a constant 0 to "rcall 0" which is not
 * handled correctly by the assembler.
 */
    nullVector();
}

uchar   usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;
static uchar    replyBuffer[7] = {
        1,                              /* report ID */
        SPM_PAGESIZE & 0xff,
        SPM_PAGESIZE >> 8,
        ((long)FLASHEND + 1) & 0xff,
        (((long)FLASHEND + 1) >> 8) & 0xff,
        (((long)FLASHEND + 1) >> 16) & 0xff,
        (((long)FLASHEND + 1) >> 24) & 0xff
    };

    if(rq->bRequest == USBRQ_HID_SET_REPORT){
        if(rq->wValue.bytes[0] == 2){
            offset = 0;
            return USB_NO_MSG;
        }
        else{
            exitMainloop = 1;
        }
    }else if(rq->bRequest == USBRQ_HID_GET_REPORT){
        usbMsgPtr = (usbMsgPtr_t)replyBuffer;
        return 7;
    }
    return 0;
}

uchar usbFunctionWrite(uchar *data, uchar len)
{
union {
    addr_t  l;
    uint    s[sizeof(addr_t)/2];
    uchar   c[sizeof(addr_t)];
}       address;
uchar   isLast;

    address.l = currentAddress;
    if(offset == 0){
        address.c[0] = data[1];
        address.c[1] = data[2];
#if (FLASHEND) > 0xffff /* we need long addressing */
        address.c[2] = data[3];
        address.c[3] = 0;
#endif
        data += 4;
        len -= 4;
    }
    offset += len;
    isLast = offset & 0x80; /* != 0 if last block received */
    do{
        addr_t prevAddr;
#if SPM_PAGESIZE > 256
        uint pageAddr;
#else
        uchar pageAddr;
#endif
        pageAddr = address.s[0] & (SPM_PAGESIZE - 1);
        if(pageAddr == 0){              /* if page start: erase */

            cli();
            boot_page_erase(address.l); /* erase page */
            sei();
            boot_spm_busy_wait();       /* wait until page is erased */
        }
        cli();
        boot_page_fill(address.l, *(short *)data);
        sei();
        prevAddr = address.l;
        address.l += 2;
        data += 2;
        /* write page when we cross page boundary */
        pageAddr = address.s[0] & (SPM_PAGESIZE - 1);
        if(pageAddr == 0){
            cli();
            boot_page_write(prevAddr);
            sei();
            boot_spm_busy_wait();
        }
        len -= 2;
    }while(len);
    currentAddress = address.l;
    return isLast;
}

static void initForUsbConnectivity(void)
{
uchar   i = 0;


    usbInit();
    /* enforce USB re-enumerate: */
    usbDeviceDisconnect();  /* do this while interrupts are disabled */
    do{             /* fake USB disconnect for > 250 ms */
        wdt_reset();
        _delay_ms(1);
    }while(--i);
    usbDeviceConnect();
    sei();
}

int __attribute__((noreturn)) main(void)
{
	wdt_enable(WDTO_2S);
    /* initialize hardware */
    bootLoaderInit();
    /* jump to application if button is pressed or magic key present */
    if(bootLoaderCondition()||BootKey==0xBEEF){

        GICR = (1 << IVCE);  /* enable change of interrupt vectors */
        GICR = (1 << IVSEL); /* move interrupts to boot flash section */

        initForUsbConnectivity();
        for(;;){ /* main event loop */
            wdt_reset();
            usbPoll();
            if(exitMainloop){

				_delay_ms(10);
				break;
           }
        }
		/* Clear magic boot key */
		BootKey=0xFFFF;
    }
    leaveBootloader();
}

