//
//  pack_lfg.h
//  LFGMake
//
//  Created by Kevin Seltmann on 11/5/16.
//  Copyright © 2016 Kevin Seltmann. All rights reserved.
//

/*
           -- LucasFile Games installer archive naming convention --
 
 [GAME]_[DISK].XXX    // Name is always 8.3
 Examples:
 
 MANIAC_A.XXX         // Single archive
 
 LOOM___A.XXX         // Archive spans two files on two disks
 LOOM___B.XXX
 
 INDY___B.XXX         // Starts with disk B
 INDY___C.XXX
 INDY___D.XXX
 INDY___E.XXX

                  -- LucasFilm Games file archive format --
 
 [Location][Len][Description]
 --------------- HEADER FOR ALL ARCHIVE FILES [8 bytes] ------------------------
 0000-0003	4	'LFG!'
 0004-0007	4	Length of archive file (ie disk segment).
                Least significant byte first.
 --------------- FIRST ARCHIVE FILE ONLY [20 bytes] ----------------------------
 0008-0014	13	Archive filename, 0 terminated.
 0015		1	0
 0016		1   Number of disks (archive files) that make up the total archive.
 0017		1   0
 0018-001B	4	Total space required for all files after expansion (in bytes).
                Least significant byte first.
 --------------- FILE DATA, REPEAT FOR EACH FILE [32 bytes] --------------------
            4  'FILE'
            4  Length of data (in bytes) that follows for the compressed file,
                including headers, until next 'FILE' marker or end of archive.
                Least significant byte first.
            13  File name, 0 terminated.
            1  0
            4  Final length of expanded file (in bytes).
                Least significant byte first.
            6  Unknown [2,0,1,0,0,0]  (suspect uint16 2, uint32 1)
            ... Compressed File Data, using PKZ 'implode'
 -------------------------------------------------------------------------------
 
 Spanning: 
 Files may span across archive segments. Segment will begin with the common
 header and then continue immediately with compressed data from where previous
 file left off.
 
  */


#ifndef pack_lfg_h
#define pack_lfg_h

#include <stdio.h>
#include "IMPLODE.H"

typedef enum {
  LFG_WINDOW_1K = 4,
  LFG_WINDOW_2K = 5,
  LFG_WINDOW_4K = 6,
  LFG_DEFAULT
} lfg_window_size_type;

int pack_lfg(lfg_window_size_type dictionary_size,
             unsigned int literal_mode,
             const char* archive,
             char** file_list,
             int num_files,
             unsigned long first_disk_size,
             unsigned long disk_size,
             unsigned int optimize_level,
             bool verbose);

#endif /* lfgpack_h */
