//#define BLASTMIDI_DEBUG
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
*          blastmidi.c
* The main library implementation file.
*
* For the API reference, see blastmidi.h.
*/

#include <stdlib.h> /* For malloc and free */
#include <math.h>
#include <string.h> /* For memcpy, memset and strcmp */
#include <assert.h>
#include "blastmidi_utility.h" /* Utility functions for bit and endian manipulation */
#include "blastmidi.h"
#include <stdio.h>

void reset ( blastmidi* instance )
{
    if ( instance->tracks )
    {
        size_t i;
        for ( i = 0; i < instance->track_count; ++i )
        {
            blastmidi_whipe_track ( instance, i );
        }
        instance->free_function ( instance->tracks );
        instance->tracks = NULL;
    }
    if ( instance->track_ends )
    {
        instance->free_function ( instance->track_ends );
        instance->track_ends = NULL;
    }
    instance->track_count = 0;
    instance->file_type = 0;
    instance->time_type = 0;
    instance->ticks_per_beat = 0;
    instance->SMPTE_frames = 0;
    instance->ticks_per_frame = 0;
    instance->valid = 0;
    instance->cursor = 0;
    instance->running_status = 0;
    instance->sysex_continuation = 0;
}

uint8_t blastmidi_initialize ( blastmidi* instance, blastmidi_custom_malloc* user_malloc, blastmidi_custom_free* user_free )
{

    /*
    * Zero out the entire structure.
    */
    memset ( ( void* ) instance, 0, sizeof ( blastmidi ) );

    /*
    * Check what the host platform is using (big or little endian), so that we can swap bytes later as needed.
    */
    if ( is_little_endian() )
    {
        instance->endian_flag = 1;
    }
    else
    {
        /*
        * This is not little endian, so we assume big endian in this scenario.
        */
        instance->endian_flag = 2;
    }

    /*
    * Did we get bad input for our memory function pointers?
    */
    if ( ( user_malloc || user_free ) && ( user_malloc == NULL || user_free == NULL ) )
    {
        return BLASTMIDI_INVALIDPARAM;
    }

    if ( user_malloc == NULL && user_free == NULL )
    {
        instance->malloc_function = malloc;
        instance->free_function = free;
    }
    else
    {
        instance->malloc_function = user_malloc;
        instance->free_function = user_free;
    }
    return BLASTMIDI_OK;
}

void blastmidi_set_data_callback ( blastmidi* instance, blastmidi_data_callback* callback, void* user_data )
{
    instance->data_callback = callback;
    instance->data_callback_data = user_data;
}

/*
* The following I/O functions return an error code.
*/
uint8_t read_bytes ( blastmidi* instance, uint8_t* buffer, size_t size )
{
    if ( instance->data_callback ( BLASTMIDI_CALLBACK_READ, size, buffer, instance->data_callback_data ) )
    {
        instance->cursor += size;
        return BLASTMIDI_OK;
    }
    return BLASTMIDI_UNEXPECTEDEND;
}

uint8_t skip_ahead ( blastmidi* instance, size_t size )
{
    if ( instance->data_callback ( BLASTMIDI_CALLBACK_SEEK, instance->cursor + size, NULL, instance->data_callback_data ) )
    {
        instance->cursor += size;
        return BLASTMIDI_OK;
    }
    return BLASTMIDI_UNEXPECTEDEND;
}

uint8_t skip_backwards ( blastmidi* instance, size_t size )
{
    assert ( instance->cursor >= size );
    if ( instance->data_callback ( BLASTMIDI_CALLBACK_SEEK, instance->cursor - size, NULL, instance->data_callback_data ) )
    {
        instance->cursor -= size;
        return BLASTMIDI_OK;
    }
    return BLASTMIDI_UNEXPECTEDEND;
}

uint8_t write_bytes ( blastmidi* instance, uint8_t* buffer, size_t size )
{
    if ( instance->data_callback ( BLASTMIDI_CALLBACK_WRITE, size, buffer, instance->data_callback_data ) )
    {
        return BLASTMIDI_OK;
    }
    return BLASTMIDI_UNEXPECTEDEND;
}

uint8_t read_byte ( blastmidi* instance, uint8_t* buffer )
{
    return read_bytes ( instance, buffer, 1 );
}

/*
* The following integer I/O functions convert to and from big endian as required.
*/
uint8_t read_16_bit ( blastmidi* instance, uint16_t* buffer )
{
    if ( read_bytes ( instance, ( uint8_t* ) buffer, 2 ) == BLASTMIDI_OK )
    {
        *buffer = convert_endian_16 ( *buffer, instance->endian_flag );
        return BLASTMIDI_OK;
    }
    return BLASTMIDI_UNEXPECTEDEND;
}

uint8_t read_32_bit ( blastmidi* instance, uint32_t* buffer )
{
    if ( read_bytes ( instance, ( uint8_t* ) buffer, 4 ) == BLASTMIDI_OK )
    {
        *buffer = convert_endian_32 ( *buffer, instance->endian_flag );
        return BLASTMIDI_OK;
    }
    return BLASTMIDI_UNEXPECTEDEND;
}

