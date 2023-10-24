// https://github.com/cosailer/mpad
// 2023-10-24 by CoSailer

#include QMK_KEYBOARD_H

#include <stdio.h>
#include <avr/interrupt.h>
#include "APDS9960.h"
#include "print.h"
#include "virtser.h"

//single led pin   F5
//backlight pins { B2, B1, B0, B3, F6, E6, F7, D7, F4, C6, C7, E2 }
//                           0,F5
// ┌──────┬──────┬──────┐
// │ ESC  │ CALC │ MYCM │    1,B2   2,B1   3,B0
// ├──────┼──────┼──────┤
// │ Ctl+C│ DEL  │ BSPC │    4,B3   5,F6   6,E6
// ├──────┼──────┼──────┤
// │ SPACE│  Up  │  ENT │    7,F7   8,D7   9,F4
// ├──────┼──────┼──────┤
// │ Left │ Down │ Right│   10,C6  11,C7  12,E2
// └──────┴──────┴──────┘

const uint8_t int_pin = D2;
const uint8_t back_pin[] = { F5, B2, B1, B0, B3, F6, E6, F7, D7, F4, C6, C7, E2 };

static void enable_exInt2(void);

typedef struct _ACK_16_Bits
{
    uint8_t byte1;
    uint8_t byte2;
} ACK_16_Bits;

typedef struct _ACK_32_Bits
{
    uint8_t byte1;
    uint8_t byte2;
    uint8_t byte3;
    uint8_t byte4;
} ACK_32_Bits;

typedef union
{
    uint16_t    U;
    int16_t     I;
    ACK_16_Bits B;
} ACK_16;

typedef union
{
    uint32_t    U;
    int32_t     I;
    ACK_32_Bits B;
} ACK_32;

static uint8_t  password = 0;
static uint32_t key_timer = 0;
static uint8_t  proximity = 0;
static uint16_t light = 0;
static int16_t  die_temp = 0;
static uint16_t voltage = 0;
static uint8_t  gesture_available = 0;

void backlight_setup(void)
{
    setPinOutput(back_pin[0]);
    setPinOutput(back_pin[1]);
    setPinOutput(back_pin[2]);
    setPinOutput(back_pin[3]);
    setPinOutput(back_pin[4]);
    setPinOutput(back_pin[5]);
    setPinOutput(back_pin[6]);
    setPinOutput(back_pin[7]);
    setPinOutput(back_pin[8]);
    setPinOutput(back_pin[9]);
    setPinOutput(back_pin[10]);
    setPinOutput(back_pin[11]);
    setPinOutput(back_pin[12]);
}

void backlight_high(void)
{
    writePinHigh(back_pin[0]);
    writePinHigh(back_pin[1]);
    writePinHigh(back_pin[2]);
    writePinHigh(back_pin[3]);
    writePinHigh(back_pin[4]);
    writePinHigh(back_pin[5]);
    writePinHigh(back_pin[6]);
    writePinHigh(back_pin[7]);
    writePinHigh(back_pin[8]);
    writePinHigh(back_pin[9]);
    writePinHigh(back_pin[10]);
    writePinHigh(back_pin[11]);
    writePinHigh(back_pin[12]);
}

void backlight_low(void)
{
    writePinLow(back_pin[0]);
    writePinLow(back_pin[1]);
    writePinLow(back_pin[2]);
    writePinLow(back_pin[3]);
    writePinLow(back_pin[4]);
    writePinLow(back_pin[5]);
    writePinLow(back_pin[6]);
    writePinLow(back_pin[7]);
    writePinLow(back_pin[8]);
    writePinLow(back_pin[9]);
    writePinLow(back_pin[10]);
    writePinLow(back_pin[11]);
    writePinLow(back_pin[12]);
}

void toggle_arrow(void)
{
    togglePin(back_pin[8]);
    togglePin(back_pin[10]);
    togglePin(back_pin[11]);
    togglePin(back_pin[12]);
}

//read internal adc
uint16_t read_adc( uint8_t ref, uint8_t ch)
{
    //uint8_t ch_a = ch & 0b011111;
    //uint8_t ch_b = ch & 0b100000;
    
    //reset adc
    ADMUX  = 0;
    ADCSRB = 0;

    //set voltage reference and channel
    ADMUX |= ref<<6;
    ADMUX |= (ch & 0b011111);
    ADCSRB |= (ch & 0b100000);
    
    _delay_ms(2);
    
    //10-bit resolution with ADC clock speed of 50 kHz to 200 kHz
    //Enable ADC, set prescaller to /64, ADC clock of 16mHz/64=125kHz
    ADCSRA |= (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);

    //do a dummy read
    ADCSRA |= (1<<ADSC);
    while ((ADCSRA & (1<<ADSC)) != 0);
    
    //actual read
    ADCSRA |= (1<<ADSC);
    while ((ADCSRA & (1<<ADSC)) != 0);
    
    return ADC;
}
    

