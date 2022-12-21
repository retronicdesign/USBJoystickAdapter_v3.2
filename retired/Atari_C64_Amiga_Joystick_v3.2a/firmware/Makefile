################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL := cmd.exe
RM := rm -rf

USER_OBJS :=

LIBS := 
PROJ := 

O_SRCS := 
C_SRCS := 
S_SRCS := 
S_UPPER_SRCS := 
OBJ_SRCS := 
ASM_SRCS := 
PREPROCESSING_SRCS := 
OBJS := 
OBJS_AS_ARGS := 
C_DEPS := 
C_DEPS_AS_ARGS := 
EXECUTABLES := 
OUTPUT_FILE_PATH :=
OUTPUT_FILE_PATH_AS_ARGS :=
AVR_APP_PATH :=$$$AVR_APP_PATH$$$
QUOTE := "
ADDITIONAL_DEPENDENCIES:=
OUTPUT_FILE_DEP:=
LIB_DEP:=
LINKER_SCRIPT_DEP:=

# Every subdirectory with source files must be described here
SUBDIRS :=  \
../usbdrv


# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS +=  \
../usbdrv/usbdrv.c \
../devdesc.c \
../main.c \
../ataristyle.c


PREPROCESSING_SRCS +=  \
../usbdrv/usbdrvasm.S


ASM_SRCS += 


OBJS +=  \
usbdrv/usbdrv.o \
usbdrv/usbdrvasm.o \
devdesc.o \
main.o \
ataristyle.o

OBJS_AS_ARGS +=  \
usbdrv/usbdrv.o \
usbdrv/usbdrvasm.o \
devdesc.o \
main.o \
ataristyle.o

C_DEPS +=  \
usbdrv/usbdrv.d \
usbdrv/usbdrvasm.d \
devdesc.d \
main.d \
ataristyle.d

C_DEPS_AS_ARGS +=  \
usbdrv/usbdrv.d \
usbdrv/usbdrvasm.d \
devdesc.d \
main.d \
ataristyle.d

OUTPUT_FILE_PATH +=Atari_C64_Amiga_Joystick_v3.2a.elf

OUTPUT_FILE_PATH_AS_ARGS +=Atari_C64_Amiga_Joystick_v3.2a.elf

ADDITIONAL_DEPENDENCIES:=

OUTPUT_FILE_DEP:= ./makedep.mk

LIB_DEP+= 

LINKER_SCRIPT_DEP+= 


# AVR32/GNU C Compiler









usbdrv/%.o: ../usbdrv/%.c
	@echo Building file: $<
	@echo Invoking: AVR/GNU C Compiler : 4.3.3
	$(QUOTE)C:\WinAVR-20100110\bin\avr-gcc.exe$(QUOTE)  -x c -funsigned-char -funsigned-bitfields -DF_CPU=12000000UL  -I"..\usbdrv" -I"D:\Program Files (x86)\Atmel\Studio\7.0\Packs\atmel\ATmega_DFP\1.2.150\include"  -Os -ffunction-sections -fdata-sections -fpack-struct -fshort-enums -Wall  -mmcu=atmega328p  -c -gdwarf-2 -std=gnu99 -MD -MP -MF "$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)"   -o "$@" "$<" 
	@echo Finished building: $<
	

./%.o: .././%.c
	@echo Building file: $<
	@echo Invoking: AVR/GNU C Compiler : 4.3.3
	$(QUOTE)C:\WinAVR-20100110\bin\avr-gcc.exe$(QUOTE)  -x c -funsigned-char -funsigned-bitfields -DF_CPU=12000000UL  -I"..\usbdrv" -I"D:\Program Files (x86)\Atmel\Studio\7.0\Packs\atmel\ATmega_DFP\1.2.150\include"  -Os -ffunction-sections -fdata-sections -fpack-struct -fshort-enums -Wall  -mmcu=atmega328p  -c -gdwarf-2 -std=gnu99 -MD -MP -MF "$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)"   -o "$@" "$<" 
	@echo Finished building: $<
	



# AVR32/GNU Preprocessing Assembler