uint8_t read_24_bit ( blastmidi* instance, uint32_t* buffer )
{
    if ( read_bytes ( instance, ( uint8_t* ) buffer, 3 ) == BLASTMIDI_OK )
    {
        *buffer = convert_endian_32 ( *buffer, instance->endian_flag );
        *buffer = extract_bits_32 ( *buffer, 1, 24 );
        return BLASTMIDI_OK;
    }
    return BLASTMIDI_UNEXPECTEDEND;
}

uint8_t read_variable_number ( blastmidi* instance, uint32_t* buffer )
{
    uint8_t i;

    *buffer = 0;

    for ( i = 0; i < 4; i++ )
    {

        uint8_t keep_going = 0;
        uint8_t current;

        uint8_t result = read_byte ( instance, &current );
        if ( result != BLASTMIDI_OK )
        {
            return result;
        }
        keep_going = extract_bits_8 ( current, 1, 1 );
        if ( keep_going == 1 && i == 3 )
        {
            return BLASTMIDI_INVALIDCHUNK;
        }
        *buffer = ( ( *buffer ) << 7 ) | ( current & 0x7f );
        if ( !keep_going )
        {
            break;
        }
    }
    return BLASTMIDI_OK;
}

uint8_t write_16_bit ( blastmidi* instance, uint16_t value )
{
    uint16_t buffer = convert_endian_16 ( value, instance->endian_flag );
    if ( write_bytes ( instance, ( uint8_t* ) &buffer, 2 ) == BLASTMIDI_OK )
    {
        return BLASTMIDI_OK;
    }
    return BLASTMIDI_WRITINGFAILED;
}

uint8_t write_32_bit ( blastmidi* instance, uint32_t value )
{
    uint32_t buffer = convert_endian_32 ( value, instance->endian_flag );
    if ( write_bytes ( instance, ( uint8_t* ) &buffer, 4 ) == BLASTMIDI_OK )
    {
        return BLASTMIDI_OK;
    }
    return BLASTMIDI_WRITINGFAILED;
}

uint8_t allocate_tracks ( blastmidi* instance )
{

    assert ( instance->track_count > 0 );
    assert ( instance->tracks == NULL );
    assert ( instance->track_ends == NULL );

    instance->tracks = ( blastmidi_event** ) instance->malloc_function ( ( sizeof ( blastmidi_event* ) * instance->track_count ) );
    if ( instance->tracks == NULL )
    {
        reset ( instance );
        return BLASTMIDI_OUTOFMEMORY;
    }
    memset ( ( void* ) instance->tracks, 0, sizeof ( blastmidi_event* ) *instance->track_count );
    instance->track_ends = ( blastmidi_event** ) instance->malloc_function ( ( sizeof ( blastmidi_event* ) * instance->track_count ) );
    if ( instance->track_ends == NULL )
    {
        reset ( instance );
        return BLASTMIDI_OUTOFMEMORY;
    }
    memset ( ( void* ) instance->track_ends, 0, sizeof ( blastmidi_event* ) *instance->track_count );
    return BLASTMIDI_OK;
}

uint8_t allocate_event ( blastmidi* instance, uint8_t type, uint8_t subtype, uint8_t* data, unsigned int data_size, blastmidi_event** event )
{

    /*
    * This function allocates the memory needed for a Midi event.
    * If data is not NULL, its content will be copied into the event's internal storage.
    * If data is NULL, the memory to hold the event data is allocated but left uninitialized.
    */

    blastmidi_event* output = NULL;
    *event = NULL;

    output = ( blastmidi_event* ) instance->malloc_function ( sizeof ( blastmidi_event ) );
    if ( output == NULL )
    {
        return BLASTMIDI_OUTOFMEMORY;
    }
    memset ( ( void* ) output, 0, sizeof ( blastmidi_event ) );
    output->track = -1;
    output->time = 0;
    output->type = type;
    output->subtype = subtype;
    output->channel = -1;
    output->data = NULL;
    output->data_size = data_size;
    output->end_of_sysex = 0;
    output->previous = NULL;
    output->next = NULL;
    if ( data_size > 0 && data_size <= sizeof ( output->small_pool ) )
    {
        output->data = output->small_pool;
    }
    else
    {
        uint8_t* data_block = ( uint8_t* ) instance->malloc_function ( data_size );
        if ( data_block == NULL )
        {
            instance->free_function ( output );
            return BLASTMIDI_OUTOFMEMORY;
        }
        output->data = data_block;
    }

    if ( data )
    {
        memcpy ( output->data, data, data_size );
    }

    *event = output;
    return BLASTMIDI_OK;
}

