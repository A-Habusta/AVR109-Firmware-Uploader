#include <avr/io.h>
#include "spi.h"
#include "util.h"

#define SPI_PORT PORTB
#define SPI_DDR DDRB

#define CS_BIT 0
#define SCK_BIT 1
#define MOSI_BIT 2
#define MISO_BIT 3

static void settings_master() {
    // Set MOSI, SCK as output
    set_bit_inplace(SPI_DDR, MOSI_BIT);
    set_bit_inplace(SPI_DDR, SCK_BIT);

    set_bit_inplace(SPI_PORT, MOSI_BIT);
    clear_bit_inplace(SPI_PORT, SCK_BIT);

    // Set MISO as input, enable pull-up
    clear_bit_inplace(SPI_DDR, MISO_BIT);
    set_bit_inplace(SPI_PORT, MISO_BIT);
}

static void settings_slave() {
    // Set MOSI, SCK, and CS as input
    clear_bit_inplace(SPI_DDR, MOSI_BIT);
    clear_bit_inplace(SPI_DDR, SCK_BIT);
    clear_bit_inplace(SPI_DDR, CS_BIT);

    // Set MISO as output
    set_bit_inplace(SPI_DDR, MISO_BIT);
}

void spi_change_settings(spi_master_slave_select_t master_slave_select, spi_mode_t mode, spi_bit_order_t bit_order, spi_clock_div_t clock_div) {
    bool spi_res = spi_disable();

    if (master_slave_select == SPI_MASTER) {
        settings_master();
    } else if (master_slave_select == SPI_SLAVE) {
        settings_slave();
    }

    change_bit_inplace(SPCR, MSTR, master_slave_select);
    change_bit_inplace(SPCR, DORD, bit_order);
    change_bit_inplace(SPCR, CPOL, get_bit(mode, 0));
    change_bit_inplace(SPCR, CPHA, get_bit(mode, 1));

    change_bit_inplace(SPCR, SPR0, get_bit(clock_div, 0));
    change_bit_inplace(SPCR, SPR1, get_bit(clock_div, 1));

    change_bit_inplace(SPSR, SPI2X, get_bit(clock_div, 2));

    spi_restore(spi_res);
}