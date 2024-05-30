#ifndef FILE_PICKER_H
#define FILE_PICKER_H

#include "fatfs/ff.h"
#include "tick_callback.h"

// File picker should only be used when the file system is already mounted
FRESULT file_picker_tick(tick_callback_result_t *result);
FRESULT start_file_picker();
FIL *file_picker_get_selected_file();

#endif // FILE_PICKER_H