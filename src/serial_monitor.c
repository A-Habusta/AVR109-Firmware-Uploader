#include <avr/io.h>
#include <avr/interrupt.h>
#include "serial_monitor.h"
#include "tick_callback.h"
#include "millis.h"
#include "clcd.h"
#include "util.h"
#include "buttons.h"
#include "common.h"

#define ROWS 16
#define COLS DISPLAY_VISIBLE_COLS
#define EMPTY_CHAR ' '

static inline void enable_usart_rx_interrupt() {
    set_bit_inplace(UCSR1B, RXCIE1);
}

static inline void disable_usart_rx_interrupt() {
    clear_bit_inplace(UCSR1B, RXCIE1);
}

static inline void start_usart_receive() {
    enable_usart_rx_interrupt();
    set_bit_inplace(UCSR1B, RXEN1);
}

static inline void stop_usart_receive() {
    clear_bit_inplace(UCSR1B, RXEN1);
    disable_usart_rx_interrupt();
}

static struct {
    char buffer[ROWS][COLS];
    uint8_t buffer_start_row;
    uint8_t buffer_end_row;
    uint8_t col_to_add;
    uint8_t first_displayed_row;
    uint8_t used_rows;
} monitor;

static void flush_row(uint8_t row) {
    for (uint8_t i = 0; i < COLS; i++) {
        monitor.buffer[row][i] = EMPTY_CHAR;
    }
}

static void flush_buffer() {
    disable_usart_rx_interrupt();

    for (uint8_t i = 0; i < ROWS; i++) {
        flush_row(i);
    }

    monitor.buffer_start_row = 0;
    monitor.buffer_end_row = 0;
    monitor.first_displayed_row = 0;
    monitor.used_rows = 1;

    monitor.col_to_add = 0;

    enable_usart_rx_interrupt();
}

static inline uint8_t next_row(uint8_t row) {
    return (row + 1) % ROWS;
}

static inline uint8_t prev_row(uint8_t row) {
    return (row + ROWS - 1) % ROWS;
}

static inline uint8_t add_rows(uint8_t row, uint8_t addend) {
    return (row + addend) % ROWS;
}

static inline uint8_t sub_rows(uint8_t row, uint8_t subtrahend) {
    uint8_t modulus_subtrahend = subtrahend % ROWS;
    return (row + ROWS - modulus_subtrahend) % ROWS;
}

static inline void increment_used_row_counter() {
    if (monitor.used_rows < ROWS) {
        monitor.used_rows++;
    }
}

static inline bool display_can_scroll() {
    return monitor.used_rows > DISPLAY_ROWS;
}

static inline bool buffer_full() {
    return monitor.buffer_start_row == next_row(monitor.buffer_end_row);
}

static inline uint8_t get_first_displayed_row() {
    return monitor.first_displayed_row;
}

static inline uint8_t get_last_displayed_row() {
    return add_rows(get_first_displayed_row(), DISPLAY_ROWS - 1);
}

// If the start is displayed, it must be on the first row, because we can't
// scroll up past the buffer start
static inline bool buffer_start_is_displayed_on_first_row() {
    return monitor.buffer_start_row == get_first_displayed_row();
}

static inline bool buffer_end_is_displayed_on_last_row() {
    return monitor.buffer_end_row == get_last_displayed_row();
}

static inline void shift_display_down() {
    monitor.first_displayed_row = next_row(monitor.first_displayed_row);
}

static inline void shift_display_up() {
    monitor.first_displayed_row = prev_row(monitor.first_displayed_row);
}

static inline void move_buffer_start() {
    monitor.buffer_start_row = next_row(monitor.buffer_start_row);
}

static inline void move_buffer_end() {
    monitor.buffer_end_row = next_row(monitor.buffer_end_row);
}

static void start_new_buffer_line() {
    if (buffer_full()) {
        // If we are looking at the buffer start and we are about to overwrite it,
        // then we should shift the display down
        if (buffer_start_is_displayed_on_first_row()) {
            shift_display_down();
        }

        flush_row(monitor.buffer_start_row);
        move_buffer_start();
    }

    // Autoscroll if we are looking at the last row
    if (buffer_end_is_displayed_on_last_row()) {
        shift_display_down();
    }

    move_buffer_end();
    monitor.col_to_add = 0;

    increment_used_row_counter();
}

static void add_char_to_buffer(char c) {
    if (c == '\r') {
        return;
    }

    if (monitor.col_to_add == COLS) {
        start_new_buffer_line();
    }

    if (c == '\n') {
        monitor.col_to_add = COLS;
        return;
    }

    monitor.buffer[monitor.buffer_end_row][monitor.col_to_add] = c;
    monitor.col_to_add++;
}

static void draw() {
    for (uint8_t i = 0; i < DISPLAY_ROWS; i++) {
        uint8_t current_row = add_rows(monitor.first_displayed_row, i);
        clcd_set_cursor_position(0, i);
        clcd_write_chars(monitor.buffer[current_row], COLS);
    }
}

static void scroll_down() {
    disable_usart_rx_interrupt();
    if (!buffer_end_is_displayed_on_last_row() && display_can_scroll()) {
        shift_display_down();
    }
    enable_usart_rx_interrupt();
}

static void scroll_up() {
    disable_usart_rx_interrupt();
    if (!buffer_start_is_displayed_on_first_row() && display_can_scroll()) {
        shift_display_up();
    }
    enable_usart_rx_interrupt();
}

static void jump_display_to_buffer_end() {
    disable_usart_rx_interrupt();
    monitor.first_displayed_row = sub_rows(monitor.buffer_end_row, DISPLAY_ROWS - 1);
    enable_usart_rx_interrupt();
}

static void cleanup_monitor() {
    stop_usart_receive();
}

static tick_callback_result_t serial_monitor_tick(millis_t _) {
    if (button_was_pressed(BUTTON_UP)) {
        scroll_up();
    } else if (button_was_pressed(BUTTON_SELECT)) {
        jump_display_to_buffer_end();
    } else if (button_was_pressed(BUTTON_DOWN)) {
        scroll_down();
    } else if (button_was_pressed(BUTTON_CUSTOM_ACTION_3)) {
        flush_buffer();
    } else if (button_was_pressed(BUTTON_BACK)) {
        cleanup_monitor();
        return TICK_CALLBACK_FINISHED;
    }

    draw();
    return TICK_CALLBACK_CONTINUE;
}

tick_callback_t switch_to_serial_monitor() {
    clcd_cursor_off();
    start_usart_receive();

    return &serial_monitor_tick;
}

void serial_monitor_init() {
    flush_buffer();
}

ISR(USART1_RX_vect) {
    uint8_t status = UCSR1A;
    char c = UDR1;
    if (bit_is_clear(status, FE) && bit_is_clear(status, DOR) && bit_is_clear(status, UPE)) {
        add_char_to_buffer(c);
    }
}