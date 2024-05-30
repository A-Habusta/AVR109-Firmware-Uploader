#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>
#include <millis.h>
#include "fatfs/ff.h"
#include "common.h"
#include "util.h"
#include "buttons.h"
#include "clcd.h"
#include "file_picker.h"

#define DIR_DECORATOR '>'
#define FILE_DECORATOR '*'
#define BACK_BUTTON_DECORATOR '<'
#define BACK_BUTTON_TEXT "- Back"
#define BACK_BUTTON_ROW 0

static const char* fresult_to_string (FRESULT result) {
    switch (result) {
        case FR_OK:
            return "FS OK";
        case FR_DISK_ERR:
            return "Disk Error";
        case FR_INT_ERR:
            return "Internal Error";
        case FR_NOT_READY:
            return "Not Ready";
        case FR_NO_FILE:
            return "No File";
        case FR_NO_PATH:
            return "No Path";
        case FR_INVALID_NAME:
            return "Invalid Name";
        case FR_DENIED:
            return "Denied";
        case FR_EXIST:
            return "Exist";
        case FR_INVALID_OBJECT:
            return "Invalid Object";
        case FR_WRITE_PROTECTED:
            return "Write Protected";
        case FR_INVALID_DRIVE:
            return "Invalid Drive";
        case FR_NOT_ENABLED:
            return "Not Enabled";
        case FR_NO_FILESYSTEM:
            return "No Filesystem";
        case FR_MKFS_ABORTED:
            return "MKFS Aborted";
        case FR_TIMEOUT:
            return "Timed Out";
        case FR_LOCKED:
            return "Locked";
        case FR_NOT_ENOUGH_CORE:
            return "Not Enough Core";
        case FR_TOO_MANY_OPEN_FILES:
            return "Too Many Files";
        case FR_INVALID_PARAMETER:
            return "Invalid Param";
        default:
            return "Unknown";
    }
}

static struct {
    DIR current_directory;
    FIL selected_file;
    uint8_t current_directory_entry_count;
    uint8_t first_displayed_row;
    uint8_t selected_displayed_row;
} state;

static inline uint8_t get_absolute_row(uint8_t display_row) {
    return state.first_displayed_row + display_row;
}

static inline uint8_t get_selected_row() {
    return get_absolute_row(state.selected_displayed_row);
}

static inline uint8_t get_row_count() {
    return state.current_directory_entry_count + 1;
}

static inline bool file_info_is_valid(FILINFO* file_info) {
    return file_info->fname[0] != '\0';
}

static inline bool file_info_is_directory(FILINFO* file_info) {
    return file_info->fattrib & AM_DIR;
}

static inline bool file_is_valid(FIL* file) {
    return file->obj.fs != NULL;
}


static FRESULT get_directory_entry(DIR* dir, uint8_t entry_index, FILINFO* file_info) {
    FRESULT f_err;

    f_rewinddir(dir);

    // Skip to desired entry and then read it (that's why the <= is used here)
    for (uint8_t i = 0; i <= entry_index; i++) {
        f_err = f_readdir(dir, file_info);
        if (f_err != FR_OK) {
            return f_err;
        }

        if (!file_info_is_valid(file_info)) {
            return FR_NO_FILE;
        }
    }

    return FR_OK;
}

static FRESULT get_current_directory_entry(uint8_t entry_index, FILINFO* file_info) {
    return get_directory_entry(&state.current_directory, entry_index, file_info);
}

static FRESULT calculate_directory_entry_count(DIR* dir, uint8_t *entry_count) {
    FRESULT f_err;
    FILINFO file_info;

    uint8_t local_entry_count = 0;

    f_rewinddir(dir);

    while (true) {
        f_err = f_readdir(dir, &file_info);
        if (f_err != FR_OK) {
            *entry_count = 0;
            return f_err;
        }

        if (!file_info_is_valid(&file_info)) {
            break;
        }

        local_entry_count++;
    }

    *entry_count = local_entry_count;
    return FR_OK;
}