uint8_t blastmidi_event_create_channel_event ( blastmidi* instance, uint8_t channel, uint8_t subtype, uint8_t param_1, uint8_t param_2, blastmidi_event** event )
{
    blastmidi_event* output = NULL;
    uint8_t result = 0;
    uint8_t data[2];
    unsigned int data_size = 0;
    uint8_t* temp_ptr8 = NULL;
    uint16_t temp16 = 0;
    switch ( subtype )
    {
        case BLASTMIDI_CHANNEL_NOTE_OFF:
        case BLASTMIDI_CHANNEL_NOTE_ON:
        case BLASTMIDI_CHANNEL_NOTE_AFTERTOUCH:
        case BLASTMIDI_CHANNEL_CONTROLLER:
            data_size = 2;
            data[0] = param_1;
            data[1] = param_2;
            break;
        case BLASTMIDI_CHANNEL_PITCH_BEND:
            /*
            * Extract the last 7 bits of both data bytes and combine them into an unsigned 16 bit integer.
            * The resulting integer will be in the native host endian order.
            */
            data_size = 2;
            param_1 = extract_bits_8 ( param_1, 2, 8 );
            param_2 = extract_bits_8 ( param_2, 2, 8 );
            temp16 = param_1;
            temp16 = temp16 << 8;
            temp16 |= ( param_2 << 1 );
            temp16 = temp16 >> 1;
            temp_ptr8 = ( uint8_t* ) &temp16;
            data[0] = temp_ptr8[0];
            data[1] = temp_ptr8[1];
            break;
        case BLASTMIDI_CHANNEL_PROGRAM_CHANGE:
        case BLASTMIDI_CHANNEL_CHANNEL_AFTERTOUCH:
            data_size = 1;
            data[0] = param_1;
            break;
        default:
            return BLASTMIDI_INVALIDPARAM;
    };

    result = allocate_event ( instance, BLASTMIDI_CHANNEL_EVENT, subtype, data, data_size, &output );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }
    output->channel = channel;
    *event = output;
    return BLASTMIDI_OK;
}

uint8_t blastmidi_event_create_meta_sequence_number_event ( blastmidi* instance, uint16_t sequence_number, blastmidi_event** event )
{
    uint8_t result = 0;
    blastmidi_event* output = NULL;

    assert ( sizeof ( sequence_number ) == 2 );

    result = allocate_event ( instance, BLASTMIDI_META_EVENT, BLASTMIDI_META_SEQUENCE_NUMBER, ( uint8_t* ) &sequence_number, sizeof ( sequence_number ), &output );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }
    *event = output;
    return BLASTMIDI_OK;
}

uint8_t blastmidi_event_create_meta_tempo_event ( blastmidi* instance, uint32_t tempo, blastmidi_event** event )
{
    blastmidi_event* output = NULL;
    uint8_t result = 0;

    assert ( sizeof ( tempo ) == 4 );

    result = allocate_event ( instance, BLASTMIDI_META_EVENT, BLASTMIDI_META_SET_TEMPO, ( uint8_t* ) &tempo, sizeof ( tempo ), &output );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }
    *event = output;
    return BLASTMIDI_OK;
}

uint8_t blastmidi_event_create_meta_data_event ( blastmidi* instance, uint8_t subtype, uint8_t* data, unsigned int data_size, blastmidi_event** event )
{
    blastmidi_event* output = NULL;
    uint8_t result = 0;
    switch ( subtype )
    {
        case BLASTMIDI_META_TEXT:
        case BLASTMIDI_META_COPYRIGHT_NOTICE:
        case BLASTMIDI_META_SEQUENCE_OR_TRACK_NAME:
        case BLASTMIDI_META_INSTRUMENT_NAME:
        case BLASTMIDI_META_LYRICS:
        case BLASTMIDI_META_MARKER:
        case BLASTMIDI_META_CUE_POINT:
        case BLASTMIDI_META_SEQUENCER_SPECIFIC:
            break;
        default:
            return BLASTMIDI_INVALIDPARAM;
    };

    result = allocate_event ( instance, BLASTMIDI_META_EVENT, subtype, data, data_size, &output );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }
    *event = output;
    return BLASTMIDI_OK;
}

uint8_t blastmidi_event_create_meta_midi_channel_prefix_event ( blastmidi* instance, uint8_t channel, blastmidi_event** event )
{
    blastmidi_event* output = NULL;
    uint8_t result = 0;

    assert ( sizeof ( channel ) == 1 );

    result = allocate_event ( instance, BLASTMIDI_META_EVENT, BLASTMIDI_META_MIDI_CHANNEL_PREFIX, ( uint8_t* ) &channel, 1, &output );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }
    *event = output;
    return BLASTMIDI_OK;
}

uint8_t blastmidi_event_create_meta_time_signature_event ( blastmidi* instance, uint8_t numerator, uint8_t denominator, uint8_t metronome, uint8_t thirtyseconds_per_24_signals, blastmidi_event** event )
{
    uint8_t result = 0;
    blastmidi_event* output = NULL;
    uint8_t data[4];

    assert ( sizeof ( numerator ) == 1 );

    data[0] = numerator;
    data[1] = denominator;
    data[2] = metronome;
    data[3] = thirtyseconds_per_24_signals;

    result = allocate_event ( instance, BLASTMIDI_META_EVENT, BLASTMIDI_META_TIME_SIGNATURE, data, 4, &output );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }
    *event = output;
    return BLASTMIDI_OK;
}

uint8_t blastmidi_event_create_meta_key_signature_event ( blastmidi* instance, int8_t key, uint8_t scale, blastmidi_event** event )
{
    uint8_t result = 0;
    blastmidi_event* output = NULL;
    uint8_t data[2];

    assert ( sizeof ( key ) == 1 );

    data[0] = ( uint8_t ) key;
    data[1] = scale;

    result = allocate_event ( instance, BLASTMIDI_META_EVENT, BLASTMIDI_META_KEY_SIGNATURE, data, 2, &output );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }
    *event = output;
    return BLASTMIDI_OK;
}

