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
*          blastmidi.h
* The main library header which defines the public API.
*/

#ifndef BLASTMIDI_H
#define BLASTMIDI_H

#include <stdint.h> /* For uint8_t, uint16_t etc */

/*
* Error codes.
*/
enum blastmidi_errors
{
    BLASTMIDI_OK = 0, /* All is joy */
    BLASTMIDI_INVALIDPARAM, /* One or more invalid parameters were passed to the function */
    BLASTMIDI_OUTOFMEMORY, /* Out of memory */
    BLASTMIDI_ALREADYADDED, /* This event has already been added to a track and is now owned by the given blastmidi instance. */
    BLASTMIDI_NOTADDED, /* This event has not been added to a track in the given blastmidi instance. */
    BLASTMIDI_NOTPARTOFTRACK, /* This event is not part of the specified track. */
    BLASTMIDI_NOCALLBACK, /* No I/O callback provided */
    BLASTMIDI_INVALIDCHUNK, /* The Midi file contained an invalid chunk */
    BLASTMIDI_INCOMPLETECHUNK, /* The Midi file chunk ended unexpectedly */
    BLASTMIDI_UNEXPECTEDEND, /* The Midi file ended unexpectedly */
    BLASTMIDI_WRITINGFAILED, /* Writing failed */
    BLASTMIDI_INVALID /* This is not a valid Midi file */
};

/*
* Meta event types.
*/
enum blastmidi_meta_events
{
    BLASTMIDI_META_SEQUENCE_NUMBER = 0,
    BLASTMIDI_META_TEXT,
    BLASTMIDI_META_COPYRIGHT_NOTICE,
    BLASTMIDI_META_SEQUENCE_OR_TRACK_NAME,
    BLASTMIDI_META_INSTRUMENT_NAME,
    BLASTMIDI_META_LYRICS,
    BLASTMIDI_META_MARKER,
    BLASTMIDI_META_CUE_POINT,
    BLASTMIDI_META_MIDI_CHANNEL_PREFIX = 32,
    BLASTMIDI_META_END_OF_TRACK = 47,
    BLASTMIDI_META_SET_TEMPO = 81,
    BLASTMIDI_META_SMPTE_OFFSET = 84,
    BLASTMIDI_META_TIME_SIGNATURE = 88,
    BLASTMIDI_META_KEY_SIGNATURE,
    BLASTMIDI_META_SEQUENCER_SPECIFIC = 127
};

/*
* Midi channel event types.
*/
enum blastmidi_channel_events
{
    BLASTMIDI_CHANNEL_NOTE_OFF = 8,
    BLASTMIDI_CHANNEL_NOTE_ON,
    BLASTMIDI_CHANNEL_NOTE_AFTERTOUCH,
    BLASTMIDI_CHANNEL_CONTROLLER,
    BLASTMIDI_CHANNEL_PROGRAM_CHANGE,
    BLASTMIDI_CHANNEL_CHANNEL_AFTERTOUCH,
    BLASTMIDI_CHANNEL_PITCH_BEND
};

/*
* Data I/O callback actions.
*/
enum blastmidi_callback_actions
{
    BLASTMIDI_CALLBACK_READ,
    BLASTMIDI_CALLBACK_WRITE,
    BLASTMIDI_CALLBACK_SEEK
};

/*
* The blastmidi_data_callback function.
* The first parameter indicates an action that the callback should perform.
* This is one of the values specified in the blastmidi_callback_actions enum.
* The second parameter specifies the number of bytes to seek, read or write depending on the action.
* If the first parameter is BLASTMIDI_CALLBACK_SEEK, the second parameter is an absolute value.
* In all other cases, it is relative.
* The third parameter is a pointer to a buffer which holds at least the number of bytes specified in the second parameter.
* This is where you should either read or write your data.
* If the first parameter is BLASTMIDI_CALLBACK_SEEK, this buffer is not used.
* The fourth parameter is a user controlled void* pointer which is not used in any way by the library, but merely passed along.
* The callback should return 0 on failure and anything else on success.
* The callback must not invoke any function that operates on the blastmidi instance with which this callback is associated.
*/
typedef int blastmidi_data_callback ( int, size_t, uint8_t*, void* );

