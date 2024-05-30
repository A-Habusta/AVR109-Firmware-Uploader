#ifndef CLCD_H
#define CLCD_H

#include <avr/io.h>
#include <avr/cpufunc.h>

// NOTE: Expects that the R/W pin is grounded

typedef enum {
    FOUR_BIT,
    EIGHT_BIT
} clcd_mode_t;

typedef enum {
    ONE_LINE,
    TWO_LINE
} clcd_lines_t;

typedef enum {
    FONT_5x8,
    FONT_5x10
} clcd_font_t;

void clcd_init(clcd_mode_t mode, clcd_lines_t lines, clcd_font_t font);

void clcd_blink_off(void);
void clcd_blink_on(void);

void clcd_cursor_off(void);
void clcd_cursor_on(void);

void clcd_display_off(void);
void clcd_display_on(void);

void clcd_clear_row(uint8_t row);
void clcd_clear_display(void);
void clcd_return_home(void);

void clcd_cursor_shift_left(void);
void clcd_cursor_shift_right(void);

void clcd_display_shift_left(void);
void clcd_display_shift_right(void);

void clcd_cursor_set_increment(void);
void clcd_cursor_set_decrement(void);

void clcd_auto_scroll_off(void);
void clcd_auto_scroll_on(void);

void clcd_set_cursor_position(uint8_t col, uint8_t row);

void clcd_write_char(char c);
void clcd_write_nibble(uint8_t nibble);
void clcd_write_byte(uint8_t byte);
void clcd_write_string(const char *str);
void clcd_write_chars(const char *source, uint8_t count);

#endif // CLCD_H