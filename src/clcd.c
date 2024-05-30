#include <stdbool.h>
#include <avr/io.h>
#include <util/delay.h>
#include "clcd.h"
#include "util.h"
#include "common.h"

#define RS_DDR DDRG
#define RS_PORT PORTG
#define RS_BIT PG4

#define EN_DDR DDRD
#define EN_PORT PORTD
#define EN_BIT PD7

#define DATA_DDR DDRG
#define DATA_PORT PORTG
#define DATA_OFFSET PG0

#define SHORT_OPERATION_DELAY_US 53
#define LONG_OPERATION_DELAY_US 3000
#define ENABLE_HOLD_HIGH_TIME_US 0.5

#define INITIALIZATION_INITIAL_DELAY_MS 100
#define INITIALIZATION_LONG_DELAY_US 5000
#define INITIALIZATION_SHORT_DELAY_US 200

#define CLEAR_SCREEN_COMMAND 0x01
#define RETURN_HOME_COMMAND 0x02

#define ENTRY_MODE_SET_BASE 0x04
#define AUTOSCROLL_POS 0
#define INC_DEC_POS 1

#define DISPLAY_CONTROL_BASE 0x08
#define BLINK_ON_OFF_POS 0
#define CURSOR_ON_OFF_POS 1
#define DISPLAY_ON_OFF_POS 2

#define CURSOR_DISPLAY_SHIFT_BASE 0x10
#define RIGHT_LEFT_SELECTOR_POS 2
#define SCREEN_CURSOR_SELECTOR_POS 3

#define FUNCTION_SET_BASE 0x20
#define FONT_SELECTOR_POS 2
#define LINE_NUMBER_POS 3
#define DATA_LENGTH_TOGGLE_POS 4

#define SET_DDRAM_ADDRESS_BASE 0x80

#define LOWER_NIBBLE_MASK 0x0F

#define SECOND_ROW_OFFSET 0x40

static struct {
    clcd_mode_t mode;
    bool initialized;
    uint8_t entry_mode_set;
    uint8_t display_control;
    uint8_t cursor_display_shift;
} clcd_state = {0};

static inline void change_rs(bool rs) {
    change_bit_inplace(RS_PORT, RS_BIT, rs);
}

static inline void change_en(bool en) {
    change_bit_inplace(EN_PORT, EN_BIT, en);
}

static void enable_pulse() {
    change_en(HIGH);
    _delay_us(ENABLE_HOLD_HIGH_TIME_US);
    change_en(LOW);
}

static void send_to_clcd(uint8_t data) {
    DATA_PORT = data;
    enable_pulse();
}

static void send_lower_nibble_to_clcd(uint8_t data) {
    uint8_t existing_data_masked = DATA_PORT & ~LOWER_NIBBLE_MASK;
    send_to_clcd(existing_data_masked | (data & LOWER_NIBBLE_MASK));
}

static void send_8bit(uint8_t data) {
    send_to_clcd(data);
}

static void send_4bit(uint8_t data) {
    uint8_t data_lower_nibble = data & LOWER_NIBBLE_MASK;
    uint8_t data_upper_nibble = (data >> 4) & LOWER_NIBBLE_MASK;

    send_lower_nibble_to_clcd(data_upper_nibble);
    _delay_us(ENABLE_HOLD_HIGH_TIME_US);
    send_lower_nibble_to_clcd(data_lower_nibble);
}

static void send(uint8_t data, bool rs) {
    change_rs(rs);

    if (clcd_state.mode == FOUR_BIT) {
        send_4bit(data);
    } else if (clcd_state.mode == EIGHT_BIT) {
        send_8bit(data);
    }
}

static void send_short_delay(uint8_t data, bool rs) {
    send(data, rs);
    _delay_us(SHORT_OPERATION_DELAY_US);
}

static void send_long_delay(uint8_t data, bool rs) {
    send(data, rs);
    _delay_us(LONG_OPERATION_DELAY_US);
}

static void init_4bit_mode() {
    uint8_t function_set_upper_nibble = FUNCTION_SET_BASE >> 4;

    send_lower_nibble_to_clcd(function_set_upper_nibble | 1);
    _delay_us(INITIALIZATION_LONG_DELAY_US);
    send_lower_nibble_to_clcd(function_set_upper_nibble | 1);
    _delay_us(INITIALIZATION_SHORT_DELAY_US);
    send_lower_nibble_to_clcd(function_set_upper_nibble | 1);
    _delay_us(INITIALIZATION_SHORT_DELAY_US);
    send_lower_nibble_to_clcd(function_set_upper_nibble);
    _delay_us(SHORT_OPERATION_DELAY_US);
}

static void init_8bit_mode() {
    send_to_clcd(FUNCTION_SET_BASE | 1);
    _delay_us(INITIALIZATION_LONG_DELAY_US);
    send_to_clcd(FUNCTION_SET_BASE | 1);
    _delay_us(INITIALIZATION_SHORT_DELAY_US);
    send_to_clcd(FUNCTION_SET_BASE | 1);
    _delay_us(INITIALIZATION_SHORT_DELAY_US);
}