/*
* The custom memory allocation functions.
* It is possible to replace the default malloc and free implementations with your own custom functions.
* These must have the same signature as malloc and free.
* See the blastmidi_initialize function for more information on how to set these.
*/
typedef void* blastmidi_custom_malloc ( size_t );
typedef void blastmidi_custom_free ( void* );

/*
* Midi event types enum.
* This enum lists the three Midi event types (Midi channel event, meta event and system exclusive event.
*/
enum blastmidi_event_types
{
    BLASTMIDI_CHANNEL_EVENT = 1,
    BLASTMIDI_META_EVENT,
    BLASTMIDI_SYSEX_EVENT
};

/*
* The blastmidi_event structure.
* This structure represents a Midi event.
* The track member indicates whether this event belongs to a track. If this is -1, the event has not yet been added to any track.
* time specifies the delta time in ticks where this event occurs.
* type specifies the event type (Midi channel event, meta event or system exclusive event as listed in the enum above).
* subtype specifies the type of the event in the given category if applicable.
* If type is BLASTMIDI_META_EVENT, subtype corresponds to one of the values in the blastmidi_meta_events enum.
* If type is BLASTMIDI_CHANNEL_EVENT, subtype corresponds to one of the values in the blastmidi_channel_events enum.
* If type is BLASTMIDI_SYSEX_EVENT, subtype is not used.
*
* If type is BLASTMIDI_CHANNEL_EVENT, channel indicates the channel to which this event applies.
* If type is BLASTMIDI_META_EVENT and subtype is BLASTMIDI_META_MIDI_CHANNEL_PREFIX, channel specifies the channel being referred to.
* Otherwise, channel is not used.
*
* data is a pointer to the first data byte in the event, and data_size specifies the number of bytes present.
* If data_size is 0, do not access data.
*
* If type is BLASTMIDI_CHANNEL_EVENT and subtype is BLASTMIDI_CHANNEL_PITCH_BEND, data should be interpreted as a
* uint16_t (in native endian byte order) representing the bend amount.
* The range is between 0 and 16383 (inclusive) where values below 8192 decrease the pitch, and values above increase it.
*
* end_of_sysex is only applicable when type is BLASTMIDI_SYSEX_EVENT.
* System exclusive messages are sometimes split into several events if the amount of data is large.
* end_of_sysex is nonzero if this is the final chunk of the given system exclusive data.
* If all of the system exclusive data is contained in a single event, end_of_sysex is nonzero.
* Otherwise, end_of_sysex is 0 for all the parts except the last one.
*
* small_pool is an array of two bytes which is used to store very short data buffers.
* Since a lot of Midi events do not have more than two data bytes we can greatly reduce the number of allocations this way.
* The data pointer above will refer to the first byte of small_pool if applicable.
*
* previous and next are pointers to the previous and the next event on the track, respectively.
*
* Do not modify the members in this structure by hand, and do not access them before the structure has been populated by one of
* the library functions.
*/
typedef struct blastmidi_event
{
    int16_t track;
    uint32_t time;
    uint8_t type;
    uint8_t subtype;
    int8_t channel;
    uint8_t* data;
    uint32_t data_size;
    uint8_t end_of_sysex;
    uint8_t small_pool[2];
    struct blastmidi_event* previous;
    struct blastmidi_event* next;
} blastmidi_event;

