#include <stdbool.h>
#include <avr/io.h>
#include "clcd.h"
#include "util.h"
#include "buttons.h"
#include "common.h"
#include "main_menu.h"
#include "usart_settings.h"
#include "serial_monitor.h"
#include "tick_callback.h"

typedef struct {
    const char *label;
    tick_callback_t (*action)(void);
} main_menu_option_t;

static struct {
    tick_callback_t current_tick_callback;
    uint8_t selected_displayed_row;
    uint8_t first_displayed_row;
} main_menu;

static inline uint8_t get_actual_selected_row() {
    return main_menu.first_displayed_row + main_menu.selected_displayed_row;
}

static void set_cursor_to_selected_row() {
    clcd_set_cursor_position(0, main_menu.selected_displayed_row);
}

static const main_menu_option_t main_menu_options[] = {
    // TODO
    {"Flash Program", NULLPTR},
    {"Serial Monitor", &switch_to_serial_monitor},
    {"USART Settings", &switch_to_usart_settings},
};
static const uint8_t main_menu_option_count = sizeof(main_menu_options) / sizeof(main_menu_option_t);


static void draw_menu_entry(uint8_t display_row, const char *label) {
    clcd_set_cursor_position(0, display_row);
    clcd_write_char('*');
    clcd_write_string(label);
}

static void draw() {
    clcd_clear_display();

    const uint8_t rows_to_draw = u8min(main_menu_option_count, DISPLAY_ROWS);
    for (uint8_t i = 0; i < rows_to_draw; i++) {
        const uint8_t main_menu_option_index = main_menu.first_displayed_row + i;
        draw_menu_entry(i, main_menu_options[main_menu_option_index].label);
    }

    set_cursor_to_selected_row();
}


static void main_menu_selection_up() {
    if (main_menu.selected_displayed_row > 0) {
        main_menu.selected_displayed_row--;
    } else if (main_menu.first_displayed_row > 0) {
        main_menu.first_displayed_row--;
    }

    draw();
}

static void main_menu_selection_down() {
    if (main_menu.selected_displayed_row < DISPLAY_ROWS - 1) {
        main_menu.selected_displayed_row++;
    } else if (main_menu.first_displayed_row + DISPLAY_ROWS < main_menu_option_count) {
        main_menu.first_displayed_row++;
    }

    draw();
}

static void main_menu_confirm_selection() {
    tick_callback_t new_tick_callback = main_menu_options[get_actual_selected_row()].action();
    if (new_tick_callback != NULLPTR) {
        main_menu.current_tick_callback = new_tick_callback;
    }
}

static tick_callback_result_t selection_tick() {
    if (button_was_pressed(BUTTON_UP)) {
        main_menu_selection_up();
    } else if (button_was_pressed(BUTTON_SELECT)) {
        main_menu_confirm_selection();
    } else if (button_was_pressed(BUTTON_DOWN)) {
        main_menu_selection_down();
    }

    return TICK_CALLBACK_CONTINUE;
}

static const tick_callback_t default_tick_callback  = &selection_tick;

void main_menu_init() {
    main_menu.current_tick_callback = default_tick_callback;
    main_menu.selected_displayed_row = 0;
    main_menu.first_displayed_row = 0;
}

void switch_to_main_menu() {
    clcd_cursor_on();
    clcd_blink_off();
    clcd_cursor_set_increment();
    clcd_return_home();

    main_menu.current_tick_callback = default_tick_callback;

    draw();
}

void main_menu_tick(millis_t current_time) {
    tick_callback_result_t result;
    result = main_menu.current_tick_callback(current_time);

    if (result == TICK_CALLBACK_CONTINUE) {
        return;
    } else if (result == TICK_CALLBACK_FINISHED) {
        switch_to_main_menu();
    }
}