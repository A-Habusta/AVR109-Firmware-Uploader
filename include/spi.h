#ifndef SPI_H
#define SPI_H

#include <avr/io.h>
#include <avr/sfr_defs.h>
#include <stdint.h>
#include <stdbool.h>
#include "util.h"

typedef enum {
    SPI_SLAVE = 0,
    SPI_MASTER = 1
} spi_master_slave_select_t;

typedef enum {
    SPI_MODE0 = 0b00,
    SPI_MODE1 = 0b01,
    SPI_MODE2 = 0b10,
    SPI_MODE3 = 0b11
} spi_mode_t;

typedef enum {
    SPI_MSBFIRST = 0,
    SPI_LSBFIRST = 1
} spi_bit_order_t;

typedef enum {
    SPI_CLOCK_DIV2 = 0b100,
    SPI_CLOCK_DIV4 = 0b000,
    SPI_CLOCK_DIV8 = 0b101,
    SPI_CLOCK_DIV16 = 0b001,
    SPI_CLOCK_DIV32 = 0b110,
    SPI_CLOCK_DIV64 = 0b010,
    SPI_CLOCK_DIV128 = 0b011,
} spi_clock_div_t;

void spi_change_settings(spi_master_slave_select_t master_slave_select, spi_mode_t mode, spi_bit_order_t bit_order, spi_clock_div_t clock_div);

static inline uint8_t spi_master_transfer(uint8_t byte) {
    SPDR = byte;
    loop_until_bit_is_set(SPSR, SPIF);
    return SPDR;
}

static inline void spi_master_send_byte(uint8_t byte) {
    spi_master_transfer(byte);
}

static inline uint8_t spi_master_receive_byte() {
   return spi_master_transfer(0xFF);
}

static inline void spi_master_send_data(const void *data, uint16_t length) {
    if (data == NULLPTR) {
        return;
    }

    uint8_t *data_bytes = (uint8_t*)data;

    for (uint16_t i = 0; i < length; i++) {
        spi_master_send_byte(data_bytes[i]);
    }
}

static inline void spi_master_receive_data(void *data, uint16_t length) {
    uint8_t *data_bytes = (uint8_t*)data;

    for (uint16_t i = 0; i < length; i++) {
        uint8_t byte = spi_master_receive_byte();
        if (data != NULLPTR) {
            data_bytes[i] = byte;
        }
    }
}

static inline void spi_master_receive_data_little_endian(void *data, uint16_t length) {
    uint8_t *data_bytes = (uint8_t*)data;

    for (uint16_t i = 0; i < length; i++) {
        uint8_t byte = spi_master_receive_byte();
        if (data != NULLPTR) {
            data_bytes[length - i - 1] = byte;
        }
    }
}

static inline bool spi_disable() {
    bool spi_enabled = get_bit(SPCR, SPE);
    clear_bit_inplace(SPCR, SPE);
    return spi_enabled;
}

static inline void spi_restore(bool spi_enabled) {
    change_bit_inplace(SPCR, SPE, spi_enabled);
}

#endif // SPI_H