/*
* The blastmidi structure.
* You should never access the elements in this structure directly.
* It holds the core internal state for the BlastMidi library.
* data_callback is a pointer to the callback which handles data I/O.
* data_callback_data is a user controlled void* pointer which the library passes along to the data callback.
* endian_flag is a flag storing the result of the runtime check for little endian.
* 0 is yet unchecked, 1 is little endian and 2 is not little endian (we assume big endian in this scenario).
* malloc_function and free_function are pointers to memory allocation functions (the system defined malloc and free by default).
* track_count specifies the number of tracks in the file.
* file_type specifies what type of Midi file this is (0, 1 or 2).
* time_type specifies what type of time measurement is used (0 for ticks per beat or 1 for ticks per SMPTE frame).
* ticks_per_beat specifies the number of ticks per beat, and is valid if time_type is 0.
* SMPTE_frames specifies the number of frames per second (24, 25, 29 or 30). Valid only if time_type is 1.
* ticks_per_frame specifies the number of ticks per frame (valid only if time_type is 1).
* valid is a flag that specifies whether the whole Midi file is valid or not(0=not valid, 1=valid).
* tracks is an array of pointers to the first event in each track that makes up the Midi file.
* The events in each track are organized as a linked list.
* track_ends is an array of pointers to the last event in each track that makes up the Midi file.
* The number of items in the track_ends list is specified by chunk_count.
* The next member of the blastmidi_event structures pointed to in this array should always be NULL.
* cursor is an I/O cursor used by the parser.
* running_status holds the last received status byte in a Midi channel event, so that the status can be reused as needed.
* sysex_continuation is a boolean flag which keeps track of whether a divided system exclusive message is being read.
*/
typedef struct blastmidi
{
    blastmidi_data_callback* data_callback;
    void* data_callback_data;
    int8_t endian_flag;
    blastmidi_custom_malloc* malloc_function;
    blastmidi_custom_free* free_function;
    uint16_t track_count;
    uint8_t file_type;
    uint8_t time_type;
    uint16_t ticks_per_beat;
    uint8_t SMPTE_frames;
    uint8_t ticks_per_frame;
    uint8_t valid;
    blastmidi_event** tracks;
    blastmidi_event** track_ends;
    size_t cursor;
    uint8_t running_status;
    uint8_t sysex_continuation;
} blastmidi;

/*
*          uint8_t blastmidi_initialize(blastmidi* instance, blastmidi_custom_malloc* user_malloc, blastmidi_custom_free* user_free);
* This function should be invoked on each new instance of the blastmidi structure that you instantiate.
* Using a blastmidi structure without invoking this function first will result in undefined behavior.
* The first parameter is a pointer to the structure instance that should be initialized.
* The second and third parameters are pointers to memory allocation functions (normally malloc and free).
* Both of these may be NULL, in which case the system defined malloc and free functions will be used.
* If one of these function pointers is not NULL, the other one must also be valid.
* The return value is one of the defined BlastMidi error codes.
*/
uint8_t blastmidi_initialize ( blastmidi* instance, blastmidi_custom_malloc* user_malloc, blastmidi_custom_free* user_free );

/*
*          void blastmidi_set_data_callback(blastmidi* instance, blastmidi_data_callback* callback, void* user_data);
* This function associates a data I/O callback with a given blastmidi instance.
* The first parameter is a pointer to the blastmidi instance.
* The second parameter is a pointer to a function of type blastmidi_data_callback. If this is NULL, no I/O can be performed.
* For more information on this callback function, see the comment above.
* The third parameter is a user controlled void* pointer which is passed along to the callback.
*/
void blastmidi_set_data_callback ( blastmidi* instance, blastmidi_data_callback* callback, void* user_data );

/*
*          uint8_t blastmidi_read(blastmidi* instance)
* Invoke this function to read a new Midi file stream.
* The return value is one of the defined BlastMidi error codes.
*/
uint8_t blastmidi_read ( blastmidi* instance );

/*
* void blastmidi_whipe_track(blastmidi* instance, unsigned int track);
* Removes all events from the given track. The track number starts at 0.
* If the track number does not refer to an existing track or if the track is already empty, this function is a no-op.
*/
void blastmidi_whipe_track ( blastmidi* instance, unsigned int track );

