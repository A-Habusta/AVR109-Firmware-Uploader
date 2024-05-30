#include "usart_settings.h"

#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include "tick_callback.h"
#include "clcd.h"
#include "buttons.h"
#include "util.h"

typedef enum {
    SETTING_BAUD_RATE = 0,
    SETTING_DATA_BITS,
    SETTING_STOP_BITS,
    SETTING_PARITY
} button_t;

// For our values, using U2X = 1 is always more accurate
#define UBBR_VALUE(baud) ((F_CPU / (8UL * baud)) - 1)

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#define SETTING_GROUPS_COUNT 4

#define EEPROM_SAVE_ADDR ((void*)0x1000)

typedef uint16_t magic_t;
static const magic_t MAGIC = 0xBEEF;

typedef struct {
    uint16_t value;
    const char *label;
} usart_setting_t;

typedef struct {
    const char *name;
    const usart_setting_t *settings;
    uint8_t count;
} usart_settings_group_t;

typedef struct {
    magic_t magic;
    uint8_t group_indices[SETTING_GROUPS_COUNT];
} selected_settings_t;

static const usart_setting_t usart_baud_settings[] = {
    {UBBR_VALUE(2400), "2400"},
    {UBBR_VALUE(4800), "4800"},
    {UBBR_VALUE(9600), "9600"},
    {UBBR_VALUE(14400), "14400"},
    {UBBR_VALUE(19200), "19200"},
    {UBBR_VALUE(28800), "28800"},
    {UBBR_VALUE(38400), "38400"},
    {UBBR_VALUE(57600), "57600"},
    {UBBR_VALUE(76800), "76800"},
    {UBBR_VALUE(115200), "115200"},
};

static const usart_setting_t usart_data_bits_settings[] = {
    {0b000, "5"},
    {0b001, "6"},
    {0b010, "7"},
    {0b011, "8"},
};

static const usart_setting_t usart_stop_bits_settings[] = {
    {0b0, "1"},
    {0b1, "2"},
};

static const usart_setting_t usart_parity_settings[] = {
    {0b00, "None"},
    {0b10, "Even"},
    {0b11, "Odd"},
};

static const usart_settings_group_t usart_settings_groups[SETTING_GROUPS_COUNT] = {
    [SETTING_BAUD_RATE] = {"Baud Rate", usart_baud_settings, ARRAY_SIZE(usart_baud_settings)},
    [SETTING_DATA_BITS] = {"Data Bits", usart_data_bits_settings, ARRAY_SIZE(usart_data_bits_settings)},
    [SETTING_STOP_BITS] = {"Stop Bits", usart_stop_bits_settings, ARRAY_SIZE(usart_stop_bits_settings)},
    [SETTING_PARITY] = {"Parity", usart_parity_settings, ARRAY_SIZE(usart_parity_settings)}
};

static struct {
    uint8_t current_group_index;
    selected_settings_t selected_settings;
} settings_state;

static void save_settings_to_eeprom() {
    cli();
    eeprom_update_block(&settings_state.selected_settings, EEPROM_SAVE_ADDR, sizeof(settings_state.selected_settings));
    sei();
}

static void try_load_settings_from_eeprom() {
    selected_settings_t loaded_settings;

    cli();
    eeprom_read_block(&loaded_settings, EEPROM_SAVE_ADDR, sizeof(loaded_settings));
    sei();

    if (loaded_settings.magic == MAGIC) {
        settings_state.selected_settings = loaded_settings;
    }
}

static const uint8_t get_current_group_index() {
    return settings_state.current_group_index;
}

static uint8_t *get_all_selected_setting_indices() {
    return settings_state.selected_settings.group_indices;
}

static inline const usart_settings_group_t *get_settings_group(uint8_t index) {
    return &usart_settings_groups[index];
}

static inline const usart_settings_group_t *get_current_settings_group() {
    return get_settings_group(get_current_group_index());
}

static inline const usart_setting_t *get_setting(uint8_t group_index, uint8_t setting_index) {
    return &get_settings_group(group_index)->settings[setting_index];
}

static inline uint8_t get_selected_setting_index_for_group(uint8_t group_index) {
    return get_all_selected_setting_indices()[group_index];
}

