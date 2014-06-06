BlastMidi

A library of routines for working with Midi files.

Copyright Blastbay Studios (Philip Bennefall) 2014.


BlastMidi is a C library that makes it easy to work with Midi files. Currently the library only supports parsing, but support for writing Midi files is planned as well.

The project is in a very early stage of development, and therefore the documentation is sparse. Currently the only source of usage information for the library is found in the library header located in the include directory.

To build the library, simply add blastmidi.c to your project and include blastmidi.h. There is only one other file that needs to be present; blastmidi_utility.h which is found in the src directory along with blastmidi.c. The library is written in Ansi C, and should build under any reasonably standards compliant C compiler. The library requires stdint.h, but it is my intent that it shouldn't use any other C99 features.

If you like this library and would like to see it developed further, feel free to contact me. My email address is philip@blastbay.com. I can only offer very limited support at this time, however, as this is merely a hobby project.

Thank you for checking out BlastMidi!

Philip Bennefall