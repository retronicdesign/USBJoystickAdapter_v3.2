/*************************************************************************************************************
*    Titre:         Fichier d'en-tête pour les fuses
*    Fichier:       fuses.h
*    Date:          3/9/2013
*************************************************************************************************************/

/*************************************************************************************************************
*   DEVICE = ATMEGA8
*	FUSES = LOW = 0x9F, HIGH = 0xC0,
*	-----
*	RSTDISBL = NO
*	WDTON = NO
*	SPIEN = YES
*	EESAVE = YES
*	BOOTSZ = Boot Flash size=1024 words, Boot address 0x0C00
*	BOOTRST = YES
*	CKOPT = YES
*	BODLEVEL = 2.7V
*	BODEN = YES
*	SUT_CKSEL = EXT. Crystal/Resonator High Freq; Start-up time = 16CK + 0MS
**************************************************************************************************************/

/*************************************************************************************************************
*   DEVICE = ATMEGA328P
*	FUSES = LOW = 0xDF, HIGH = 0xD2, EXTENDED = 0xFD
*	-----
*	BODLEVEL = 2.7V
*	RSTDISBL = NO
*   DWEN = NO
*	SPIEN = YES
*	WDTON = NO
*	EESAVE = YES
*	BOOTSZ = Boot Flash size=1024 words, Boot address 0x7800
*	BOOTRST = YES
*	CKDIV8 = NO
*	CKOUT = NO
*	SUT_CKSEL = EXT. Crystal/Resonator 8.0- Mhz; Start-up time = 16CK + 0MS
**************************************************************************************************************/

/*************************************************************************************************************
*	LOCKBITS = 0xCF
*	--------
*	LB = No memory lock features enabled
*   BLB0 = No lock on SPM and LPM in application Section
*   BLB1 = LPM and SPM prohibited in Boot Section
**************************************************************************************************************/

#include <avr/io.h>
#include <avr/signature.h> 

#ifndef FUSES_H
#define FUSES_H

__fuse_t __fuse __attribute__((section (".fuse"))) = 
{
#ifdef __AVR_ATmega8__
	.low = 0x9F,
	.high = 0xC0,
#elif __AVR_ATmega328P__
	.low = 0xDF,
	.high = 0xD2,
	.extended = 0xFD
#endif
};

unsigned char __lock __attribute__((section (".lock"))) = 0xCF;

#endif
