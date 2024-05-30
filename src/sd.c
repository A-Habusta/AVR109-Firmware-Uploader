#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util.h>
#include "sd.h"
#include "spi.h"
#include "util.h"

#define CS_PORT PORTB
#define CS_DDR DDRB
#define CS_BIT 0

#define SD_CMD_ARG_SIZE 4
#define SD_CMD_BASE 0x40

#define SD_CMD0_INIT_CRC (0x95 >> 1)
#define SD_CMD8_INIT_CRC (0x87 >> 1)

#define SD_READ_START_TOKEN 0xFE

#define SD_HC_CHECK_ARG 0x000001AA

#define SD_RESPONSE_MAX_DELAY_BYTES 10

#define TIMEOUT_OCR 15625

typedef enum {
    CMD0 = 0,
    CMD1 = 1,
    CMD8 = 8,
    CMD9 = 9,
    CMD10 = 10,
    CMD12 = 12,
    CMD16 = 16,
    CMD17 = 17,
    CMD18 = 18,
    CMD55 = 55,
    CMD58 = 58,
    ACMD23 = 23,
    ACMD41 = 41
} sd_command_t;

static struct {
    bool use_block_address;
    bool timed_out;
} sd_status;

static inline uint16_t response_get_extra_size(sd_command_t command) {
    switch (command) {
        case CMD8:
        case CMD58:
            return 4;
        default:
            return 0;
    }
}

typedef struct {
    uint32_t value;
} sd_response_t;

static inline sd_error_t response_get_error(uint8_t response) {
    if (response & 0x80) {
        return SD_ERROR_NO_RESPONSE;
    } else if (response & 0x40) {
        return SD_ERROR_PARAMETER;
    } else if (response & 0x20) {
        return SD_ERROR_ADDRESS_ERROR;
    } else if (response & 0x10) {
        return SD_ERROR_ERASE_SEQUENCE;
    } else if (response & 0x08) {
        return SD_ERROR_COMMAND_CRC;
    } else if (response & 0x04) {
        return SD_ERROR_ILLEGAL_COMMAND;
    } else if (response & 0x02) {
        return SD_ERROR_ERASE_RESET;
    } else if (response & 0x01) {
        return SD_ERROR_IDLE;
    } else {
        return SD_ERROR_OK;
    }
}

static inline bool error_is_benign(sd_error_t error) {
    return (error == SD_ERROR_IDLE) || (error == SD_ERROR_OK);
}

static inline void init_timeout_timer () {
    TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10);
    OCR1A = TIMEOUT_OCR;
}

static inline void enable_timeout_timer() {
    sd_status.timed_out = false;

    TCNT1 = 0;
    // Clear the interrupt flag
    set_bit_inplace(TIFR, OCF1A);

    set_bit_inplace(TIMSK, OCIE1A);
}

static inline void disable_timeout_timer() {
    clear_bit_inplace(TIMSK, OCIE1A);
}

// These functions behave this way to allow using nested calls to sd_cs_select
// and sd_cs_restore
static inline bool sd_cs_select() {
    bool cs_res = get_bit(CS_PORT, CS_BIT);
    clear_bit_inplace(CS_PORT, CS_BIT);
    return cs_res;
}

static inline void sd_cs_restore(bool cs_res) {
    change_bit_inplace(CS_PORT, CS_BIT, cs_res);
}

static inline void set_default_response(sd_response_t *response) {
    if (response != NULLPTR) {
        *response = (sd_response_t) {0};
    }
}

static sd_error_t sd_receive_response(sd_response_t *response, uint16_t response_extra_size) {
    uint8_t response_start = 0xFF;
    uint8_t attempt_count = 0;

    do {
        if(attempt_count > SD_RESPONSE_MAX_DELAY_BYTES) {
            set_default_response(response);
            return SD_ERROR_NO_RESPONSE;
        }

        response_start = spi_master_receive_byte();
        attempt_count++;
    } while (response_start == 0xFF);

    sd_error_t err = response_get_error(response_start);
    if (!error_is_benign(err)) {
        set_default_response(response);
        return err;
    }

    if (response != NULLPTR) {
        response->value = 0;
    }

    if (response_extra_size > 0) {
        spi_master_receive_data_little_endian(response, response_extra_size);
    }

    return err;
}

static inline void sd_send_command_crc(sd_command_t command, uint32_t argument, uint8_t crc) {
    spi_master_transfer(SD_CMD_BASE | command);
    for (int8_t i = SD_CMD_ARG_SIZE - 1; i >= 0; i--) {
        uint8_t arg_byte = (argument >> (i * BITS_IN_BYTE)) & 0xFF;
        spi_master_transfer(arg_byte);
    }
    spi_master_transfer((crc << 1) | 1);
}


