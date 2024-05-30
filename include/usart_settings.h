#ifndef USART_SETTINGS_H
#define USART_SETTINGS_H

#include <millis.h>
#include "tick_callback.h"

void usart_settings_init(void);
tick_callback_t switch_to_usart_settings(void);

#endif // USART_SETTINGS_H