# AVR32/GNU Assembler
usbdrv/usbdrvasm.o: ../usbdrv/usbdrvasm.S
	@echo Building file: $<
	@echo Invoking: AVR/GNU Assembler : 4.3.3
	$(QUOTE)C:\WinAVR-20100110\bin\avr-gcc.exe$(QUOTE) -Wa,-gdwarf2 -x assembler-with-cpp -c  -mmcu=atmega328p  -Wall -gdwarf-2 -std=gnu99                                                                                                                           -DF_CPU=12000000UL -Os -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -Wall -gdwarf-2 -std=gnu99                                                                                                                           -DF_CPU=12000000UL -Os -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -I "..\usbdrv" -I "D:\Program Files (x86)\Atmel\Studio\7.0\Packs\atmel\ATmega_DFP\1.2.150\include"  -MD -MP -MF "$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)"   -o "$@" "$<" 
	@echo Finished building: $<
	


usbdrv/%.o: ../usbdrv/%.S
	@echo Building file: $<
	@echo Invoking: AVR/GNU Assembler : 4.3.3
	$(QUOTE)C:\WinAVR-20100110\bin\avr-gcc.exe$(QUOTE) -Wa,-gdwarf2 -x assembler-with-cpp -c  -mmcu=atmega328p  -Wall -gdwarf-2 -std=gnu99                                                                                                                           -DF_CPU=12000000UL -Os -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -Wall -gdwarf-2 -std=gnu99                                                                                                                           -DF_CPU=12000000UL -Os -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -I "..\usbdrv" -I "D:\Program Files (x86)\Atmel\Studio\7.0\Packs\atmel\ATmega_DFP\1.2.150\include"  -MD -MP -MF "$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)"   -o "$@" "$<" 
	@echo Finished building: $<
	




ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: $(OUTPUT_FILE_PATH) $(ADDITIONAL_DEPENDENCIES)

$(OUTPUT_FILE_PATH): $(OBJS) $(USER_OBJS) $(OUTPUT_FILE_DEP) $(LIB_DEP) $(LINKER_SCRIPT_DEP)
	@echo Building target: $@
	@echo Invoking: AVR/GNU Linker : 4.3.3
	$(QUOTE)C:\WinAVR-20100110\bin\avr-gcc.exe$(QUOTE) -o$(OUTPUT_FILE_PATH_AS_ARGS) $(OBJS_AS_ARGS) $(USER_OBJS) $(LIBS) -Wl,-Map="Atari_C64_Amiga_Joystick_v3.2a.map" -Wl,--start-group  -Wl,--end-group -Wl,-section-start=.bootloader=0x7000  -Wl,-section-start=.noinit=0x80013b   -mmcu=atmega328p   
	@echo Finished building target: $@
	"C:\WinAVR-20100110\bin\avr-objcopy.exe" -O ihex -R .eeprom -R .fuse -R .lock -R .signature -R .user_signatures  "Atari_C64_Amiga_Joystick_v3.2a.elf" "Atari_C64_Amiga_Joystick_v3.2a.hex"
	"C:\WinAVR-20100110\bin\avr-objcopy.exe" -j .eeprom  --set-section-flags=.eeprom=alloc,load --change-section-lma .eeprom=0  --no-change-warnings -O ihex "Atari_C64_Amiga_Joystick_v3.2a.elf" "Atari_C64_Amiga_Joystick_v3.2a.eep" || exit 0
	"C:\WinAVR-20100110\bin\avr-objdump.exe" -h -S "Atari_C64_Amiga_Joystick_v3.2a.elf" > "Atari_C64_Amiga_Joystick_v3.2a.lss"
	"C:\WinAVR-20100110\bin\avr-objcopy.exe" -O srec -R .eeprom -R .fuse -R .lock -R .signature -R .user_signatures "Atari_C64_Amiga_Joystick_v3.2a.elf" "Atari_C64_Amiga_Joystick_v3.2a.srec"
	"C:\WinAVR-20100110\bin\avr-size.exe" "Atari_C64_Amiga_Joystick_v3.2a.elf"
	
	





# Other Targets
clean:
	-$(RM) $(OBJS_AS_ARGS) $(EXECUTABLES)  
	-$(RM) $(C_DEPS_AS_ARGS)   
	rm -rf "Atari_C64_Amiga_Joystick_v3.2a.elf" "Atari_C64_Amiga_Joystick_v3.2a.a" "Atari_C64_Amiga_Joystick_v3.2a.hex" "Atari_C64_Amiga_Joystick_v3.2a.lss" "Atari_C64_Amiga_Joystick_v3.2a.eep" "Atari_C64_Amiga_Joystick_v3.2a.map" "Atari_C64_Amiga_Joystick_v3.2a.srec" "Atari_C64_Amiga_Joystick_v3.2a.usersignatures"
	