uint8_t blastmidi_event_create_sysex_event ( blastmidi* instance, uint8_t* data, unsigned int data_size, uint8_t end_of_sysex, blastmidi_event** event )
{
    blastmidi_event* output = NULL;
    uint8_t result = allocate_event ( instance, BLASTMIDI_SYSEX_EVENT, 0, data, data_size, &output );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }
    output->end_of_sysex = end_of_sysex;
    *event = output;
    return BLASTMIDI_OK;
}

uint8_t read_meta_event ( blastmidi* instance, uint8_t* end_of_track, blastmidi_event** event_ptr )
{
    uint8_t type = 0;
    uint32_t event_size = 0;
    uint8_t result = read_byte ( instance, &type );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }
    result = read_variable_number ( instance, &event_size );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }
    switch ( type )
    {
        case BLASTMIDI_META_SEQUENCE_NUMBER:
        {
            uint16_t sequence_number = 0;
            result = read_16_bit ( instance, &sequence_number );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
#ifdef BLASTMIDI_DEBUG
            printf ( "Sequence number: %u\n", ( uint32_t ) sequence_number );
#endif
            result = blastmidi_event_create_meta_sequence_number_event ( instance, sequence_number, event_ptr );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
            break;
        }
        case BLASTMIDI_META_TEXT:
        case BLASTMIDI_META_COPYRIGHT_NOTICE:
        case BLASTMIDI_META_SEQUENCE_OR_TRACK_NAME:
        case BLASTMIDI_META_INSTRUMENT_NAME:
        case BLASTMIDI_META_LYRICS:
        case BLASTMIDI_META_MARKER:
        case BLASTMIDI_META_CUE_POINT:
        case BLASTMIDI_META_SEQUENCER_SPECIFIC:
#ifdef BLASTMIDI_DEBUG
            printf ( "Data event.\n" );
#endif
            result = blastmidi_event_create_meta_data_event ( instance, type, NULL, event_size, event_ptr );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
            result = read_bytes ( instance, event_ptr[0]->data, event_size );
            if ( result != BLASTMIDI_OK )
            {
                blastmidi_event_free ( instance, *event_ptr );
                *event_ptr = NULL;
                return result;
            }
            break;
        case BLASTMIDI_META_MIDI_CHANNEL_PREFIX:
        {
            uint8_t channel = 0;
            result = read_byte ( instance, &channel );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
#ifdef BLASTMIDI_DEBUG
            printf ( "Channel prefix event.\nChannel: %u\n", ( uint32_t ) channel );
#endif
            result = blastmidi_event_create_meta_midi_channel_prefix_event ( instance, channel, event_ptr );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
            break;
        }
        case BLASTMIDI_META_END_OF_TRACK:
            /*
            * Set the end_of_track variable that we were given, to 1.
            * This informs the caller that it is time to stop reading events on this particular track.
            */
#ifdef BLASTMIDI_DEBUG
            printf ( "End of track.\n" );
#endif
            *end_of_track = 1;
            break;
        case BLASTMIDI_META_SET_TEMPO:
        {
            uint32_t tempo = 0;
            result = read_24_bit ( instance, &tempo );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
#ifdef BLASTMIDI_DEBUG
            printf ( "Tempo: %u\n", ( uint32_t ) tempo );
#endif
            result = blastmidi_event_create_meta_tempo_event ( instance, tempo, event_ptr );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
            break;
        }
        case BLASTMIDI_META_SMPTE_OFFSET:
            /*
            * Todo: Implement support for this. Right now we just skip ahead.
            */
            result = skip_ahead ( instance, 5 );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
#ifdef BLASTMIDI_DEBUG
            printf ( "SMPTE offset.\n" );
#endif
            break;
        case BLASTMIDI_META_TIME_SIGNATURE:
        {
            uint8_t numerator = 0;
            uint8_t denominator = 0;
            uint8_t metronome = 0;
            uint8_t thirtyseconds_per_24_signals = 0;
            result = read_byte ( instance, &numerator );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
            result = read_byte ( instance, &denominator );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
            result = read_byte ( instance, &metronome );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
            result = read_byte ( instance, &thirtyseconds_per_24_signals );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
#ifdef BLASTMIDI_DEBUG
            printf ( "Time signature.\nNumerator: %u\nDenominator: %u\nMetronome: %u\nThirty seconds per 24 signal: %u\n", ( uint32_t ) numerator, ( uint32_t ) denominator, ( uint32_t ) metronome, ( uint32_t ) thirtyseconds_per_24_signals );
#endif
            result = blastmidi_event_create_meta_time_signature_event ( instance, numerator, denominator, metronome, thirtyseconds_per_24_signals, event_ptr );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
            break;
        }
        case BLASTMIDI_META_KEY_SIGNATURE:
        {
            int8_t key = 0;
            uint8_t scale = 0;
            result = read_byte ( instance, ( uint8_t* ) &key );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
            result = read_byte ( instance, &scale );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
#ifdef BLASTMIDI_DEBUG
            printf ( "Key signature.\nKey: %d\nScale: %u\n", ( int32_t ) key, ( uint32_t ) scale );
#endif
            result = blastmidi_event_create_meta_key_signature_event ( instance, key, scale, event_ptr );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
            break;
        }
        default:
#ifdef BLASTMIDI_DEBUG
            printf ( "Unknown meta event type.\nType number: %u\nSize: %u\n", ( uint32_t ) type, event_size );
#endif
            result = skip_ahead ( instance, event_size );
            if ( result != BLASTMIDI_OK )
            {
                return result;
            }
    };
    return BLASTMIDI_OK;
}