/*
*         void blastmidi_free(blastmidi* instance);
* Frees all resources associated with the given instance.
*/
void blastmidi_free ( blastmidi* instance );

/*
* uint8_t blastmidi_event_create_channel_event(blastmidi* instance, uint8_t channel, uint8_t subtype, uint8_t param_1, uint8_t param_2, blastmidi_event** event);
* Creates a Midi channel event. Depending on the event, param_1 and param_2 may or may not be used.
* After a successful call to this function, event points to an initialized blastmidi_event structure representing the new event.
*/
uint8_t blastmidi_event_create_channel_event ( blastmidi* instance, uint8_t channel, uint8_t subtype, uint8_t param_1, uint8_t param_2, blastmidi_event** event );

/*
* uint8_t blastmidi_event_create_meta_sequence_number_event(blastmidi* instance, uint16_t sequence_number, blastmidi_event** event);
* Creates a meta sequence number event.
* After a successful call to this function, event points to an initialized blastmidi_event structure representing the new event.
*/
uint8_t blastmidi_event_create_meta_sequence_number_event ( blastmidi* instance, uint16_t sequence_number, blastmidi_event** event );

/*
* uint8_t blastmidi_event_create_meta_tempo_event(blastmidi* instance, uint32_t tempo, blastmidi_event** event);
* Creates a meta tempo event. The tempo is specified as the number of microseconds per quarter note.
* After a successful call to this function, event points to an initialized blastmidi_event structure representing the new event.
*/
uint8_t blastmidi_event_create_meta_tempo_event ( blastmidi* instance, uint32_t tempo, blastmidi_event** event );

/*
* uint8_t blastmidi_event_create_meta_data_event(blastmidi* instance, uint8_t subtype, uint8_t* data, unsigned int data_size, blastmidi_event** event);
* Creates any one of the data meta events. The exact event type is specified by subtype and may be one of the following:
* BLASTMIDI_META_TEXT, BLASTMIDI_META_COPYRIGHT_NOTICE, BLASTMIDI_META_SEQUENCE_OR_TRACK_NAME,
* BLASTMIDI_META_INSTRUMENT_NAME, BLASTMIDI_META_LYRICS, BLASTMIDI_META_MARKER, BLASTMIDI_META_CUE_POINT
* or BLASTMIDI_META_SEQUENCER_SPECIFIC.
* The contents of data is copied into internal storage, so the memory does not need to be valid once this function returns.
* After a successful call to this function, event points to an initialized blastmidi_event structure representing the new event.
*/
uint8_t blastmidi_event_create_meta_data_event ( blastmidi* instance, uint8_t subtype, uint8_t* data, unsigned int data_size, blastmidi_event** event );

/*
* uint8_t blastmidi_event_create_meta_midi_channel_prefix_event(blastmidi *instance, uint8_t channel, blastmidi_event **event);
* Creates a Midi channel prefix event.
* A Midi channel prefix event is used to tell the reader that the following meta events belong to a given channel.
* The effect of a Midi channel prefix event goes away either when another channel prefix event or any non-meta event occurs.
* After a successful call to this function, event points to an initialized blastmidi_event structure representing the new event.
*/
uint8_t blastmidi_event_create_meta_midi_channel_prefix_event ( blastmidi* instance, uint8_t channel, blastmidi_event** event );

/*
* uint8_t blastmidi_event_create_meta_time_signature_event(blastmidi *instance, uint8_t numerator, uint8_t denominator, uint8_t metronome, uint8_t thirtyseconds_per_24_signals, blastmidi_event **event);
* Creates a meta time signature event.
* numerator is specified as a literal value, such as 3 or 4.
* denominator is the value to which the power of two must be raised to equal the number of subdivisions per whole note.
* 0 means a whole note, for example, 1 means a half note, 2 means a quarter note and 3 means an eighth note etc.
* metronome specifies how often the metronome should click, in clock ticks per quarter note.
* There are 24 clock ticks per quarter note, so if you want the metronome to click every half note you would pass 48.
* thirtyseconds_per_24_signals specifies the number of thirtyseconds per quarter note, in clock signals per quarter note.
* As mentioned previously, there are 24 clock signals per quarter note. Therefore, thirtyseconds_per_24_signals should
* nearly always be 8 since there are 8 thirtysecond notes per quarter. Only use a value other than 8 if you have a good reason.
* After a successful call to this function, event points to an initialized blastmidi_event structure representing the new event.
*/
uint8_t blastmidi_event_create_meta_time_signature_event ( blastmidi* instance, uint8_t numerator, uint8_t denominator, uint8_t metronome, uint8_t thirtyseconds_per_24_signals, blastmidi_event** event );

