#ifndef OLED_H
#define OLED_H

#include <avr/io.h>
#include <stdio.h>
#include <string.h>
#include "USI_TWI_Master.h"
#include "letters.h"

#define OLED_SLAVE_ADDR 0x3C

#define scrollspeed 75
#define scrollspeedfast 5

void oled_init();
void oled_send_text(char *text, uint8_t starting_line);
void oled_clear_display();
void oled_set_text_position(uint8_t col, uint8_t line);
void oled_send_text_underscore(char *text, uint8_t starting_line, uint8_t underscore_char);
void oled_send_text_offset(char *text, uint8_t starting_line, uint8_t offset);

void oled_send_chars(char *text, uint8_t starting_line, uint8_t column_start, uint8_t underscore_char);

#endif
