###############################################################################
# Makefile for the project tempfin1
###############################################################################

## General Flags
PROJECT = tempfin1
MCU = atmega328p
TARGET = tempfin1.elf
CC = avr-gcc

CPP = avr-g++

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -std=gnu99                    -DF_CPU=16000000UL -O0 -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d 

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS += -Wl,-u,vfprintf  -lprintf_flt  -lm -Wl,-Map=tempfin1.map


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom -R .fuse -R .lock -R .signature

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings


## Libraries
LIBS = -lprintf_flt -lm 

## Objects that must be built in order to link
OBJECTS = util.o kinen.o report.o main.o xio.o xio_usart.o system.o controller.o xio_spi.o heater.o sensor.o 

## Objects explicitly added by the user
LINKONLYOBJECTS = 

## Build
all: $(TARGET) tempfin1.hex tempfin1.eep tempfin1.lss size

## Compile
util.o: ../util.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

kinen.o: ../kinen.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

report.o: ../report.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

main.o: ../main.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

xio.o: ../xio/xio.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

xio_usart.o: ../xio/xio_usart.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

system.o: ../system.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

controller.o: ../controller.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

xio_spi.o: ../xio/xio_spi.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

heater.o: ../heater.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

sensor.o: ../sensor.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

##Link
$(TARGET): $(OBJECTS)
	 $(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

%.hex: $(TARGET)
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

%.eep: $(TARGET)
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0

%.lss: $(TARGET)
	avr-objdump -h -S $< > $@

size: ${TARGET}
	@echo
	@avr-size -C --mcu=${MCU} ${TARGET}

## Clean target
.PHONY: clean
clean:
	-rm -rf $(OBJECTS) tempfin1.elf dep/* tempfin1.hex tempfin1.eep tempfin1.lss tempfin1.map


## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

