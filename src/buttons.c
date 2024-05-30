#include <avr/io.h>
#include "buttons.h"
#include "util.h"

#define BUTTON_PIN PINC
#define BUTTON_PORT PORTC
#define BUTTON_DDR DDRC

static uint8_t last_button_state = 0xFF;
static uint8_t current_button_state = 0xFF;

void buttons_init(void) {
    BUTTON_DDR = 0x00;
    BUTTON_PORT = 0xFF;
}

void buttons_poll(void) {
    last_button_state = current_button_state;
    current_button_state = BUTTON_PIN;
}

bool button_was_pressed(button_name_t button) {
    return get_bit(last_button_state, button) && !get_bit(current_button_state, button);
}

bool button_was_released(button_name_t button) {
    return !get_bit(last_button_state, button) && get_bit(current_button_state, button);
}