void clcd_init(clcd_mode_t mode, clcd_lines_t lines, clcd_font_t font) {
    if (clcd_state.initialized) {
        return;
    }

    set_bit_inplace(RS_DDR, RS_BIT);
    set_bit_inplace(EN_DDR, EN_BIT);
    DATA_DDR |= (0x0F << DATA_OFFSET);

    change_rs(LOW);
    change_en(LOW);

    clcd_state.mode = mode;
    clcd_state.initialized = true;

    clcd_state.cursor_display_shift = CURSOR_DISPLAY_SHIFT_BASE;
    clcd_state.display_control = DISPLAY_CONTROL_BASE;
    clcd_state.entry_mode_set = ENTRY_MODE_SET_BASE;

    // Necessary delay before initialisation of CLCD
    _delay_ms(INITIALIZATION_INITIAL_DELAY_MS);

    if (mode == FOUR_BIT) {
        init_4bit_mode();
    } else if (mode == EIGHT_BIT) {
        init_8bit_mode();
    }

    uint8_t function_set = FUNCTION_SET_BASE;
    change_bit_inplace(function_set, DATA_LENGTH_TOGGLE_POS, mode);
    change_bit_inplace(function_set, LINE_NUMBER_POS, lines);
    change_bit_inplace(function_set, FONT_SELECTOR_POS, font);

    send_short_delay(function_set, LOW);
    send_short_delay(DISPLAY_CONTROL_BASE, LOW); // Turn everything off

    clcd_clear_display();
    clcd_cursor_set_increment();
    clcd_display_on();
}

void clcd_blink_off() {
    clear_bit_inplace(clcd_state.display_control, BLINK_ON_OFF_POS);
    send_short_delay(clcd_state.display_control, LOW);
}

void clcd_blink_on() {
    set_bit_inplace(clcd_state.display_control, BLINK_ON_OFF_POS);
    send_short_delay(clcd_state.display_control, LOW);
}

void clcd_cursor_off() {
    clear_bit_inplace(clcd_state.display_control, CURSOR_ON_OFF_POS);
    send_short_delay(clcd_state.display_control, LOW);
}

void clcd_cursor_on() {
    set_bit_inplace(clcd_state.display_control, CURSOR_ON_OFF_POS);
    send_short_delay(clcd_state.display_control, LOW);
}

void clcd_display_off() {
    clear_bit_inplace(clcd_state.display_control, DISPLAY_ON_OFF_POS);
    send_short_delay(clcd_state.display_control, LOW);
}

void clcd_display_on() {
    set_bit_inplace(clcd_state.display_control, DISPLAY_ON_OFF_POS);
    send_short_delay(clcd_state.display_control, LOW);
}

void clcd_clear_row(uint8_t row) {
    clcd_set_cursor_position(0, row);
    for (uint8_t i = 0; i < DISPLAY_COLS; i++) {
        clcd_write_char(' ');
    }
}

void clcd_clear_display() {
    send_long_delay(CLEAR_SCREEN_COMMAND, LOW);
}

void clcd_return_home() {
    send_long_delay(RETURN_HOME_COMMAND, LOW);
}

void clcd_cursor_shift_left() {
    clear_bit_inplace(clcd_state.cursor_display_shift, SCREEN_CURSOR_SELECTOR_POS);
    clear_bit_inplace(clcd_state.cursor_display_shift, RIGHT_LEFT_SELECTOR_POS);
    send_short_delay(clcd_state.cursor_display_shift, LOW);
}

void clcd_cursor_shift_right() {
    clear_bit_inplace(clcd_state.cursor_display_shift, SCREEN_CURSOR_SELECTOR_POS);
    set_bit_inplace(clcd_state.cursor_display_shift, RIGHT_LEFT_SELECTOR_POS);
    send_short_delay(clcd_state.cursor_display_shift, LOW);
}

void clcd_display_shift_left() {
    set_bit_inplace(clcd_state.cursor_display_shift, SCREEN_CURSOR_SELECTOR_POS);
    clear_bit_inplace(clcd_state.cursor_display_shift, RIGHT_LEFT_SELECTOR_POS);
    send_short_delay(clcd_state.cursor_display_shift, LOW);
}

void clcd_display_shift_right() {
    set_bit_inplace(clcd_state.cursor_display_shift, SCREEN_CURSOR_SELECTOR_POS);
    set_bit_inplace(clcd_state.cursor_display_shift, RIGHT_LEFT_SELECTOR_POS);
    send_short_delay(clcd_state.cursor_display_shift, LOW);
}

void clcd_cursor_set_increment() {
    set_bit_inplace(clcd_state.entry_mode_set, INC_DEC_POS);
    send_short_delay(clcd_state.entry_mode_set, LOW);
}

void clcd_cursor_set_decrement() {
    clear_bit_inplace(clcd_state.entry_mode_set, INC_DEC_POS);
    send_short_delay(clcd_state.entry_mode_set, LOW);
}

void clcd_auto_scroll_off() {
    clear_bit_inplace(clcd_state.entry_mode_set, AUTOSCROLL_POS);
    send_short_delay(clcd_state.entry_mode_set, LOW);
}

void clcd_auto_scroll_on() {
    set_bit_inplace(clcd_state.entry_mode_set, AUTOSCROLL_POS);
    send_short_delay(clcd_state.entry_mode_set, LOW);
}

void clcd_set_cursor_position(uint8_t col, uint8_t row) {
    uint8_t address = col + (row == 1 ? SECOND_ROW_OFFSET : 0);
    send_short_delay(SET_DDRAM_ADDRESS_BASE | address, LOW);
}

void clcd_write_char(char c) {
    send_short_delay(c, HIGH);
}

void clcd_write_string(const char *str){
    while (*str) {
        clcd_write_char(*str++);
    }
}

void clcd_write_chars(const char *source, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        clcd_write_char(source[i]);
    }
}

void clcd_write_byte(uint8_t byte) {
    clcd_write_nibble(byte >> 4);
    clcd_write_nibble(byte);
}

void clcd_write_nibble(uint8_t byte) {
    static const char characters[] = "0123456789ABCDEF";
    clcd_write_char(characters[byte & 0x0F]);
}