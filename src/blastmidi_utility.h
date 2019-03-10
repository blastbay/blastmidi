/*
*          BlastMidi
*
* A library of routines for working with Midi files.
*
*          Copyright Blastbay Studios (Philip Bennefall) 2014 - 2015.
* Distributed under the Boost Software License, Version 1.0.
*    (See accompanying file LICENSE_1_0.txt or copy at
*          http://www.boost.org/LICENSE_1_0.txt)
*
*          blastmidi_utility.h
* A collection of utility functions for endian and bit management.
*/

#ifndef BLASTMIDI_UTILITY_H
#define BLASTMIDI_UTILITY_H

#include <stdint.h>
#include <assert.h>

/*
*          int is_little_endian ();
* This returns nonzero if we are running on a little endian platform.
*/
int is_little_endian ()
{
    int16_t i = 1;
    return ( int ) * ( ( uint8_t* ) &i ) == 1;
}

/*
*          uint32_t extract_bits_32(uint32_t value, uint8_t a, uint8_t b);
* Extract bits from a 32 bit number.
* a is the starting bit and b is the ending bit to extract from the whole.
* a and b can range from 1 to 32.
* a must always be smaller than or equal to b.
* 1 is the most significant bit, and 32 is the least significant.
* The result is shifted right so that the least significant extracted bit ends up as the least significant one in the new number.
*/
uint32_t extract_bits_32 ( uint32_t value, uint8_t a, uint8_t b )
{

    assert ( a <= b );
    assert ( a >= 1 && a <= 32 && b >= 1 && b <= 32 );

    value = value << ( a - 1 );
    value = value >> ( 31 - ( b - a ) );
    return value;
}

/*
*          uint32_t swap_32(uint32_t x);
* Swap the bytes of a 32 bit integer.
*/
uint32_t swap_32 ( uint32_t x )
{
    x = ( x & 0x0000FFFF ) << 16 | ( x & 0xFFFF0000 ) >> 16;
    return ( x & 0x00FF00FF ) << 8 | ( x & 0xFF00FF00 ) >> 8;
}

/*
*          uint16_t extract_bits_16(uint16_t value, uint8_t a, uint8_t b);
* Extract bits from a 16 bit number.
* a is the starting bit and b is the ending bit to extract from the whole.
* a and b can range from 1 to 16.
* a must always be smaller than or equal to b.
* 1 is the most significant bit, and 16 is the least significant.
* The result is shifted right so that the least significant extracted bit ends up as the least significant one in the new number.
*/
uint16_t extract_bits_16 ( uint16_t value, uint8_t a, uint8_t b )
{

    assert ( a <= b );
    assert ( a >= 1 && a <= 16 && b >= 1 && b <= 16 );

    value = value << ( a - 1 );
    value = value >> ( 15 - ( b - a ) );
    return value;
}

/*
*          uint16_t swap_16(uint16_t x);
* Swap the bytes of a 16 bit integer.
*/
uint16_t swap_16 ( uint16_t x )
{
    return ( ( x & 0xff ) << 8 ) | ( ( x & 0xff00 ) >> 8 );
}

/*
*          uint8_t extract_bits_8(uint8_t value, uint8_t a, uint8_t b);
* Extract bits from an 8 bit number.
* a is the starting bit and b is the ending bit to extract from the whole.
* a and b can range from 1 to 8.
* a must always be smaller than or equal to b.
* 1 is the most significant bit, and 8 is the least significant.
* The result is shifted right so that the least significant extracted bit ends up as the least significant one in the new number.
*/
uint8_t extract_bits_8 ( uint8_t value, uint8_t a, uint8_t b )
{

    assert ( a <= b );
    assert ( a >= 1 && a <= 8 && b >= 1 && b <= 8 );

    value = value << ( a - 1 );
    value = value >> ( 7 - ( b - a ) );
    return value;
}

/*
*          uint32_t convert_endian_32(uint32_t x, int8_t endian_flag);
* Midi files are always big endian, so if this is a little endian machine we have to swap.
* This version works on a 32 bit integer.
* The endian flag is 1 if the host is little endian and 2 if it is not (big endian is then assumed). Any other value is an error.
*/
uint32_t convert_endian_32 ( uint32_t x, int8_t endian_flag )
{
    assert ( endian_flag >= 1 && endian_flag <= 2 );
    if ( endian_flag == 1 )
    {
        return swap_32 ( x );
    }
    return x;
}

/*
*          uint16_t convert_endian_16(uint16_t x, int8_t endian_flag);
* Midi files are always big endian, so if this is a little endian machine we have to swap.
* This version works on a 16 bit integer.
* The endian flag is 1 if the host is little endian and 2 if it is not (big endian is then assumed). Any other value is an error.
*/
uint16_t convert_endian_16 ( uint16_t x, int8_t endian_flag )
{
    assert ( endian_flag >= 1 && endian_flag <= 2 );
    if ( endian_flag == 1 )
    {
        return swap_16 ( x );
    }
    return x;
}

#endif /* BLASTMIDI_UTILITY_H */