uint8_t read_sysex_event ( blastmidi* instance, blastmidi_event** event_ptr )
{
    uint32_t event_size = 0;
    uint8_t final_byte = 0;
    uint8_t result = read_variable_number ( instance, &event_size );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }
    if ( event_size == 0 )
    {
        /*
        * This should never happen, but we allow for it.
        */
        return BLASTMIDI_OK;
    }

    result = blastmidi_event_create_sysex_event ( instance, NULL, event_size - 1, 1, event_ptr );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }

    if ( event_size > 1 )
    {
        result = read_bytes ( instance, event_ptr[0]->data, event_size - 1 );
        if ( result != BLASTMIDI_OK )
        {
            blastmidi_event_free ( instance, *event_ptr );
            *event_ptr = NULL;
            return result;
        }
    }

    /*
    * Read the last data byte in the message, in order to check what scenario this is (continuation or the end of the whole message).
    */
    result = read_byte ( instance, &final_byte );
    if ( result != BLASTMIDI_OK )
    {
        blastmidi_event_free ( instance, *event_ptr );
        *event_ptr = NULL;
        return result;
    }
    if ( final_byte == 0xF7 )
    {
        /*
        * This is the end of the entire event. The event structure is set up for this by default.
        */
        instance->sysex_continuation = 0;
    }
    else
    {
        /*
        * This is not the last part of the sysex continuation event, so we remember the fact that one is ongoing.
        * We also have to update the end_of_sysex member in our event structure, so that it indicates 0.
        */
        instance->sysex_continuation = 1;
        event_ptr[0]->end_of_sysex = 0;
    }
    return BLASTMIDI_OK;
}

uint8_t read_authorization_sysex_event ( blastmidi* instance, blastmidi_event** event_ptr )
{
    uint32_t event_size = 0;
    uint8_t result = read_variable_number ( instance, &event_size );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }
    if ( event_size == 0 )
    {
        /*
        * This should never happen, but we allow for it.
        */
        return BLASTMIDI_OK;
    }

    /*
    * Todo: Figure out whether we need a flag in the event structure to indicate that this is a sysex authorization event.
    */

    result = blastmidi_event_create_sysex_event ( instance, NULL, event_size, 1, event_ptr );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }

    result = read_bytes ( instance, event_ptr[0]->data, event_size );
    if ( result != BLASTMIDI_OK )
    {
        blastmidi_event_free ( instance, *event_ptr );
        *event_ptr = NULL;
        return result;
    }

    return BLASTMIDI_OK;
}

