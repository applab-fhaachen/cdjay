#include <stdlib.h>
#include <bsp/board_api.h>
#include <tusb.h>
#include <pico/stdio.h>
#include <pico/stdlib.h>


// Ensure a default LED pin is available. On Raspberry Pi Pico the onboard LED is GPIO 25.
#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 25
#endif

#ifndef PLAY_LED_PIN
#define PLAY_LED_PIN 15
#endif


//turn the built_in_led on and off
void set_built_in_led(bool led_on) {
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

void set_red_led(bool led_on) {
    gpio_put(PLAY_LED_PIN, led_on);
}

//variables that hold the operands
int operand1 = 0;
int operand2 = 0;

//this flag is used to determine which operand the user sends
//when we get the first input, we set this flag to false
bool waiting_for_first_op = true;


// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
    (void) itf;
    
    char buff[64];
    uint32_t count = tud_cdc_read(buff, sizeof(buff)); //put the received message into the buffer (char array) and store the length as count
    buff[count] = '\0'; // Null-terminate the string so atoi() works correctly

    int value = atoi(buff); //convert the character buffer to integer

    if(waiting_for_first_op){
        operand1 = value; //if the program was waiting for the first operand, set the input integer as the value of the first operand
        waiting_for_first_op = false; //set the flag to false
        set_red_led(true); //turns on the red led to indicate that the Pico is waiting for the second input
        
        tud_cdc_write_str("first number received! send the second number"); //ask user to send the second number
        tud_cdc_write_flush(); //flush the buffer to ensure the message is sent fully to the host
    }
    else {
        //get the second operand and calculate the sum
        operand2 = value; 
        int sum = operand1 + operand2;

        char out[64]; //a character buffer to hold our output
        snprintf(out, sizeof(out), "sum = %d\n", sum); //this is a safe method to write a string buffer that avoids buffer overflow

        tud_cdc_write_str(out);
        tud_cdc_write_flush();
        set_red_led(false);

        waiting_for_first_op = true;

    }
}


int main(void)
{

    //initialize the pico 
    board_init();

    //intitialize the tinyUSB stack
    tusb_init();

    //intialize Pico's default led pin so that we can use it
    gpio_init(PICO_DEFAULT_LED_PIN);

    //set that pin as an output
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    
    while (true) {
    
        tud_task();

    }

    return 0;
}