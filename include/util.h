#ifndef UTIL_H
#define UTIL_H

#include <avr/common.h>
#include <avr/sfr_defs.h>
#include <stdbool.h>
#include <stddef.h>

#define BITS_IN_BYTE 8

#define HIGH true
#define LOW false

#define NULLPTR NULL

#define u8max(a, b) ((a) > (b) ? (a) : (b))
#define u8min(a, b) ((a) < (b) ? (a) : (b))

#define set_bit(byte, bit) ((byte) | _BV(bit))
#define clear_bit(byte, bit) ((byte) & ~_BV(bit))
#define change_bit(byte, bit, value) (clear_bit(byte, bit) | ((value) << (bit)))

#define set_bit_inplace(byte, bit) ((byte) = set_bit(byte, bit))
#define clear_bit_inplace(byte, bit) ((byte) = clear_bit(byte, bit))
#define change_bit_inplace(byte, bit, value) ((byte) = change_bit(byte, bit, value))

#define get_bit(byte, bit) (((byte) >> (bit)) & 1)

#endif // UTIL_H