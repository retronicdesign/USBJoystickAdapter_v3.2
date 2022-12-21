/*************************************************************************************************************
*    Titre:         Fichier d'en-tête pour les fuses
*    Fichier:       fuses.h
*    Date:          3/9/2013
*************************************************************************************************************/

/*************************************************************************************************************
*   DEVICE = ATMEGA328P
*	FUSES = LOW = 0xDF, HIGH = 0xC8, EXTENDED = 0xFD
*	-----
*	BODLEVEL = 2.7V
*	RSTDISBL = NO
*   DWEN = NO
*	SPIEN = YES
*	WDTON = YES
*	EESAVE = NO
*	BOOTSZ = Boot Flash size=2024 words, Boot address 0x3800
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
	.low = 0xDF,
	.high = 0xC8,
	.extended = 0xFD
};

unsigned char __lock __attribute__((section (".lock"))) = 0xCF;

#endif