static inline sd_error_t sd_send_command_crc_with_response(sd_command_t command, uint32_t argument, uint8_t crc, sd_response_t *response) {
    bool cs_res = sd_cs_select();

    sd_send_command_crc(command, argument, crc);
    sd_error_t err = sd_receive_response(response, response_get_extra_size(command));

    sd_cs_restore(cs_res);

    return err;
}

static inline sd_error_t sd_send_command_with_response(sd_command_t command, uint32_t argument, sd_response_t *response) {
    return sd_send_command_crc_with_response(command, argument, 0x00, response);
}

static sd_error_t check_for_sdv2(bool *sdv2) {
    sd_response_t response;
    sd_error_t err = sd_send_command_crc_with_response(CMD8, SD_HC_CHECK_ARG, SD_CMD8_INIT_CRC, &response);

    if (err == SD_ERROR_IDLE) {
        if ((response.value & 0xFFF) != SD_HC_CHECK_ARG) {
            return SD_ERROR_INVALID_VOLTAGE_RANGE;
        }
        *sdv2 = true;
    } else if (err == SD_ERROR_ILLEGAL_COMMAND || err == SD_ERROR_NO_RESPONSE) {
        *sdv2 = false;
    } else {
        return err;
    }

    return SD_ERROR_OK;
}

static sd_error_t sd_wait_for_initialization(bool hc) {
    enable_timeout_timer();

    while(!sd_status.timed_out) {
        sd_send_command_with_response(CMD55, 0, NULLPTR);
        sd_error_t err = sd_send_command_with_response(ACMD41, (uint32_t)hc << 30, NULLPTR);

        if (err != SD_ERROR_IDLE) {
            return err;
        }
    }

    return SD_ERROR_TIMEOUT;
}

sd_error_t sd_init() {
    init_timeout_timer();

    // Set CS as output
    set_bit_inplace(CS_DDR, CS_BIT);

    spi_restore(true);
    spi_change_settings(SPI_MASTER, SPI_MODE0, SPI_MSBFIRST, SPI_CLOCK_DIV64);

    sd_cs_restore(HIGH);
    // Tick the clock more than 74 times with CS and MOSI high
    spi_master_receive_data(NULLPTR, 10);
    sd_cs_restore(LOW);
    // Tick the clock more than 14 times with CS low and MOSI high
    spi_master_receive_data(NULLPTR, 2);
    sd_cs_restore(HIGH);

    sd_error_t err = sd_send_command_crc_with_response(CMD0, 0, SD_CMD0_INIT_CRC, NULLPTR);
    if (err != SD_ERROR_IDLE) {
        return err;
    }

    bool sdv2 = false;

    err = check_for_sdv2(&sdv2);
    if (err != SD_ERROR_IDLE) {
        return err;
    }

    err = sd_wait_for_initialization(sdv2);
    if (err != SD_ERROR_OK) {
        return err;
    }

    if (sdv2) {
        sd_response_t ocr;
        err = sd_send_command_with_response(CMD58, 0, &ocr);
        if (err != SD_ERROR_OK) {
            return err;
        }

        sd_status.use_block_address = ocr.value & (1UL << 30);
    }

    if (!sd_status.use_block_address) {
        err = sd_send_command_with_response(CMD16, SD_BLOCK_SIZE, NULLPTR);
        if (err != SD_ERROR_OK) {
            return err;
        }
    }

    spi_change_settings(SPI_MASTER, SPI_MODE0, SPI_MSBFIRST, SPI_CLOCK_DIV2);

    return SD_ERROR_OK;
}

sd_error_t sd_read_block(uint8_t *buffer, uint32_t block_number) {
    bool cs_res = sd_cs_select();

    uint32_t block_address = sd_status.use_block_address ? block_number : block_number * SD_BLOCK_SIZE;

    sd_error_t err = sd_send_command_with_response(CMD17, block_address, NULLPTR);
    if (err != SD_ERROR_OK) {
        return err;
    }

    enable_timeout_timer();

    uint8_t response_start = 0xFF;
    do {
        response_start = spi_master_receive_byte();

        if (sd_status.timed_out) {
            sd_cs_restore(cs_res);
            return SD_ERROR_TIMEOUT;
        }
    } while (response_start == 0xFF);


    if (response_start != SD_READ_START_TOKEN) {
        sd_cs_restore(cs_res);
        return SD_ERROR_GENERIC;
    }

    spi_master_receive_data(buffer, SD_BLOCK_SIZE);
    spi_master_receive_data(NULLPTR, 2); // Discard CRC

    sd_cs_restore(cs_res);

    return SD_ERROR_OK;
}

bool sd_is_initialized() {
    sd_response_t ocr;
    sd_error_t err = sd_send_command_with_response(CMD58, 0, &ocr);
    if (err != SD_ERROR_OK) {
        return false;
    }

    // Check if the card is not busy
    return ocr.value & (1UL << 31);
}

void sd_finish() {
    spi_disable();
}

ISR(TIMER1_COMPA_vect) {
    sd_status.timed_out = true;
    disable_timeout_timer();
}