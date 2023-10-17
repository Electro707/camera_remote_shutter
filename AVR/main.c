/**
 * Camera Shutter Control Project
 * By Electro707, 2023
 *
 * This is the main firmware file for the camera shutter board.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 */
#define F_CPU 8000000       // CPU clock cycles

#include <stdbool.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "USI_TWI_Master.h"
#include "oled.h"


/* LED Related Macros */
#define LED_PORT PORTB
#define LED_RED_PIN (1 << 1)
#define LED_GREEN_PIN (1 << 3)
#define LED_BLUE_PIN (1 << 5)
#define TURN_ON_RED_LED (LED_PORT &= ~LED_RED_PIN)
#define TURN_ON_GREEN_LED (LED_PORT &= ~LED_GREEN_PIN)
#define TURN_ON_BLUE_LED (LED_PORT &= ~LED_BLUE_PIN)
#define TURN_OFF_RED_LED (LED_PORT |= LED_RED_PIN)
#define TURN_OFF_GREEN_LED (LED_PORT |= LED_GREEN_PIN)
#define TURN_OFF_BLUE_LED (LED_PORT |= LED_BLUE_PIN)
#define TURN_OFF_ALL_LED TURN_OFF_RED_LED; TURN_OFF_GREEN_LED; TURN_OFF_BLUE_LED
/* Rotary Encoder and Button Read Macros */
#define READ_ROTARY_ENCODER_BIT ((PINA >> 6) & 0b11)
#define READ_ROTARY_ENCODER_BUTTON ((PINB >> 4) & 0b1)
#define READ_TRIGGER_BUTTON ((PINA >> 2) & 0b1)
#define READ_MODE_BUTTON ((PINA >> 3) & 0b1)
/* Trigger Related Macros */
#define TRIGGER_ON PORTA |= (1 << 0)
#define TRIGGER_OFF PORTA &= ~(1 << 0)

#define RESET_TIMER TCNT0H = 0; TCNT0L = 0

typedef enum{
    ROTARY_ENCODER_ROT_NOTHING = 0,
    ROTARY_ENCODER_ROT_CW = 1,
    ROTARY_ENCODER_ROT_CCW = 2,
}RotaryEncoderRotation_e;

typedef enum{
    TRIGGER_MODE_STANDBY = 0,
    TRIGGER_MODE_ARM,
    TRIGGER_MODE_TRIGGERED,
    TRIGGER_MODE_END,
}TriggerMode_e;

typedef struct{
    RotaryEncoderRotation_e dir;
    bool bt_press;   // true if we pressed on the rotary encoder
}RotaryEncoderStruct;

typedef struct{
    uint16_t tt;        // Time to Trigger
    uint16_t trt;       // Trigger Duration
    uint16_t old_tt;
    uint16_t old_trt;
    TriggerMode_e mode;
    uint8_t selected_to_change;     // index to variable we selected to be changed
    uint16_t *var_to_change;        // pointer to variable to change
}ShutterTriggerVars;

uint8_t blinking_led_var = 0;       // Variable used for blinking an LED during pre-trigger time
uint8_t timer_counter = 0;          // Counter used to make TIMER0 count once a second

ShutterTriggerVars shutter_trigger;
RotaryEncoderStruct encoder_vars;

int text_to_ascii(uint16_t n, char *text);
void update_sutter_trigger_time(void);

int main(void){
    // Clear variables
    shutter_trigger.tt = 0;
    shutter_trigger.trt = 10;
    shutter_trigger.mode = TRIGGER_MODE_STANDBY;
    shutter_trigger.selected_to_change = 1;
    shutter_trigger.var_to_change = &shutter_trigger.trt;
    encoder_vars.bt_press = 0;

    // Setup GPIO
    DDRA = 0b00100011;
    DDRB = 0b00101111;

    // Setup Timer
    // 8Mhz / 256 / 250 = 125 Hz
    OCR0A = 250;
    TCCR0A = 1;
    TCCR0B = 0b100;
    TIMSK |= 1 << OCIE0A;

    // Clear Rotary Encoder LEDs
    TURN_OFF_RED_LED;
    TURN_OFF_GREEN_LED;
    TURN_OFF_BLUE_LED;
    
    // Enable Pin Change Interrupt for the important pins
    GIMSK |= (1 << PCIE1);
//     PCMSK0 |= (1 << PCINT2) | (1 << PCINT3) | (1 << PCINT6) | (1 << PCINT7);
//     PCMSK1 |= (1 << PCINT12);
    PCMSK0 |= (1 << PCINT6) | (1 << PCINT7);
    
    USI_TWI_Master_Initialise();
    oled_init();
    // oled_send_text("CAMERA SHUTTER", 0);
    // oled_send_text("CONTROLLER REV 0.1", 1);
    update_sutter_trigger_time();
    oled_send_text("Shutter Speed:", 0);
    oled_send_text("T- Trigger:", 2);
    oled_send_text("Timelapse:", 4);

    oled_send_chars("# Pics:", 5, 0, 0);
    oled_send_chars("Interv:", 5, 64, 0);

    
    // Enable interrupts
    sei();
    
    while(1){
        _delay_ms(10);
        if(shutter_trigger.mode == TRIGGER_MODE_STANDBY){
            // if we turn the rotary encoder
            if(encoder_vars.dir != ROTARY_ENCODER_ROT_NOTHING){
                if(encoder_vars.dir == ROTARY_ENCODER_ROT_CW){
                    TURN_ON_RED_LED;
                    TURN_OFF_GREEN_LED;
                    (*shutter_trigger.var_to_change)++;
                }
                else if(encoder_vars.dir ==ROTARY_ENCODER_ROT_CCW){
                    TURN_ON_GREEN_LED;
                    TURN_OFF_RED_LED;
                    if(shutter_trigger.selected_to_change == 1){
                        if(*shutter_trigger.var_to_change > 1){(*shutter_trigger.var_to_change)--;}
                    } else {
                        if(*shutter_trigger.var_to_change > 0){(*shutter_trigger.var_to_change)--;}
                    }
                }
                update_sutter_trigger_time();
                // Clear this variable after we are done with it
                encoder_vars.dir = ROTARY_ENCODER_ROT_NOTHING;
            }
            // if we press the trigger button, change MODE and start the arming
            if(READ_TRIGGER_BUTTON == 0){
                shutter_trigger.old_trt = shutter_trigger.trt;
                shutter_trigger.old_tt = shutter_trigger.tt;
                blinking_led_var = 0;
                timer_counter = 0;
                RESET_TIMER;
                TURN_OFF_ALL_LED;
                if(shutter_trigger.tt == 0){
                    TRIGGER_ON;
                    shutter_trigger.mode = TRIGGER_MODE_TRIGGERED;
                } else {
                    shutter_trigger.mode = TRIGGER_MODE_ARM;
                }
            }
            // if we press the mode button, switch modes
            if(READ_MODE_BUTTON == 0){
            }
            // if we press the rotary encoder button, switch what value we are changing
            if(READ_ROTARY_ENCODER_BUTTON == 1 && encoder_vars.bt_press == 0){
                encoder_vars.bt_press = 1;
                if(shutter_trigger.selected_to_change == 1){
                    shutter_trigger.selected_to_change = 2;
                    shutter_trigger.var_to_change = &shutter_trigger.tt;
                }
                else{
                    shutter_trigger.selected_to_change = 1;
                    shutter_trigger.var_to_change = &shutter_trigger.trt;
                }
                update_sutter_trigger_time();
            }
            // down-press on rotary encoder button
            else if(READ_ROTARY_ENCODER_BUTTON == 0 && encoder_vars.bt_press == 1){
                encoder_vars.bt_press = 0;
            }
        }
    }
}

