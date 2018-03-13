//
//  read_lfg.h
//  LFGDump
//
//  Created by Kevin Seltmann on 10/23/16.
//  Copyright © 2016 Kevin Seltmann. All rights reserved.
//
//  Designed to read the archiving used on LucasArts Classic Adventure
//  install files (*.XXX) and other Lucasfilm Games archives created with the
//  PKWARE Data Compression Library from ~1990.  Implementation for LFG file
//  extraction reverse-engineered from existing .XXX files.  Implementation
//  of explode algorithm based on specifications found on the internet.

/*
 
 LucasFilm Games file archive format:
 ------------------------------- ALL -------------------------------------------
 0000-0003	4	'LFG!'
 0004-0007	4	Length of this part of archive file. Least significant
                byte first.
 ---------------------FIRST ARCHIVE FILE ONLY  ---------------------------------
 0008-0014	13	Archive filename, 0 terminated.
 0015		1	0
 0016		1   Number of disks (archive files) that make up the total archive.
 0017		1   0
 0018-001B	4	Total space needed for all expanded files (in bytes), least
                significant byte first.
 ---------------- FILE DATA, REPEAT FOR EACH FILE ------------------------------
            4	'FILE'
            4	Length of data (in bytes) that follows for the compressed file,
                including headers, until next 'FILE' marker or end of archive.
                Least significant byte first.
           13	File name, zero term
            1	0
            4	Final length of expanded file (in bytes).  Least significant
                byte first.
            6	Unknown   [2,0,1,0,0,0]  (suspect uint16 2, uint32 1)
            ... Compressed File Data, using PKZ 'implode' ...
 -------------------------------------------------------------------------------
 
 */

#ifndef read_lfg_h
#define read_lfg_h

#include <stdio.h>
#include "DOSTYPES.H"

typedef enum
{
    VERBOSE_LEVEL_SILENT,
    VERBOSE_LEVEL_NORMAL,
    VERBOSE_LEVEL_HIGH
} verbose_level_enum;

int read_lfg_archive(int file_max,
                     const char * file_list[],
                     bool info_only,
                     bool show_stats,
                     verbose_level_enum verbose_level,
                     bool overwrite_flag,
                     const char* output_dir);

#endif /* read_lfg_h */