#ifdef VIRTSER_ENABLE

//  cmd 0: [0xAA] [ C1 ] [0xE0] : authentication          ACK 0:[0xE1] [ P1 ] ['\n']
//  cmd 1: [0xAA] [ B1 ] [ B2 ] [0xE1] : set back light   ACK 1:[0xE1] ['\n']
//  cmd 2: [0xAA] [0xE2] : request timer                  ACK 2:[0xE2] [ T1 ] [ T2 ] [ T3 ] [ T4 ] ['\n']
//  cmd 3: [0xAA] [0xE3] : request voltage                ACK 3:[0xE3] [ U1 ] [ U2 ] ['\n']
//  cmd 4: [0xAA] [0xE4] : request die_temp               ACK 4:[0xE4] [ D1 ] [ D2 ] ['\n']
//  cmd 5: [0xAA] [0xE3] : request proximity              ACK 5:[0xE5] [ P1 ] ['\n']
//  cmd 6: [0xAA] [0xE3] : request light                  ACK 6:[0xE6] [ L1 ] [ L2 ] ['\n']

uint8_t cmd[2];
uint8_t cmd_index = 0;
char buffer[64];    //out buffer

void virtser_send_buffer(char *out, uint8_t size)
{
    for(int i = 0; i < size; i++)
    {
        virtser_send(out[i]);
    }
}

ACK_32 reply2 = {0};
ACK_16 reply3 = {0};
ACK_16 reply4 = {0};
ACK_16 reply6 = {0};

void virtser_recv(uint8_t in)
{
    switch(in) //receive start
    {
        case 0xAA: //cmd start
            cmd_index = 0; //reset index
            cmd[0] = 0;
            cmd[1] = 0;
            break;
            
        case 0xE0: //start authentication
            password = cmd[0];
            //password = ~password;
            
            //send ACK 0
            virtser_send(0xE0);
            virtser_send(password);
            virtser_send('\n');
            break;
            
        case 0xE1: //receive cmd 1
            //execute cmd 1
            //buffer 1
            writePin(back_pin[0],  cmd[1]&0b00000001);
            writePin(back_pin[1],  cmd[1]&0b00000010);
            writePin(back_pin[2],  cmd[1]&0b00000100);
            writePin(back_pin[3],  cmd[1]&0b00001000);
            writePin(back_pin[4],  cmd[1]&0b00010000);
            writePin(back_pin[5],  cmd[1]&0b00100000);
            writePin(back_pin[6],  cmd[1]&0b01000000);
        
            //buffer 0
            writePin(back_pin[7],  cmd[0]&0b00000001);
            writePin(back_pin[8],  cmd[0]&0b00000010);
            writePin(back_pin[9],  cmd[0]&0b00000100);
            writePin(back_pin[10], cmd[0]&0b00001000);
            writePin(back_pin[11], cmd[0]&0b00010000);
            writePin(back_pin[12], cmd[0]&0b00100000);
            //writePin(back_pin[0], cmd[0]&0b01000000);
        
            //send ACK 1
            virtser_send(0xE1);
            virtser_send('\n');
            break;
        
        case 0xE2: //request timer
            //execute cmd 2
            reply2.U = key_timer; //timer_read32();
        
            //send ACK 2
            virtser_send(0xE2);
            virtser_send(reply2.B.byte1);
            virtser_send(reply2.B.byte2);
            virtser_send(reply2.B.byte3);
            virtser_send(reply2.B.byte4);
            virtser_send('\n');
            break;
            
        case 0xE3: //request voltage
            //execute cmd 3
            reply3.U = voltage;
        
            //send ACK 3
            virtser_send(0xE3);
            virtser_send(reply3.B.byte1);
            virtser_send(reply3.B.byte2);
            virtser_send('\n');
            break;
            
        case 0xE4: //request die_temp
            //execute cmd 4
            reply4.I = die_temp;
        
            //send ACK 4
            virtser_send(0xE4);
            virtser_send(reply4.B.byte1);
            virtser_send(reply4.B.byte2);
            virtser_send('\n');
            break;
            
        case 0xE5: //request proximity
            //execute cmd 5
            //readProximity(&proximity);
        
            //send ACK 5
            virtser_send(0xE5);
            virtser_send(proximity);
            virtser_send('\n');
            break;
            
        case 0xE6: //request light
            //execute cmd 6
            //readAmbientLight(&light);
            reply6.U = light;
        
            //send ACK 6
            virtser_send(0xE6);
            virtser_send(reply6.B.byte1);
            virtser_send(reply6.B.byte2);
            virtser_send('\n');
            break;
            
        default:
            cmd[cmd_index] = in;
            cmd_index = 1;
    }
}