/**
 * Updates the display with the current trigger times
 */
void update_sutter_trigger_time(void){
    char text[7];
    text_to_ascii(shutter_trigger.trt, text);
    if(shutter_trigger.selected_to_change == 1){
        text[0] = '>';
    } else {text[0] = '-';}
    oled_send_text(text, 1);
    text_to_ascii(shutter_trigger.tt, text);
    if(shutter_trigger.selected_to_change == 2){
        text[0] = '>';
    } else {text[0] = '-';}
    oled_send_text(text, 3);
}

/**
 * Converts a number to a string
 *
 * todo: rename function
 */
int text_to_ascii(uint16_t n, char *text){
    for(int i=0;i<=6;i++){text[i] = 0;}
    uint8_t text_len = 5;
    while(1){
        text[text_len--] = (n % 10) + 0x30;
        n /= 10;
        if(text_len == 0){
            break;
        }
    }
    return text_len;
}

/**
 * Interrupt for timer. This gets triggered once every 1/125 seconds
 */
ISR(TIMER0_COMPA_vect){
    // Only actually do stuff here once the counter reaches one second
    if(++timer_counter != 125){return;}else{timer_counter = 0;}        

    switch(shutter_trigger.mode){
        case TRIGGER_MODE_ARM:
            blinking_led_var = !blinking_led_var;
            if(blinking_led_var){TURN_ON_BLUE_LED;}else{TURN_OFF_BLUE_LED;}
            if(shutter_trigger.tt != 0){shutter_trigger.tt--;}
            if(shutter_trigger.tt == 0){
                // TRIGGERED
                TRIGGER_ON;
                shutter_trigger.mode = TRIGGER_MODE_TRIGGERED;
            }
            update_sutter_trigger_time();
            break;
        case TRIGGER_MODE_TRIGGERED:
            shutter_trigger.trt--;
            if(shutter_trigger.trt == 0){
                // Trigger has stopped
                shutter_trigger.mode = TRIGGER_MODE_END;
            }
            TURN_ON_BLUE_LED;
            update_sutter_trigger_time();
            break;
        case TRIGGER_MODE_END:
            TRIGGER_OFF;
            TURN_OFF_ALL_LED;
            shutter_trigger.trt = shutter_trigger.old_trt;
            shutter_trigger.tt = shutter_trigger.old_tt;
            shutter_trigger.mode = TRIGGER_MODE_STANDBY;
            update_sutter_trigger_time();
            break;
        default:
            break;
    }
}

/**
 * Interrupt for PortC, which is only for the rotary encoder
 */
ISR(PCINT_vect){
    static uint8_t pvcv;       // Previous Value (XX) and Current Value (YY), 0bXXYY
    static uint8_t last_val;   // the last rotary encoder pin values

    uint8_t r = READ_ROTARY_ENCODER_BIT;
    // only update if the value of the rotary encoder has changed
    // todo: wouldn't this always be the case anyways due to the interrupt?? test without
    //if(r != encoder_vars.last_val){
    //    encoder_vars.last_val = r;
        pvcv = ((pvcv << 2) | r) & 0x0F;
            
        switch(pvcv){
            case 0b0001:
            case 0b0111:
            case 0b1000:
            case 0b1110:
                encoder_vars.dir = ROTARY_ENCODER_ROT_CW;
                break;
            case 0b0010:
            case 0b0100:
            case 0b1011:
            case 0b1101:
                encoder_vars.dir = ROTARY_ENCODER_ROT_CCW;
                break;
        }
    //}
    // clear the interrupt flag for PCIF
    GIFR |= (1 << PCIF);
}
