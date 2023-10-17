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
#define N_DIGITS    5

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
#define TURN_ON_CYAN TURN_ON_GREEN_LED; TURN_ON_BLUE_LED
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
    TRIGGER_MODE_WAITING_FOR_NEXT_PIC,
    TRIGGER_MODE_END,
}TriggerMode_e;

typedef enum{
    VARIABLE_CHANGE_TRT = 0,
    VARIABLE_CHANGE_TT,
    VARIABLE_CHANGE_NPIC,
    VARIABLE_CHANGE_INVERV,
}ChangeVariable_e;

typedef struct{
    RotaryEncoderRotation_e dir;
    bool bt_press;   // true if we pressed on the rotary encoder
}RotaryEncoderStruct_s;

/**
 * All trigger settings
 */
typedef struct{
    int16_t tt;         // Time to Trigger
    int16_t trt;        // Trigger Duration
    int16_t n_pic;      // number of pictures for timelapse mode
    int16_t tmlps_interv;   // The interval in seconds between different timelapse
}ShutterTriggerVars_s;

/**
 * Any system/run-time config
 */
typedef struct{
    TriggerMode_e mode;                 // the current trigger state machine mode
    ChangeVariable_e selected_to_change;     // index to variable we selected to be changed
    uint8_t selected_digit;         // index to digit to be changed
    int16_t *var_to_change;        // pointer to variable to change
}SystemConfig_s;

const int16_t tens_radix[5] = {1, 10, 100, 1000, 10000};

uint8_t blinking_led_var = 0;       // Variable used for blinking an LED during pre-trigger time
uint8_t timer_counter = 0;          // Counter used to make TIMER0 count once a second

ShutterTriggerVars_s shutter_trigger = {0};
ShutterTriggerVars_s old_shutter_trigger = {0};
RotaryEncoderStruct_s encoder_vars;
SystemConfig_s sys;

int text_to_ascii(uint16_t n, char *text);
void update_sutter_trigger_time(void);
void increment_change_var(void);
void start_arming(void);

