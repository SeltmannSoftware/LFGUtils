//
//  implode.h
//  LFGMake
//
//  Created by Kevin Seltmann on 6/17/16.
//  Copyright © 2016 Kevin Seltmann. All rights reserved.
//

#ifndef implode_h
#define implode_h

#include <stdio.h>

typedef enum {
    IMPLODE_BINARY = 0,
    IMPLODE_ASCII = 1
} implode_literal_type;

typedef enum {
    IMPLODE_1K_DICTIONARY = 4,     // 1024 bytes.
    IMPLODE_2K_DICTIONARY = 5,     // 2048 bytes.
    IMPLODE_4K_DICTIONARY = 6      // 4096 bytes.
} implode_dictionary_size_type;

typedef struct {
    // Statistics
    long literal_count;  // Number of literals
    long lookup_count;   // Number of lookups into sliding dictionary
    
    int max_offset;    // Max possible offset is 2^(6+N)-1
                       // Assume N < 9 (valid values are 4-6).
    int min_offset;    // Min offset is 0
    int max_length;    // Max possible length is 518
    int min_length;    // Min length is 2
} implode_stats_type;

unsigned long implode( FILE * in_file,
                       FILE * out_file,
                       unsigned long length,
                       implode_literal_type literal_encode_mode,
                       implode_dictionary_size_type dictionary_size,
                       unsigned int optimization_level,
                       implode_stats_type* implode_stats,
                       unsigned long *max_length,
                       FILE* (*max_reached)( FILE*, unsigned long* ) );

#endif /* implode_h */
