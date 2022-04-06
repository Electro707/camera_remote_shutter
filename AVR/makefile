PORT=/dev/ttyUSB0
MCU=attiny261

LDFLAGS=-Wall, -Wl,-gc-sections -Wl,-relax -Wl,-Map=$(TARGET).map,--cref
#CFLAGS=-g -Wall -mcall-prologues -mmcu=$(MCU) -Os
#CFLAGS=-g -Wall -mmcu=$(MCU) -Os
CFLAGS=-Wall -mmcu=$(MCU) -Os

BUILD_FOLDER=build/

default: compile size

compile:
	mkdir -p build
	avr-gcc $(CFLAGS) -c USI_TWI_Master.c -o $(BUILD_FOLDER)USI_TWI_Master.o
	avr-gcc $(CFLAGS) -c oled.c -o $(BUILD_FOLDER)oled.o
	avr-gcc $(CFLAGS) -c letters.c -o $(BUILD_FOLDER)letters.o
	avr-gcc $(CFLAGS) main.c $(BUILD_FOLDER)USI_TWI_Master.o $(BUILD_FOLDER)oled.o $(BUILD_FOLDER)letters.o -o $(BUILD_FOLDER)out.elf
	avr-objcopy -j .text -j .data -O ihex $(BUILD_FOLDER)out.elf $(BUILD_FOLDER)out.hex

quick: compile size program

size:
	avr-size -C --mcu=$(MCU) $(BUILD_FOLDER)out.elf

program: compile
	avrdude -v -P $(PORT) -p $(MCU) -cavrisp -b 19200 -U flash:w:$(BUILD_FOLDER)out.hex -U efuse:w:0xff:m  -U hfuse:w:0xdf:m  -U lfuse:w:0xE2:m

clean:
	rm -rf build

documentation:
	doxygen doxygen_config