#endif

bool encoder_update_user(uint8_t index, bool clockwise)
{
     // First encoder
    if(index == 0)
    {
        if (clockwise)
        {
            tap_code(KC_VOLD);
        }
        else
        {
            tap_code(KC_VOLU);
        }
    }
    
    return false;
}

void keyboard_post_init_user(void)
{
    // Optional - runs on startup
    // configure backlight pins
    backlight_setup();
    wait_ms(10);
    backlight_high();
    wait_ms(100);
    backlight_low();
    wait_ms(500);
    backlight_high();
    wait_ms(100);
    backlight_low();
    wait_ms(500);
    backlight_high();
    wait_ms(100);
    backlight_low();
    
    //enable external interrupt 2 on pin PD2
    enable_exInt2();
    
    //APDS9960 sensor init
    apds9960_init();
  
    wait_ms(1);
    //setProximityGain(PGAIN_1X);
    //enableProximitySensor(false);
    enableLightSensor(false);
    enableGestureSensor(true);
    wait_ms(1);
}

//the housekeeping task is run every 100ms
void housekeeping_task_user(void)
{
    if (timer_elapsed32(key_timer) > 100)  // ms
    {
        key_timer = timer_read32();  // resets timer

        //read internal vcc
        voltage = read_adc(0b01, 0b011110 );
        voltage = 1126400/voltage;
        
        //read ambient light
        readAmbientLight(&light);
        
        //read on-die temperature
        die_temp = read_adc(0b11, 0b100111);
        die_temp = die_temp - 275;

        //check gesture flag
        if(gesture_available == 1)
        {
            cli(); //disable interrupt, the actual calculation will take a while
            if(isGestureAvailable()==true)
            {
                //the directions of the output needs to be adjusted
                //due to the placement of the sensor
                switch ( readGesture() )
                {
                  case DIR_UP:     //right
                    tap_code(KC_RIGHT);
                    break;
                  case DIR_DOWN:   //left
                    tap_code(KC_LEFT);
                    break;
                  case DIR_LEFT:   //up
                    tap_code(KC_UP);
                    break;
                  case DIR_RIGHT:  //down
                    tap_code(KC_DOWN);
                    break;
                  case DIR_NEAR:   //turn off all backlight
                    backlight_low();  //backlight_high();
                    break;
                  case DIR_FAR:    //toggle arrow key backlight
                    toggle_arrow();
                    break;
                  default:         //turn off all backlight
                    backlight_low();
                    break;
                } 
                
                //virtser_send(gesture);
                //virtser_send('\n');
                gesture_available = 0;
            }
            sei();  //enable interrupt
        }
    }
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] =
{
    //                           0,F5
    // ┌──────┬──────┬──────┐
    // │ ESC  │ CALC │ MYCM │    1,B2   2,B1   3,B0
    // ├──────┼──────┼──────┤
    // │ Ctl+C│ DEL  │ BSPC │    4,B3   5,F6   6,E6
    // ├──────┼──────┼──────┤
    // │ SPACE│  Up  │  ENT │    7,F7   8,D7   9,F4
    // ├──────┼──────┼──────┤
    // │ Left │ Down │ Right│   10,C6  11,C7  12,E2
    // └──────┴──────┴──────┘
    
    [0] = LAYOUT_num_pad_4x3(
        KC_ESCAPE,  KC_CALC, KC_MYCM,
        LCTL(KC_C), KC_DEL,  KC_BSPC,
        KC_SPC,     KC_UP,   KC_ENTER,
        KC_LEFT,    KC_DOWN, KC_RIGHT
    )
};


//  interrupt setup
static void enable_exInt2(void)
{
    //set D2(INT2) as input
    setPinInput(int_pin);  

    //setup External Interrupt 2
    EIMSK |= (1<<INT2);   //INT2
    EICRA |= (1<<ISC21);  //falling edge in INT2
    sei();
}

ISR(INT2_vect)
{
    gesture_available = 1;
}