static inline uint8_t get_selected_setting_index_for_current_group() {
    return get_selected_setting_index_for_group(get_current_group_index());
}

static inline void set_selected_setting_index_for_group(uint8_t group_index, uint8_t new_index) {
    get_all_selected_setting_indices()[group_index] = new_index;
}

static inline void set_selected_setting_index_for_current_group(uint8_t new_index) {
    set_selected_setting_index_for_group(get_current_group_index(), new_index);
}

static inline const usart_setting_t *get_selected_setting_for_group(uint8_t group_index) {
    return get_setting(group_index, get_selected_setting_index_for_group(group_index));
}

static inline const usart_setting_t *get_selected_setting() {
    return get_selected_setting_for_group(get_current_group_index());
}

static void draw() {
    const usart_settings_group_t *group = get_current_settings_group();
    const usart_setting_t *setting = get_selected_setting();

    clcd_clear_display();
    clcd_return_home();

    clcd_write_string("Select ");
    clcd_write_string(group->name);

    clcd_set_cursor_position(0, 1);
    clcd_write_string("> ");
    clcd_write_string(setting->label);
}

static void selection_up() {
    const usart_settings_group_t *group = get_current_settings_group();
    uint8_t current_index = get_selected_setting_index_for_current_group();
    uint8_t new_index = (current_index + 1) % group->count;

    set_selected_setting_index_for_current_group(new_index);
}

static void selection_down() {
    const usart_settings_group_t *group = get_current_settings_group();
    uint8_t current_index = get_selected_setting_index_for_current_group();
    uint8_t new_index = (current_index + group->count - 1) % group->count;

    set_selected_setting_index_for_current_group(new_index);
}

static void commit_settings() {
    const usart_setting_t *baud_setting = get_selected_setting_for_group(SETTING_BAUD_RATE);
    const usart_setting_t *data_bits_setting = get_selected_setting_for_group(SETTING_DATA_BITS);
    const usart_setting_t *stop_bits_setting = get_selected_setting_for_group(SETTING_STOP_BITS);
    const usart_setting_t *parity_setting = get_selected_setting_for_group(SETTING_PARITY);

    cli();

    UBRR1H = (uint8_t)(baud_setting->value >> 8);
    UBRR1L = (uint8_t)(baud_setting->value);

    change_bit_inplace(UCSR1C, UCSZ10, get_bit(data_bits_setting->value, 0));
    change_bit_inplace(UCSR1C, UCSZ11, get_bit(data_bits_setting->value, 1));

    change_bit_inplace(UCSR1B, UCSZ12, get_bit(data_bits_setting->value, 2));

    change_bit_inplace(UCSR1C, USBS1, stop_bits_setting->value);
    change_bit_inplace(UCSR1C, UPM10, get_bit(parity_setting->value, 0));
    change_bit_inplace(UCSR1C, UPM11, get_bit(parity_setting->value, 1));

    sei();

}

static void cleanup_settings() {
    save_settings_to_eeprom();
    commit_settings();
}

static void next_settings_group() {
    settings_state.current_group_index++;
}

static tick_callback_result_t usart_settings_tick(millis_t _) {
    if (button_was_pressed(BUTTON_UP)) {
        selection_up();
    } else if (button_was_pressed(BUTTON_DOWN)) {
        selection_down();
    } else if (button_was_pressed(BUTTON_SELECT)) {
        next_settings_group();
    } else {
        return TICK_CALLBACK_CONTINUE;
    }

    if (get_current_group_index() >= SETTING_GROUPS_COUNT) {
        cleanup_settings();
        return TICK_CALLBACK_FINISHED;
    }

    draw();
    return TICK_CALLBACK_CONTINUE;
}

tick_callback_t switch_to_usart_settings() {
    settings_state.current_group_index = 0;
    clcd_cursor_off();
    draw();

    return &usart_settings_tick;
}

void usart_settings_init() {
    // We always use double speed asynchronous mode
    set_bit_inplace(UCSR0A, U2X0);
    set_bit_inplace(UCSR1A, U2X1);

    settings_state.selected_settings.magic = MAGIC;
    try_load_settings_from_eeprom();
    commit_settings();
}