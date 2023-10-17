#include "oled.h"

void send_i2c_command(uint8_t i2cdata);

void send_i2c_command(uint8_t i2cdata){
uint8_t to_send[] = {OLED_SLAVE_ADDR<<1, 0x00, i2cdata};
USI_TWI_Start_Transceiver_With_Data(to_send, 3);
}


void oled_set_text_position(uint8_t col, uint8_t line){
    send_i2c_command(0x21);//Set Column Address
    send_i2c_command(col);
    send_i2c_command(127);
    send_i2c_command(0x22);//Set Page Address
    send_i2c_command(line);
    send_i2c_command(line);
}

void oled_clear_display(){
    uint8_t i2c_buff[10];
    i2c_buff[0] = OLED_SLAVE_ADDR<<1;
    i2c_buff[1] = 0x40;
    for (uint8_t x=0; x<8; x++) {
        i2c_buff[x+2] = 0x00;
    }

    send_i2c_command(0x21);//Set Column Address
    send_i2c_command(0x00);
    send_i2c_command(127);
    send_i2c_command(0x22);//Set Page Address
    send_i2c_command(0x00);
    send_i2c_command(7);
    for (uint16_t i=0; i<128; i++) {
        USI_TWI_Start_Transceiver_With_Data(i2c_buff, 10);
    }
}

void oled_send_text(char *text, uint8_t starting_line){
    oled_send_chars(text, starting_line, 0, 0xFF);
}

void oled_send_text_underscore(char *text, uint8_t starting_line, uint8_t underscore_char){
    oled_send_chars(text, starting_line, 0, underscore_char);
}

void oled_send_text_offset(char *text, uint8_t starting_line, uint8_t offset){
    oled_send_chars(text, starting_line, offset, 0);
}

void oled_send_chars(char *text, uint8_t starting_line, uint8_t column_start, uint8_t underscore_char){
    uint8_t i2c_buff[10];
    uint8_t k = 0;
    uint8_t txt_per_line = 0;
    uint8_t current_line = starting_line;
    oled_set_text_position(column_start, starting_line);

    i2c_buff[0] = OLED_SLAVE_ADDR<<1;
    i2c_buff[1] = 0x40;
    i2c_buff[7] = 0x00;

    uint8_t lenght = strlen(text);
    for(uint8_t j=0;j<lenght;j++){
        if(text[j] == '\n'){
            oled_set_text_position(column_start, ++current_line);
            txt_per_line = 0;
            continue;
        }
        if(text[j] == ' '){
            for(k=0;k<5;k++){i2c_buff[k+2] = 0x00;}
            USI_TWI_Start_Transceiver_With_Data(i2c_buff, 8);
            txt_per_line++;
            continue;
        }

        // todo: either verify below, or don't have it at all!
        // if((++txt_per_line)*6 >= 125){
        //     for(k=0;k<5;k++){i2c_buff[k+2] = pgm_read_byte_near(&font[k]);}
        //     USI_TWI_Start_Transceiver_With_Data(i2c_buff, 8);
        //     oled_set_text_position(column_start, ++current_line);
        //     txt_per_line = 0;
        // }

        for(k=0;k<5;k++){i2c_buff[k+2] = pgm_read_byte_near(&font[(((text[j]-0x20) * 5) + k)]);}
        if(underscore_char == j){
            for(k=0;k<5;k++){i2c_buff[k+2] |= 1<<7;}
        }
        USI_TWI_Start_Transceiver_With_Data(i2c_buff, 8);

    }
}

void oled_init(){
    send_i2c_command(0xAE);//Set while display off

    send_i2c_command(0xD5);//Set Display Clock Divide Ratio/Oscillator Frequency
    send_i2c_command(0x80);

    send_i2c_command(0xA8);//Set Multiplex Ratio
    send_i2c_command(0x3f);

    send_i2c_command(0xD3);//Set Display Offset
    send_i2c_command(0x00);

    send_i2c_command(0x40 | 0x00);//Set Display Start Line

    send_i2c_command(0x20);//Set Memory Addressing Mode
    send_i2c_command(0x00);

    send_i2c_command(0x8D);//Charge Pump Setting
    send_i2c_command(0x14);

    send_i2c_command(0xA1);//Set Segment Re-map

    send_i2c_command(0xC8);//Set COM Output Scan Direction

    send_i2c_command(0xDA);//Set COM Pins	Hardware Configuration
    send_i2c_command(0x12);

    send_i2c_command(0x81);//Set Contrast Control
    send_i2c_command(0xcf);

    send_i2c_command(0xD9);//Set Pre-charge Period
    send_i2c_command(0xF1);

    send_i2c_command(0xDB);//Set VCOMH Deselect Level
    send_i2c_command(0x40);

    send_i2c_command(0xA6);//Normal display(not inverted)

    send_i2c_command(0x21);//Set Column Address
    send_i2c_command(0x00);
    send_i2c_command(127);

    send_i2c_command(0x22);//Set Page Address
    send_i2c_command(0x00);
    send_i2c_command(7);

    oled_clear_display();
    send_i2c_command(0xA5);//Turn whole display on
    send_i2c_command(0xAF);//Display on
    send_i2c_command(0xA4);//Turn display to follow ram
}
