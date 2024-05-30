#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>

#define BUTTONS_COUNT 8

typedef enum {
    BUTTON_UP = 7,
    BUTTON_SELECT = 6,
    BUTTON_DOWN = 5,
    BUTTON_CUSTOM_ACTION_0 = 4,
    BUTTON_CUSTOM_ACTION_1 = 3,
    BUTTON_CUSTOM_ACTION_2 = 2,
    BUTTON_CUSTOM_ACTION_3 = 1,
    BUTTON_BACK = 0
} button_name_t;

void buttons_init(void);
void buttons_poll(void);

bool button_was_pressed(button_name_t button);
bool button_was_released(button_name_t button);

#endif // BUTTONS_H