uint8_t read_track_events ( blastmidi* instance, uint16_t track_id )
{

    /*
    * Read all the events in a loop, breaking out either when an error occurs or when the end of track meta event is encountered.
    */

    while ( 1 )
    {

        blastmidi_event* event = NULL;
        /*
        * This is the event that we are about to create.
        */

        /*
        * Begin by reading the delta time.
        */
        uint32_t delta_time = 0;
        uint8_t event_type = 0;

        uint8_t end_of_track = 0;
        /*
        * This variable is passed to read_meta_event, and gets set to 1 if the end of track meta event is encountered.
        */

        uint8_t result = read_variable_number ( instance, &delta_time );
        if ( result != BLASTMIDI_OK )
        {
            return result;
        }
#ifdef BLASTMIDI_DEBUG
        printf ( "Delta time: %u\n", ( uint32_t ) delta_time );
#endif

        /*
        * Read the event type specifier.
        */
        result = read_byte ( instance, &event_type );
        if ( result != BLASTMIDI_OK )
        {
            return result;
        }

        switch ( event_type )
        {
            case 0xFF:
                result = read_meta_event ( instance, &end_of_track, &event );
                if ( result != BLASTMIDI_OK )
                {
                    return result;
                }
                break;
            case 0xF0:
#ifdef BLASTMIDI_DEBUG
                printf ( "SysEx event.\n" );
#endif
                result = read_sysex_event ( instance, &event );
                if ( result != BLASTMIDI_OK )
                {
                    return result;
                }
                break;
            case 0xF7:
                /*
                * This is either a part of a continuation sysex event, or a single authorization sysex event.
                * We determine this by checking the sysex_continuation flag in the blastmidi structure.
                */
                if ( instance->sysex_continuation == 1 )
                {
#ifdef BLASTMIDI_DEBUG
                    printf ( "Continued SysEx event.\n" );
#endif
                    result = read_sysex_event ( instance, &event );
                }
                else
                {
#ifdef BLASTMIDI_DEBUG
                    printf ( "Authorization SysEx event.\n" );
#endif
                    result = read_authorization_sysex_event ( instance, &event );
                }
                if ( result != BLASTMIDI_OK )
                {
                    return result;
                }
                break;
            default:
            {
                uint8_t midi_event_type = 0;
                uint8_t channel = 0;
                uint8_t new_status_bit = extract_bits_8 ( event_type, 1, 1 );
                if ( new_status_bit == 0 )
                {
                    /*
                    * This is a so called running status event, which means that the status byte is the same as for our last event.
                    * Therefore, we reuse that status byte and treat this byte as the first data byte of the event.
                    */
                    event_type = instance->running_status;
                    skip_backwards ( instance, 1 );
                }
                instance->running_status = event_type;
                midi_event_type = extract_bits_8 ( event_type, 1, 4 );
                channel = extract_bits_8 ( event_type, 5, 8 );
#ifdef BLASTMIDI_DEBUG
                printf ( "Midi event type: %u\nChannel: %u\n", ( uint32_t ) midi_event_type, ( uint32_t ) channel );
#endif

                switch ( midi_event_type )
                {
                    case BLASTMIDI_CHANNEL_NOTE_OFF:
                    case BLASTMIDI_CHANNEL_NOTE_ON:
                    case BLASTMIDI_CHANNEL_NOTE_AFTERTOUCH:
                    case BLASTMIDI_CHANNEL_CONTROLLER:
                    case BLASTMIDI_CHANNEL_PITCH_BEND:
                    {
                        uint8_t first = 0;
                        uint8_t second = 0;
                        result = read_byte ( instance, &first );
                        if ( result != BLASTMIDI_OK )
                        {
                            return result;
                        }
                        result = read_byte ( instance, &second );
                        if ( result != BLASTMIDI_OK )
                        {
                            return result;
                        }
                        result = blastmidi_event_create_channel_event ( instance, channel, midi_event_type, first, second, &event );
                        if ( result != BLASTMIDI_OK )
                        {
#ifdef BLASTMIDI_DEBUG
                            printf ( "Event creation error with two arguments.\n" );
#endif
                            return result;
                        }
                        break;
                    }
                    case BLASTMIDI_CHANNEL_PROGRAM_CHANGE:
                    case BLASTMIDI_CHANNEL_CHANNEL_AFTERTOUCH:
                    {
                        uint8_t first = 0;
                        result = read_byte ( instance, &first );
                        if ( result != BLASTMIDI_OK )
                        {
                            return result;
                        }
                        result = blastmidi_event_create_channel_event ( instance, channel, midi_event_type, first, 0, &event );
                        if ( result != BLASTMIDI_OK )
                        {
                            return result;
                        }
                        break;
                    }
                    default:
#ifdef BLASTMIDI_DEBUG
                        printf ( "This is an unknown Midi channel event.\n" );
#endif
                        if ( event )
                        {
                            blastmidi_event_free ( instance, event );
                        }
                        return BLASTMIDI_INVALIDCHUNK;
                };
            }
        };

        /*
        * If event is not NULL, we add it to the end of the track.
        */
        if ( event )
        {
            result = blastmidi_add_event_to_end_of_track ( instance, track_id, event, delta_time );
            if ( result != BLASTMIDI_OK )
            {
                blastmidi_event_free ( instance, event );
                return result;
            }
        }

        if ( end_of_track )
        {

            /*
            * The end of track meta event was encountered, so we break out of the loop and return.
            */
            break;
        }

    }

    return BLASTMIDI_OK;
}

uint8_t read_track ( blastmidi* instance, uint16_t track_id )
{

    /*
    * Begin by reading and verifying the header.
    * A track chunk should have a type of MTrk. We make a temporary storage of 5 bytes, and NULL terminate the string.
    */
    uint8_t temp[5];
    uint32_t chunk_size = 0;

    uint8_t result = read_bytes ( instance, temp, 4 );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }
    temp[4] = 0;
    if ( strcmp ( ( const char* ) temp, "MTrk" ) != 0 )
    {
#ifdef BLASTMIDI_DEBUG
        printf ( "Invalid chunk.\n" );
#endif
        return BLASTMIDI_INVALIDCHUNK;
    }

    result = read_32_bit ( instance, &chunk_size );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }

    if ( chunk_size == 0 )
    {
        return BLASTMIDI_INVALIDCHUNK;
    }

    instance->running_status = 0;
    instance->sysex_continuation = 0;

    return read_track_events ( instance, track_id );
}

