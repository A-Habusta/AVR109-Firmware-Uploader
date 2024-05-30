#ifndef TICK_CALLBACK_H
#define TICK_CALLBACK_H

#include <stdbool.h>
#include <millis.h>

typedef enum {
    TICK_CALLBACK_CONTINUE,
    TICK_CALLBACK_FINISHED
} tick_callback_result_t;

typedef tick_callback_result_t (*tick_callback_t)(millis_t current_time);

#endif // TICK_CALLBACK_H