/**
 * Camera Shutter Control Project, oled module
 * By Electro707, 2023
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 */

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

void oled_send_buff(uint8_t *buff, uint8_t len, uint8_t starting_line, uint8_t column_start);

void oled_send_chars(char *text, uint8_t starting_line, uint8_t column_start, uint8_t underscore_char);

#endif