uint8_t read_header ( blastmidi* instance )
{

    /*
    * Read the header chunk.
    * This header chunk should have a type of MThd. We make a temporary storage of 5 bytes, and NULL terminate the string.
    */
    uint8_t temp[5];
    uint32_t header_size = 0;
    uint16_t file_type = 0;
    uint16_t tracks = 0;
    uint16_t time_division = 0;
    uint8_t time_type = 0;

    uint8_t result = read_bytes ( instance, temp, 4 );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }
    temp[4] = 0;
    if ( strcmp ( ( const char* ) temp, "MThd" ) != 0 )
    {
#ifdef BLASTMIDI_DEBUG
        printf ( "Invalid chunk.\n" );
#endif
        return BLASTMIDI_INVALID;
    }

    result = read_32_bit ( instance, &header_size );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }

    /*
    * The header chunk should always have a size of 6.
    */
    if ( header_size != 6 )
    {
#ifdef BLASTMIDI_DEBUG
        printf ( "Invalid chunk header size.\nReported: %u\n", ( uint32_t ) header_size );
#endif
        return BLASTMIDI_INVALID;
    }

    /*
    * First, we read the type of the Midi file. It is stored in the first 2 bytes of the header data.
    */
    result = read_16_bit ( instance, &file_type );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }

    /*
    * The type must be either 0, 1 or 2. Verify this.
    */
    if ( file_type != 0 && file_type != 1 && file_type != 2 )
    {
        return BLASTMIDI_INVALID;
    }
#ifdef BLASTMIDI_DEBUG
    printf ( "File type: %u\n", ( uint32_t ) file_type );
#endif
    instance->file_type = ( uint8_t ) file_type;

    /*
    * Read the number of track chunks that are supposed to be in the file.
    */
    result = read_16_bit ( instance, &tracks );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }

    /*
    * There must always be at least one track. Verify this.
    */
    if ( tracks == 0 )
    {
        return BLASTMIDI_INVALID;
    }

    instance->track_count = tracks;

#ifdef BLASTMIDI_DEBUG
    printf ( "Tracks: %u\n", ( uint32_t ) tracks );
#endif

    /*
    * Read the time division.
    */
    result = read_16_bit ( instance, &time_division );
    if ( result != BLASTMIDI_OK )
    {
        return result;
    }

    /*
    * Figure out what the time measurement is, by reading the most significant bit.
    */
    time_type = ( uint8_t ) extract_bits_16 ( time_division, 1, 1 );
#ifdef BLASTMIDI_DEBUG
    printf ( "Time type: %u\n", ( uint32_t ) time_type );
#endif
    /*
    * Not really necessary, but we verify anyway just to be pedantic.
    */
    if ( time_type != 0 && time_type != 1 )
    {
#ifdef BLASTMIDI_DEBUG
        printf ( "This should never happen.\nTime type is %u.\n", ( uint32_t ) time_type );
#endif
        return BLASTMIDI_INVALID;
    }

    /*
    * Handle the time type possibilities.
    */
    if ( time_type == 0 )
    {

        /*
        * The time type specifies ticks per beat. Extract the following 15 bits to get the correct value.
        */
        uint16_t ticks_per_beat = extract_bits_16 ( time_division, 2, 16 );

        /*
        * This value must be greater than 0. Verify that.
        */
        if ( ticks_per_beat == 0 )
        {
            return BLASTMIDI_INVALID;
        }
        instance->ticks_per_beat = ticks_per_beat;
#ifdef BLASTMIDI_DEBUG
        printf ( "Ticks per beat: %u\n", ( uint32_t ) ticks_per_beat );
#endif
    }
    else
    {

        /*
        * The time type specifies frames per second. First figure out the number of SMPTE frames.
        */
        int8_t SMPTE_frames = 0;
        uint8_t ticks_per_frame = 0;

        SMPTE_frames = ( int8_t ) extract_bits_16 ( time_division, 1, 8 );
        SMPTE_frames = abs ( SMPTE_frames );
#ifdef BLASTMIDI_DEBUG
        printf ( "SMPTE frames: %d\n", ( int32_t ) SMPTE_frames );
#endif
        /*
        * This value must be either 24, 25, 29 or 30. Verify that it is.
        */
        if ( SMPTE_frames != 24 && SMPTE_frames != 25 && SMPTE_frames != 29 && SMPTE_frames != 30 )
        {
            return BLASTMIDI_INVALID;
        }

        /*
        * Now extract the ticks per frame.
        */
        ticks_per_frame = ( uint8_t ) extract_bits_16 ( time_division, 9, 16 );

        /*
        * This value must be greater than 0. Verify that.
        */
        if ( ticks_per_frame == 0 )
        {
            return BLASTMIDI_INVALID;
        }
#ifdef BLASTMIDI_DEBUG
        printf ( "Ticks per frame: %u\n", ( uint32_t ) ticks_per_frame );
#endif
        instance->SMPTE_frames = ( uint8_t ) SMPTE_frames;
        instance->ticks_per_frame = ticks_per_frame;
    }
    instance->time_type = time_type;
    return BLASTMIDI_OK;
}

uint8_t blastmidi_read ( blastmidi* instance )
{

    uint8_t result = 0;
    uint16_t i;

    /*
    * Did the user forget to give us an I/O callback?
    */
    if ( instance->data_callback == NULL )
    {
        return BLASTMIDI_NOCALLBACK;
    }

    /*
    * First of all, we reset the instance.
    */
    reset ( instance );

    /*
    * Now it is time to read the header.
    */
    result = read_header ( instance );
    if ( result != BLASTMIDI_OK )
    {
        reset ( instance );
        return result;
    }

    /*
    * Allocate memory for all the tracks. The number of tracks to allocate has already been stored in instance->track_count.
    */
    result = allocate_tracks ( instance );
    if ( result != BLASTMIDI_OK )
    {
        reset ( instance );
        return result;
    }

    /*
    * Read the tracks.
    */
    for ( i = 0; i < instance->track_count; ++i )
    {
        result = read_track ( instance, i );
        if ( result != BLASTMIDI_OK )
        {
            reset ( instance );
            return result;
        }
    }
    instance->valid = 1;
    return BLASTMIDI_OK;
}

