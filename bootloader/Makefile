# Name: Makefile
# Project: bootloadHID
# Author: Christian Starkjohann
# Creation Date: 2007-03-19
# Tabsize: 4
# Copyright: (c) 2007 by OBJECTIVE DEVELOPMENT Software GmbH
# License: GNU GPL v2 (see License.txt)
# This Revision: $Id$

###############################################################################
# Configure the following variables according to your AVR. The example below
# is for an ATMega8. Program the device with
#     make fuse    # to set the clock generator, boot section size etc.
#     make flash   # to load the boot loader into flash
#     make lock    # to protect the boot loader from overwriting

DEVICE = atmega328p
BOOTLOADER_ADDRESS = 0x7000
F_CPU = 12000000

# FUSES: EXT: 0xFD  HIGH: 0xDA  LOW: 0xDF

###############################################################################

AVRDUDE = avrdude -c stk500v2 -P avrdoper -p $(DEVICE)

LDFLAGS += -Wl,--relax,--gc-sections -Wl,--section-start=.text=$(BOOTLOADER_ADDRESS)

# Omit -fno-* options when using gcc 3, it does not support them.
COMPILE = avr-gcc -Wall -Os -fno-move-loop-invariants -fno-tree-scev-cprop -fno-inline-small-functions -Iusbdrv -I. -mmcu=$(DEVICE) -DF_CPU=$(F_CPU)
# NEVER compile the final product with debugging! Any debug output will
# distort timing so that the specs can't be met.

OBJECTS =  usbdrv/usbdrvasm.o main.o

# symbolic targets:
all:	main.hex main.lss h

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

.c.s:
	$(COMPILE) -S $< -o $@

flash:	all
	$(AVRDUDE) -U flash:w:main.hex:i

readflash:
	$(AVRDUDE) -U flash:r:read.hex:i

fuse:
	$(AVRDUDE) -U hfuse:w:$(FUSEH):m -U lfuse:w:$(FUSEL):m

lock:
	$(AVRDUDE) -U lock:w:0x2f:m

read_fuses:
	$(UISP) --rd_fuses

clean:
	rm -f main.hex main.bin *.o usbdrv/*.o main.s usbdrv/usbdrv.s

# file targets:
main.bin:	$(OBJECTS)
	$(COMPILE) -o main.bin $(OBJECTS) $(LDFLAGS)

main.hex:	main.bin
	rm -f main.hex main.eep.hex
	avr-objcopy -j .text -j .data -O ihex main.bin main.hex
	avr-size main.hex

disasm:	main.bin
	avr-objdump -d main.bin

main.lss: main.bin
	avr-objdump -h -S main.bin > main.lss

cpp:
	$(COMPILE) -E main.c

h:
	HEXtoH.exe main.hex bootloader.h