/*
* uint8_t blastmidi_event_create_meta_key_signature_event(blastmidi* instance, int8_t key, uint8_t scale, blastmidi_event** event);
* Creates a meta key signature event.
* key specifies the key in terms of the number of flats or sharps that exist in the key.
* A negative value indicates the number of flats, and a positive value indicates the number of sharps. 0 means C.
* scale is set to 0 for major and 1 for minor.
* After a successful call to this function, event points to an initialized blastmidi_event structure representing the new event.
*/
uint8_t blastmidi_event_create_meta_key_signature_event ( blastmidi* instance, int8_t key, uint8_t scale, blastmidi_event** event );

/*
* Todo: Add a constructor function for the SMPTE offset meta event.
*/

/*
* uint8_t blastmidi_event_create_sysex_event(blastmidi* instance, uint8_t* data, unsigned int data_size, uint8_t end_of_sysex, blastmidi_event** event);
* Creates a system exclusive event.
* end_of_sysex should be nonzero if this is the last, or indeed the only, event associated with this data chunk.
* If you wish to split up a large chunk of data into many events, specify 0 for end_of_sysex for all events except the last one.
* The contents of data is copied into internal storage, so the memory does not need to be valid once this function returns.
* After a successful call to this function, event points to an initialized blastmidi_event structure representing the new event.
*/
uint8_t blastmidi_event_create_sysex_event ( blastmidi* instance, uint8_t* data, unsigned int data_size, uint8_t end_of_sysex, blastmidi_event** event );

/*
* uint8_t blastmidi_add_event(blastmidi* instance, uint16_t track_id, blastmidi_event* event, uint32_t delta_time, blastmidi_event* add_after);
* This function associates the given Midi event with a particular track in a blastmidi instance.
* Note that the blastmidi instance takes ownership of the event after this point, so the event should not be freed manually.
* track_id starts at 0 and specifies the track to which this event should be added.
* add_after specifies the event after which this new one should be inserted.
* The new event is added immediately after add_after, with the appropriate delta time in between (see below).
* If add_after is NULL, the event will be added to the beginning of the track.
* delta_time is the number of ticks that must elapse between add_after and the new event.
* If add_after is NULL, delta_time is the amount of time that must pass from the beginning of the track until the new event occurs.
*/
uint8_t blastmidi_add_event ( blastmidi* instance, uint16_t track_id, blastmidi_event* event, uint32_t delta_time, blastmidi_event* add_after );

/*
* uint8_t blastmidi_add_event_to_beginning_of_track(blastmidi* instance, uint16_t track_id, blastmidi_event* event, uint32_t delta_time);
* This function associates the given Midi event with a particular track in a blastmidi instance.
* Note that the blastmidi instance takes ownership of the event after this point, so the event should not be freed manually.
* track_id starts at 0 and specifies the track to which this event should be added.
* The new event is inserted at the very beginning of the track, with the appropriate delta time in between (see below).
* delta_time is the amount of time that must pass from the beginning of the track until this new event occurs.
*/
uint8_t blastmidi_add_event_to_beginning_of_track ( blastmidi* instance, uint16_t track_id, blastmidi_event* event, uint32_t delta_time );