void blastmidi_whipe_track ( blastmidi* instance, unsigned int track )
{
    blastmidi_event* current = NULL;
    assert ( instance );
    if ( instance == NULL )
    {
        return;
    }
    if ( track >= instance->track_count )
    {
        return;
    }
    current = instance->tracks[track];
    while ( current )
    {
        blastmidi_event* next = current->next;
        blastmidi_event_free ( instance, current );
        current = next;
    }
}

void blastmidi_event_free ( blastmidi* instance, blastmidi_event* event )
{
    /*
    * Note: This function does not take the linked list into consideration. Pointer bookkeeping has to have been done before
    * this function is invoked.
    */
    assert ( event );
    if ( event->data && event->data_size > sizeof ( event->small_pool ) )
    {
        instance->free_function ( event->data );
    }
    instance->free_function ( event );
}

uint8_t blastmidi_add_event ( blastmidi* instance, uint16_t track_id, blastmidi_event* event, uint32_t delta_time, blastmidi_event* add_after )
{
    if ( instance == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( event == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( event->track != -1 )
    {
        return BLASTMIDI_ALREADYADDED;
    }
    if ( track_id >= instance->track_count )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    event->track = track_id;
    event->time = delta_time;
    if ( add_after )
    {
        blastmidi_event* old_next = add_after->next;
        add_after->next = event;
        event->previous = add_after;
        event->next = old_next;
        if ( instance->track_ends[track_id] == add_after )
        {
            instance->track_ends[track_id] = event;
        }
    }
    else
    {
        blastmidi_event* old_beginning = instance->tracks[track_id];
        event->previous = NULL;
        event->next = old_beginning;
        instance->tracks[track_id] = event;
        if ( instance->track_ends[track_id] == NULL )
        {
            instance->track_ends[track_id] = event;
        }
    }
    return BLASTMIDI_OK;
}

uint8_t blastmidi_add_event_to_beginning_of_track ( blastmidi* instance, uint16_t track_id, blastmidi_event* event, uint32_t delta_time )
{
    if ( instance == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( event == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( event->track != -1 )
    {
        return BLASTMIDI_ALREADYADDED;
    }
    if ( track_id >= instance->track_count )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    return blastmidi_add_event ( instance, track_id, event, delta_time, NULL );
}

uint8_t blastmidi_add_event_to_end_of_track ( blastmidi* instance, uint16_t track_id, blastmidi_event* event, uint32_t delta_time )
{
    if ( instance == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( event == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( event->track != -1 )
    {
        return BLASTMIDI_ALREADYADDED;
    }
    if ( track_id >= instance->track_count )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    return blastmidi_add_event ( instance, track_id, event, delta_time, instance->track_ends[track_id] );
}

uint8_t blastmidi_get_first_event_on_track ( blastmidi* instance, uint16_t track_id, blastmidi_event** event )
{
    if ( instance == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( event == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( track_id >= instance->track_count )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    *event = instance->tracks[track_id];
    return BLASTMIDI_OK;
}

uint8_t blastmidi_get_last_event_on_track ( blastmidi* instance, uint16_t track_id, blastmidi_event** event )
{
    if ( instance == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( event == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( track_id >= instance->track_count )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    *event = instance->track_ends[track_id];
    return BLASTMIDI_OK;
}

uint8_t blastmidi_get_next_event_on_track ( blastmidi* instance, blastmidi_event** event )
{
    if ( instance == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( event == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( *event == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    *event = event[0]->next;
    return BLASTMIDI_OK;
}

uint8_t blastmidi_get_previous_event_on_track ( blastmidi* instance, blastmidi_event** event )
{
    if ( instance == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( event == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( *event == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    *event = event[0]->previous;
    return BLASTMIDI_OK;
}

uint8_t blastmidi_remove_event_from_track ( blastmidi* instance, uint16_t track_id, blastmidi_event* event )
{

    blastmidi_event* previous = NULL;
    blastmidi_event* next = NULL;

    if ( instance == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( track_id >= instance->track_count )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( event == NULL )
    {
        return BLASTMIDI_INVALIDPARAM;
    }
    if ( event->track == -1 )
    {
        return BLASTMIDI_NOTADDED;
    }
    if ( event->track != track_id )
    {
        return BLASTMIDI_NOTPARTOFTRACK;
    }
    previous = event->previous;
    next = event->next;
    if ( previous )
    {
        previous->next = next;
    }
    if ( next )
    {
        next->previous = previous;
    }
    if ( instance->tracks[track_id] == event )
    {
        instance->tracks[track_id] = next;
    }
    if ( instance->track_ends[track_id] == event )
    {
        instance->track_ends[track_id] = previous;
    }
    blastmidi_event_free ( instance, event );
    return BLASTMIDI_OK;
}

void blastmidi_free ( blastmidi* instance )
{
    reset ( instance );
}