static FRESULT move_to_directory(const char* directory_path) {
    FRESULT f_err;

    f_err = f_opendir(&state.current_directory, directory_path);
    if (f_err != FR_OK) {
        return f_err;
    }

    f_err = f_chdir(directory_path);
    if (f_err != FR_OK) {
        return f_err;
    }

    f_err = calculate_directory_entry_count(&state.current_directory, &state.current_directory_entry_count);
    if (f_err != FR_OK) {
        return f_err;
    }

    state.selected_displayed_row = 0;
    state.first_displayed_row = 0;

    return FR_OK;
}

static void set_cursor_to_selected_row() {
    clcd_set_cursor_position(0, state.selected_displayed_row);
}

static void draw_row(uint8_t row, const char* label, char decorator) {
    clcd_set_cursor_position(0, row);
    clcd_write_char(decorator);

    // Truncate the label if it's too long
    clcd_write_chars(label, strnlen(label, DISPLAY_COLS - 1));
}

static FRESULT draw_file_picker_entry(uint8_t row) {
    if (get_absolute_row(row) == BACK_BUTTON_ROW) {
        draw_row(BACK_BUTTON_ROW, BACK_BUTTON_TEXT, BACK_BUTTON_DECORATOR);
        return FR_OK;
    }

    FILINFO file_info;
    FRESULT f_err = get_current_directory_entry(get_absolute_row(row) - 1, &file_info);
    if (f_err != FR_OK) {
        return f_err;
    }

    char decorator = file_info_is_directory(&file_info) ? DIR_DECORATOR : FILE_DECORATOR;

    draw_row(row, file_info.fname, decorator);

    return FR_OK;
}

static FRESULT draw_file_picker() {
    clcd_clear_display();

    for (uint8_t i = 0; i < u8min(DISPLAY_ROWS, get_row_count()); i++) {
        FRESULT f_err = draw_file_picker_entry(i);
        if (f_err != FR_OK) {
            return f_err;
        }
    }

    set_cursor_to_selected_row();

    return FR_OK;
}

static FRESULT scroll_down() {
    if (state.selected_displayed_row < DISPLAY_ROWS - 1) {
        state.selected_displayed_row++;
    } else if (state.first_displayed_row + DISPLAY_ROWS < get_row_count()) {
        state.first_displayed_row++;
    } else {
        return FR_OK;
    }

    return draw_file_picker();
}

static FRESULT scroll_up() {
    if (state.selected_displayed_row > 0) {
        state.selected_displayed_row--;
    } else if (state.first_displayed_row > 0) {
        state.first_displayed_row--;
    } else {
        return FR_OK;
    }

    return draw_file_picker();
}

static FRESULT select_directory(FILINFO *file_info) {
    FRESULT f_err = move_to_directory(file_info->fname);
    if (f_err != FR_OK) {
        return f_err;
    }

    return draw_file_picker();
}

static FRESULT select_file(FILINFO *file_info) {
    // TODO
    return FR_OK;
}

static FRESULT select_back_button() {
    FRESULT f_err = move_to_directory("..");
    if (f_err != FR_OK) {
        return f_err;
    }

    return draw_file_picker();
}

static FRESULT select_option() {
    if(get_selected_row() == BACK_BUTTON_ROW) {
        select_back_button();
    }

    FILINFO file_info;
    FRESULT f_err = get_current_directory_entry(get_selected_row() - 1, &file_info);
    if (f_err != FR_OK) {
        return f_err;
    }

    if (file_info_is_directory(&file_info)) {
        return select_directory(&file_info);
    } else {
        return select_file(&file_info);
    }
}

FRESULT file_picker_tick(tick_callback_result_t *result) {
    *result = TICK_CALLBACK_CONTINUE;

    if (button_was_pressed(BUTTON_UP)) {
        return scroll_up();
    } else if (button_was_pressed(BUTTON_DOWN)) {
        return scroll_down();
    } else if (button_was_pressed(BUTTON_SELECT)) {
        return select_option();
    } else if (button_was_pressed(BUTTON_BACK)) {
        *result = TICK_CALLBACK_FINISHED;
    }

    return FR_OK;
}

FRESULT start_file_picker() {
    FRESULT f_err;

    f_err = move_to_directory("/");
    if (f_err != FR_OK) {
        return f_err;
    }

    clcd_cursor_on();
    draw_file_picker();

    return FR_OK;
}

FIL *file_picker_get_selected_file() {
    FIL *current_file = &state.selected_file;

    if (!file_is_valid(current_file)) {
       return NULL;
    }

    return current_file;
}
