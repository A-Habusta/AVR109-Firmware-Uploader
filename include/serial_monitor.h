#ifndef SERIAL_MONITOR_H
#define SERIAL_MONITOR_H

#include <millis.h>
#include "tick_callback.h"

void serial_monitor_init(void);
tick_callback_t switch_to_serial_monitor(void);

#endif // SERIAL_MONITOR_H