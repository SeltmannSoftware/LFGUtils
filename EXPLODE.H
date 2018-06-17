//
//  explode.h
//  LFGDump V 1.5
//
//  Created by Seltmann Software on 6/12/16.
//  Copyright © 2016,2017,2018 Seltmann Software. All rights reserved.
//
//  Designed to extract the archiving used on LucasArts Classic Adventure
//  install files (*.XXX) and possibly other archives created with the PKWARE
//  Data Compression Library from ~1990.  Implementation based on
//  specification found on the internet
//  Tested on all LucasArts Classic Adventure games and produces indentical
//  output.

#ifndef explode_h
#define explode_h

#include <stdio.h>

typedef struct {
    unsigned int dictionary_size;
    unsigned int literal_mode;

    // Statistics
    unsigned long literal_count;
    unsigned long dictionary_count;
    int max_offset;
    int min_offset;
    int max_length;
    int min_length;
} explode_stats_type;

unsigned long write_buffer_get_bytes_written( void );
unsigned long read_buffer_get_bytes_read( void );

/* Extract a file from an archive file and explode it.
   in_fp:           Pointer to imploded data start in archive file.
   out_fp:          Output filename. NULL exceptable (stats only).
   expected_length: Expected length of file (0 if not provided).
   eof_reached():   Callback that indicates archive EOF is reached.
        Callback should return new file pointer with
        the continued data for the imploded file.
*/
long extract_and_explode( FILE* in_fp,
       FILE* out_fp,
       long expected_length,
       explode_stats_type* explode_stats,
       FILE* (*eof_reached)(void));

#endif /* explode_h */