int main(void){
    uint8_t mode_bt_press = 0;
    int16_t change_by;
    // Clear variables
    shutter_trigger.tt = 0;
    shutter_trigger.trt = 10;
    sys.mode = TRIGGER_MODE_STANDBY;
    sys.selected_to_change = VARIABLE_CHANGE_TRT;
    sys.var_to_change = &shutter_trigger.trt;
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

    oled_send_text_offset("# Pics:", 5, 0);
    oled_send_text_offset("Interv:", 5, 64);

    
    // Enable interrupts
    sei();
    
    while(1){
        _delay_ms(10);
        if(sys.mode == TRIGGER_MODE_STANDBY){
            // if we turn the rotary encoder
            if(encoder_vars.dir != ROTARY_ENCODER_ROT_NOTHING){
                change_by = tens_radix[sys.selected_digit];
                if(encoder_vars.dir == ROTARY_ENCODER_ROT_CW){
                    TURN_ON_RED_LED;
                    TURN_OFF_GREEN_LED;
                    *sys.var_to_change += change_by;
                }
                else if(encoder_vars.dir ==ROTARY_ENCODER_ROT_CCW){
                    TURN_ON_GREEN_LED;
                    TURN_OFF_RED_LED;
                    *sys.var_to_change -= change_by;
                    // special case for trt were we are capping it at 1
                    if(shutter_trigger.trt < 1)
                        shutter_trigger.trt = 1;
                    // cap any values at 0
                    if(*sys.var_to_change < 0)
                        *sys.var_to_change = 0;
                }
                update_sutter_trigger_time();
                // Clear this variable after we are done with it
                encoder_vars.dir = ROTARY_ENCODER_ROT_NOTHING;
            }
            // if we press the trigger button, change MODE and start the arming
            if(READ_TRIGGER_BUTTON == 0){
                start_arming();
            }
            // if we press the mode button, switch modes
            if(READ_MODE_BUTTON == 0 && mode_bt_press == 0){
                mode_bt_press = 1;
                increment_change_var();
            }
            else if(READ_MODE_BUTTON == 1 && mode_bt_press){
                mode_bt_press = 0;
            }
            // if we press the rotary encoder button, switch what value we are changing
            if(READ_ROTARY_ENCODER_BUTTON == 1 && encoder_vars.bt_press == 0){
                encoder_vars.bt_press = 1;
                sys.selected_digit += 1;
                if(sys.selected_digit >= N_DIGITS){
                    sys.selected_digit = 0;
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

void start_arming(void){
    // check that interval time, if npic != 0, is greater than trt
    if(shutter_trigger.n_pic != 0){
        if(shutter_trigger.tmlps_interv < (shutter_trigger.trt+shutter_trigger.tt)){
            return;
        }
    }

    memcpy(&old_shutter_trigger, &shutter_trigger, sizeof(shutter_trigger));
    blinking_led_var = 0;
    timer_counter = 0;
    RESET_TIMER;
    TURN_OFF_ALL_LED;
    if(shutter_trigger.tt == 0){
        TRIGGER_ON;
        sys.mode = TRIGGER_MODE_TRIGGERED;
    } else {
        sys.mode = TRIGGER_MODE_ARM;
    }
}

/**
 * Gets called when we want to increment what variable we are changing
 */
void increment_change_var(void){
    switch (sys.selected_to_change){
    case VARIABLE_CHANGE_TRT:
        sys.selected_to_change = VARIABLE_CHANGE_TT;
        sys.var_to_change = &shutter_trigger.tt;
        break;
    case VARIABLE_CHANGE_TT:
        sys.selected_to_change = VARIABLE_CHANGE_NPIC;
        sys.var_to_change = &shutter_trigger.n_pic;
        break;
    case VARIABLE_CHANGE_NPIC:
        sys.selected_to_change = VARIABLE_CHANGE_INVERV;
        sys.var_to_change = &shutter_trigger.tmlps_interv;
        break;
    case VARIABLE_CHANGE_INVERV:
        sys.selected_to_change = VARIABLE_CHANGE_TRT;
        sys.var_to_change = &shutter_trigger.trt;
        break;
    }
    update_sutter_trigger_time();
}

/**
 * Updates the display with the current trigger times
 */
void update_sutter_trigger_time(void){
    char text[7];
    int8_t underscore_digit;

    underscore_digit = 0xFF;
    text_to_ascii(shutter_trigger.trt, text);
    if(sys.selected_to_change == VARIABLE_CHANGE_TRT){
        underscore_digit = N_DIGITS-sys.selected_digit-1;
    }
    oled_send_text_underscore(text, 1, underscore_digit);

    underscore_digit = 0xFF;
    text_to_ascii(shutter_trigger.tt, text);
    if(sys.selected_to_change == VARIABLE_CHANGE_TT){
        underscore_digit = N_DIGITS-sys.selected_digit-1;
    }
    oled_send_text_underscore(text, 3, underscore_digit);

    underscore_digit = 0xFF;
    text_to_ascii(shutter_trigger.n_pic, text);
    if(sys.selected_to_change == VARIABLE_CHANGE_NPIC){
        underscore_digit = N_DIGITS-sys.selected_digit-1;
    }
    oled_send_text_underscore(text, 6, underscore_digit);

    underscore_digit = 0xFF;
    text_to_ascii(shutter_trigger.tmlps_interv, text);
    if(sys.selected_to_change == VARIABLE_CHANGE_INVERV){
        underscore_digit = N_DIGITS-sys.selected_digit-1;
    }
    oled_send_chars(text, 6, 64, underscore_digit);
}

/**
 * Converts a number to a string
 *
 * todo: rename function
 */
int text_to_ascii(uint16_t n, char *text){
    for(int i=0;i<=6;i++){text[i] = 0;}
    uint8_t text_len = N_DIGITS-1;
    while(1){
        text[text_len] = (n % 10) + 0x30;
        n /= 10;
        if(text_len-- == 0){
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

    switch(sys.mode){
        case TRIGGER_MODE_ARM:
            blinking_led_var = !blinking_led_var;
            if(blinking_led_var){TURN_ON_CYAN;}else{TURN_OFF_ALL_LED;}
            if(shutter_trigger.tt != 0){shutter_trigger.tt--;}
            if(shutter_trigger.tt == 0){
                // TRIGGERED
                TRIGGER_ON;
                sys.mode = TRIGGER_MODE_TRIGGERED;
            }
            break;
        case TRIGGER_MODE_TRIGGERED:
            TURN_ON_BLUE_LED;
            shutter_trigger.trt--;
            if(shutter_trigger.trt == 0){
                if(shutter_trigger.n_pic != 0){
                    sys.mode = TRIGGER_MODE_WAITING_FOR_NEXT_PIC;
                    shutter_trigger.n_pic--;
                } else {
                    // Trigger has stopped
                    sys.mode = TRIGGER_MODE_END;
                }
                
            }
            break;
        case TRIGGER_MODE_WAITING_FOR_NEXT_PIC:
            TRIGGER_OFF; TURN_OFF_ALL_LED;
            if(shutter_trigger.tmlps_interv == 0){
                shutter_trigger.trt = old_shutter_trigger.trt;
                shutter_trigger.tt = old_shutter_trigger.tt;
                if(shutter_trigger.tt == 0){
                    TRIGGER_ON;
                    sys.mode = TRIGGER_MODE_TRIGGERED;
                } else {
                    sys.mode = TRIGGER_MODE_ARM;
                }
            }
            break;
        case TRIGGER_MODE_END:
            TRIGGER_OFF;
            TURN_OFF_ALL_LED;
            memcpy(&shutter_trigger, &old_shutter_trigger, sizeof(shutter_trigger));
            sys.mode = TRIGGER_MODE_STANDBY;
            break;
        default:
            return; // intentional as we don't want to update the screen if not in picture taking mode
    }

    if(shutter_trigger.tmlps_interv != 0){
        shutter_trigger.tmlps_interv--;
    }
    update_sutter_trigger_time();
}

/**
 * Interrupt for PortC, which is only for the rotary encoder
 */
ISR(PCINT_vect){
    static uint8_t pvcv;       // Previous Value (XX) and Current Value (YY), 0bXXYY
    // static uint8_t last_val;   // the last rotary encoder pin values

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