/*
* uint8_t blastmidi_add_event_to_end_of_track(blastmidi* instance, uint16_t track_id, blastmidi_event* event, uint32_t delta_time);
* This function associates the given Midi event with a particular track in a blastmidi instance.
* Note that the blastmidi instance takes ownership of the event after this point, so the event should not be freed manually.
* track_id starts at 0 and specifies the track to which this event should be added.
* The new event is inserted at the very end of the track, with the appropriate delta time (see below).
* delta_time is the amount of time that must pass from the current end of the track until this new event occurs.
* If the track is empty, the new event is added as the first event on the track with the appropriate delta time.
*/
uint8_t blastmidi_add_event_to_end_of_track ( blastmidi* instance, uint16_t track_id, blastmidi_event* event, uint32_t delta_time );

/*
* uint8_t blastmidi_get_first_event_on_track(blastmidi* instance, uint16_t track_id, blastmidi_event** event);
* This function gets the first event from the given track.
* track_id starts at 0 and specifies the track from which the event should be retrieved.
* Note that the event is not copied. You should therefore not modify the event, as it is still owned by the blastmidi instance.
*/
uint8_t blastmidi_get_first_event_on_track ( blastmidi* instance, uint16_t track_id, blastmidi_event** event );

/*
* uint8_t blastmidi_get_last_event_on_track(blastmidi* instance, uint16_t track_id, blastmidi_event** event);
* This function gets the last event from the given track.
* track_id starts at 0 and specifies the track from which the event should be retrieved.
* Note that the event is not copied. You should therefore not modify the event, as it is still owned by the blastmidi instance.
*/
uint8_t blastmidi_get_last_event_on_track ( blastmidi* instance, uint16_t track_id, blastmidi_event** event );

/*
* uint8_t blastmidi_get_next_event_on_track(blastmidi* instance, blastmidi_event** event);
* This function gets the next event from the given track.
* event is expected to initially contain a pointer to an event on the given track, based on which the next one will be retrieved.
* The event pointer is updated so that it points to the next event on the track after the call to this function completes.
* If there is no event present after the current one on the track, event will be set to point to NULL.
* Note that the retrieved event is not copied. You should therefore not modify it , as it is still owned by the blastmidi instance.
*/
uint8_t blastmidi_get_next_event_on_track ( blastmidi* instance, blastmidi_event** event );

/*
* uint8_t blastmidi_get_previous_event_on_track(blastmidi* instance, blastmidi_event** event);
* This function gets the previous event from the given track.
* event is expected to initially contain a pointer to an event on the given track, based on which the previous one will be retrieved.
* The event pointer is updated so that it points to the previous event on the track after the call to this function completes.
* If there is no event present before the current one on the track, event will be set to point to NULL.
* Note that the retrieved event is not copied. You should therefore not modify it , as it is still owned by the blastmidi instance.
*/
uint8_t blastmidi_get_previous_event_on_track ( blastmidi* instance, blastmidi_event** event );

/*
* uint8_t blastmidi_remove_event_from_track(blastmidi* instance, uint16_t track_id, blastmidi_event* event);
* This function removes an event from the track to which it currently belongs.
* track_id starts at 0 and specifies the track from which this event should be removed.
* The event must have been added to the given track in the blastmidi instance prior to this call.
* The event will be removed from the given track, and all its associated resources will be automatically freed.
*/
uint8_t blastmidi_remove_event_from_track ( blastmidi* instance, uint16_t track_id, blastmidi_event* event );

/*
*         void blastmidi_event_free(blastmidi* instance, blastmidi_event* event);
* Frees all resources associated with the given event.
* Note: This function does not take the linked list into consideration. Pointer bookkeeping has to have been done before
* this function is invoked.
* Note also that after the event has been associated with a track in a blastmidi instance, you should not free it manually.
* The blastmidi instance takes ownership of the event as soon as it is associated.
* To remove an event that has been added to a track, call blastmidi_remove_event_from_track instead.
*/
void blastmidi_event_free ( blastmidi* instance, blastmidi_event* event );

#endif /* BLASTMIDI_H */
