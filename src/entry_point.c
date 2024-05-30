#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <millis.h>
#include "clcd.h"
#include "util.h"
#include "buttons.h"
#include "main_menu.h"
#include "usart_settings.h"
#include "serial_monitor.h"
#include "file_picker.h"

#define LOOP_RATE 30
#define LOOP_INTERVAL (1000 / LOOP_RATE)

static millis_t last_tick_time;

static void setup() {
    clcd_init(FOUR_BIT, TWO_LINE, FONT_5x8);
    buttons_init();
    main_menu_init();
    serial_monitor_init();
    usart_settings_init();
    millis_init();
    sei();

    last_tick_time = millis();

    switch_to_main_menu();
}

static void loop() {
    millis_t current_time = millis();

    if (current_time - last_tick_time >= LOOP_INTERVAL) {
        last_tick_time += LOOP_INTERVAL;

        buttons_poll();
        main_menu_tick(current_time);
    }
}

int main() {
    setup();

    while (1) {
        loop();
    }
}