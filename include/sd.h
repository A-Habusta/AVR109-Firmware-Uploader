#ifndef SD_H
#define SD_H

#include <stdint.h>

#define SD_BLOCK_SIZE 512

typedef enum {
    SD_ERROR_OK = 0,
    SD_ERROR_IDLE,
    SD_ERROR_ERASE_RESET,
    SD_ERROR_ILLEGAL_COMMAND,
    SD_ERROR_COMMAND_CRC,
    SD_ERROR_ERASE_SEQUENCE,
    SD_ERROR_ADDRESS_ERROR,
    SD_ERROR_PARAMETER,
    SD_ERROR_GENERIC,
    SD_ERROR_TIMEOUT,
    SD_ERROR_INVALID_VOLTAGE_RANGE,
    SD_ERROR_NO_RESPONSE
} sd_error_t;

static inline const char *sd_error_to_string(sd_error_t error) {
    switch(error) {
        case SD_ERROR_OK:
            return "Card OK";
        case SD_ERROR_IDLE:
            return "Card is idle";
        case SD_ERROR_ERASE_RESET:
            return "Erase Reset";
        case SD_ERROR_ILLEGAL_COMMAND:
            return "Illegal Command";
        case SD_ERROR_COMMAND_CRC:
            return "Cmd CRC invalid";
        case SD_ERROR_ERASE_SEQUENCE:
            return "Erase Sequence";
        case SD_ERROR_ADDRESS_ERROR:
            return "Address Error";
        case SD_ERROR_PARAMETER:
            return "Param Error";
        case SD_ERROR_TIMEOUT:
            return "Access timed out";
        case SD_ERROR_INVALID_VOLTAGE_RANGE:
            return "Invalid V range";
        case SD_ERROR_NO_RESPONSE:
            return "No response";
        case SD_ERROR_GENERIC:
            return "Generic error";
        default:
            return "Unknown error";
    }
}

sd_error_t sd_init();
sd_error_t sd_read_block(uint8_t *buffer, uint32_t block_number);
bool sd_is_initialized();
void sd_finish();

#endif