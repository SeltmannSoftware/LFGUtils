//
//  read_lfg.c
//  LFGDump
//
//  Created by Kevin Seltmann on 10/23/16.
//  Copyright (C) 2016 Kevin Seltmann. All rights reserved.
//
//  Designed to extract the archiving used on LucasArts Classic Adventure
//  install files (*.XXX) and possibly other archives created with the PKWARE
//  Data Compression Library from ~1990.  Implementation for LFG file
//  extraction reverse-engineered from existing .XXX files.  Implementation
//  of explode algorithm based on specifications found on the internet.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "DOSTYPES.H"
#include "EXPLODE.H"
#include "READ_LFG.H"

// ----

// Check that first four bytes of file are 'LFG!'
// File pointer is set at 4 bytes from start of file.
bool isFileLFG( FILE *fp_in ) {
  
  char header[4];
  
  fseek ( fp_in, 0, SEEK_SET );
  
  if ( fread( header, sizeof header[0], 0x4, fp_in) == 0x4 ) {
    if( memcmp ( header, "LFG!", 4 ) == 0 ) {
      return true;
    }
  }
  return false;
}

// Check that next four bytes of file are 'FILE'
// File pointer is advanced 4 bytes
bool isFileNext( FILE *fp_in ) {
  
  char buffer[4];
  
  if ( fread( buffer, sizeof buffer[0], 0x4, fp_in) == 0x4 ) {
    if( memcmp ( buffer, "FILE", 4 ) == 0 ) {
      return true;
    }
  }
  return false;
}

// Read in the next four bytes. Treated as a value, stored with least
// significant byte first. On newer CPUs result is stored in a type that is larger than required. No harm.
bool read_uint32( FILE* fp_in, unsigned long* result ) {
  unsigned char buffer[4];
  
  if ( fread( buffer, sizeof buffer[0], 0x4, fp_in ) == 0x4 ) {
    *result = ((uint32_t)buffer[3] << 24) | ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[1] << 8) | ((uint32_t)buffer[0]);
    return true;
  }
  return false;
}

// Read a block of bytes into a buffer
bool read_chunk( FILE *fp_in, char * buffer, int length ) {
  
  if ( fread( buffer, sizeof buffer[0], length, fp_in ) == length ) {
    return true;
  }
  return false;
}

// Read one byte, return whether it is expected or not.
bool read_expected_byte(FILE *fp_in,
                        char expectedByte)
{
  
  if (expectedByte == fgetc(fp_in))
  {
    return true;
  }
  return false;
}

// ---

bool open_archive(FILE **fp,
                  char* filename,
                  unsigned long* reported_length,
                  long* actual_length )
{
  FILE *fp_open;
  unsigned long length;
  long file_length;
  
  // Open for binary read
  fp_open=fopen(filename, "rb");
  
  // Open failed...
  if (fp_open == 0) {
    //printf("Error opening file %s.\n\n", filename);
    return false;
  }
  
  // Find file length
  fseek ( fp_open, 0, SEEK_END );
  file_length = ftell( fp_open );
  
  // -- Read LFG Header --
  
  //Check first four bytes
  if (!isFileLFG(fp_open)) {
    printf("\n%s does not appear to be a LFG archive "
           "('LFG!' tag not found).\n\n",
           filename);
    fclose (fp_open);
    return false;
  }
  
  // Read and check archive length
  if (!read_uint32(fp_open, &length)) {
    printf("%s does not appear to be a valid LFG archive.\n\n",
           filename);
    fclose (fp_open);
    return false;
  }
  
  // Sanity check on length
  if (file_length != length + 8)
  {
    printf("Warning: Actual archive file length (%ld)\n         "
           "does not match indicated length (%ld + 8).\n",
           file_length, length);
  }
  
  // Update length field, file length, and file pointer.
  *reported_length = length;
  *actual_length = file_length;
  *fp = fp_open;
  
  return true;
}

/* --- */

typedef struct             // Archive Data (reported)
{
  long file_length;      // Length of archive disk/file. **
  char filename[14];     // Reported archive name.
  unsigned long length;       // Length of current archive disk/file.
  char num_disks;        // Number of disks/archive files.
  unsigned long space_needed; // Total bytes after expansion
  long total_length;
} archive_info_type;

archive_info_type archive_info = {0};

verbose_level_enum verbose = VERBOSE_LEVEL_SILENT;

struct
{
  int file_count;                 // number of files extracted
  long bytes_written_so_far;       // give/checks final length (add check?)
  long bytes_read_so_far;          // not used
  
  FILE* fp;                       // File pointer to current archive file
  long file_pos;
  
  char cur_filename[256];         // archive path & filename
  unsigned long filename_length;  // length of above
  char* file_name;                // short file name (no path)
  int   file_index;               // index in file list
  int   file_max;                 // entries in file list (rename?)
  const char ** file_list;        // list of archive files
} disk_info = {0};

typedef struct
{
  unsigned long length;           // Compressed length
  char filename[14];         // Name of compressed file
  unsigned long final_length;     // Uncompressed length
} file_info_type;

file_info_type file_info = {0};

explode_stats_type explode_stats;

// Used as a callback function.
// Closes old file pointer, opens next file in archive.
// First tries incrementing last letter in filename, ie
// INDY___C.XXX -> INDY___D.XXX
// If that fails, uses next filename in supplied list.
FILE* new_file(void)
{
  unsigned long temp;
  
  fclose(disk_info.fp);
  
  if (disk_info.file_pos >= archive_info.file_length)
  {
    disk_info.file_pos -= archive_info.file_length;
    disk_info.file_pos += 8;
    archive_info.num_disks--;
  }
  
  // [TODO?] Only works on 8.3 filenames
  temp = (disk_info.filename_length>5)?disk_info.filename_length-5:0;
  disk_info.cur_filename[temp]++;
  
  if (!open_archive(&disk_info.fp, disk_info.cur_filename,
                    &archive_info.length, &archive_info.file_length))
  {
    
    // Try next file instead.  A little wonky with filelist.
    if (disk_info.file_index+1 < disk_info.file_max)
    {
      
      disk_info.filename_length = strlen(disk_info.
                                         file_list[disk_info.
                                                   file_index+1]);
      
      if (disk_info.filename_length < 256)
      {
        strcpy(disk_info.cur_filename,
               disk_info.file_list[disk_info.file_index+1]);
      }
      else
      {
        printf("\nError: Continued file not found. Extraction incomplete.\n");
        return NULL;
      }
      
      disk_info.file_name = strrchr(disk_info.cur_filename, '/');
      if (!disk_info.file_name)
      {
        disk_info.file_name = strrchr(disk_info.cur_filename, '\\');
        if (!disk_info.file_name)
          disk_info.file_name = disk_info.cur_filename;
        else
          disk_info.file_name++;
      }
      else
        disk_info.file_name++;
      
      if (!open_archive(&disk_info.fp, disk_info.cur_filename,
                        &archive_info.length, &archive_info.file_length))
      {
        printf("\nError: Continued file not found. Extraction incomplete.\n");
        return NULL;
      }
      
      disk_info.file_index++;
    }
    else
    {
      printf("\nError: Continued file not found. Extraction incomplete.\n");
      return NULL;
    }
  }
  archive_info.total_length += archive_info.file_length;
  
  if (verbose == VERBOSE_LEVEL_HIGH)
  {
    printf( "  (%10ld )", read_buffer_get_bytes_read());
    printf( "  (%10ld )\n", write_buffer_get_bytes_written());
    printf("\n%s         %7ld bytes:\n", disk_info.file_name,
           archive_info.file_length);
    printf( "  %-12s ", file_info.filename);
  }
  
  return disk_info.fp;
}


int read_lfg_archive(int file_max,
                     const char * file_list[],
                     bool info_only,
                     bool show_stats,
                     verbose_level_enum verbose_level,
                     bool overwrite_flag,
                     const char* output_dir)
{
  int file_index = 0;
  bool isNotEnd = true;
  char temp_buff[6];
  const char exp_buff[6] = {2,0,1,0,0,0};
  bool file_error = false;
  
  // Profiling
  clock_t start, stop;
  double elapsed_time = 0;
  
  // Output (extracted) file pointer.
  FILE* out_fp = NULL;
  
  // Use supplied path if exists.
  char* complete_filename = NULL;
  long file_length = 0;
  
  verbose = verbose_level;
  archive_info.total_length = 0;
  
  disk_info.file_index = file_index;
  disk_info.filename_length = strlen(file_list[disk_info.file_index]);
  disk_info.file_max = file_max;
  disk_info.file_list = file_list;
  
  if (disk_info.filename_length < 256)
  {
    strcpy(disk_info.cur_filename, file_list[disk_info.file_index]);
  }
  else
  {
    return 0;
  }
  
  disk_info.file_name = strrchr(disk_info.cur_filename, '/');
  if (!disk_info.file_name)
  {
    disk_info.file_name = strrchr(disk_info.cur_filename, '\\');
    if (!disk_info.file_name)
      disk_info.file_name = disk_info.cur_filename;
    else
      disk_info.file_name++;
  }
  else
    disk_info.file_name++;
  
  if (!open_archive(&disk_info.fp, disk_info.cur_filename,
                    &archive_info.length, &archive_info.file_length))
  {
    printf("\nError opening file %s.\n\n", disk_info.cur_filename);
    return 0;
  }
  archive_info.total_length += archive_info.file_length;
  
  file_error |= !read_chunk(disk_info.fp, archive_info.filename, 13);
  file_error |= !read_expected_byte(disk_info.fp, 0);
  file_error |= !read_chunk(disk_info.fp, &archive_info.num_disks, 1);
  file_error |= !read_expected_byte(disk_info.fp, 0);
  file_error |= !read_uint32(disk_info.fp, &archive_info.space_needed);
  
  if (file_error)
  {
    printf("%s does not appear to be a valid initial LFG archive.\n\n",
           disk_info.cur_filename);
    fclose (disk_info.fp);
    return 0;
  }
  
  if (archive_info.num_disks == 0)
  {
    printf("Warning: Disk count of 0 indicated. File may be corrupted.\n");
  }
  
  if (verbose != VERBOSE_LEVEL_SILENT)
  {
    printf( "Reported archive name: \t\t\t%s\n", archive_info.filename );
    printf( "Disk count: \t\t\t\t%u\n", archive_info.num_disks );
    printf("Space needed for extraction: \t\t%lu bytes\n",
           archive_info.space_needed);
    printf("\n");
    
    if (!info_only)
    {
      if (output_dir)
        printf("Extracting files to %s...\n", output_dir);
      else
        printf( "Extracting files...\n" );
    }
    else
      printf( "Archived file info:\n" );
    
    printf("                    Archived      Extracted             ");
    printf("Literal   Dictionary" );
    
    if (show_stats)
      printf("   Literal  Dictionary      Min/Max     Min/Max     Elapsed");
    
    printf("\n  Filename          size (B)       size (B)    Ratio    ");
    printf("   mode     size (B)");
    
    if (show_stats)
      printf("     count     lookups       offset      length    time (s)");
    
    printf("\n------------------------------------------------------------------------------");
    
    if (show_stats)
      printf("---------------------------------------------------------------");
    
    printf("\n");
    
    if (verbose == VERBOSE_LEVEL_HIGH)
    {
      printf("%s         %7ld bytes:\n", disk_info.file_name,
             archive_info.file_length);
    }
  }
  
  while (isNotEnd && isFileNext(disk_info.fp))
  {
    file_error |= !read_uint32(disk_info.fp, &file_info.length);
    
    disk_info.file_pos = ftell(disk_info.fp);
    
    file_error |= !read_chunk(disk_info.fp, file_info.filename, 13);
    
    // Meaning of this value unknown.
    file_error |= !read_chunk(disk_info.fp, temp_buff, 1);
    
    file_error |= !read_uint32(disk_info.fp, &file_info.final_length);
    
    // Meaning of value  unknown
    file_error |= !read_chunk(disk_info.fp, temp_buff, 6);
    
    if (file_error)
    {
      printf("Unexpected end of file %s.\n\n", disk_info.cur_filename);
      fclose (disk_info.fp);
      return 0;
    }
    
    if (memcmp(temp_buff, exp_buff, 6) != 0)
    {
      printf("Warning: Unexpected values in header. File may be corrupted.\n");
    }
    
    disk_info.file_pos += file_info.length;
    
    if (verbose != VERBOSE_LEVEL_SILENT)
    {
      printf("  %-13s",  file_info.filename);
    }
    
    disk_info.file_count++;
    
    disk_info.bytes_read_so_far += file_info.length;
    disk_info.bytes_written_so_far  += file_info.final_length;
    
    if ( disk_info.file_pos >= archive_info.file_length )
    {
      isNotEnd = false;
    }
    
    if (!info_only)
    {
      if (output_dir)
      {
        file_length = strlen(output_dir) + 1;
      }
      
      file_length += strlen(file_info.filename) + 1;
      complete_filename = (char*)malloc(file_length);
      
      if (output_dir)
      {
        strcpy(complete_filename, output_dir);
        strcat(complete_filename, "/");
        strcat(complete_filename, file_info.filename);
      }
      else
      {
        strcpy(complete_filename, file_info.filename);
      }
      
      // Check if file exists. Not handling any race condition
      // in which file is created after check by another process.
      // (Not worth the trouble here at least)
      if ((!overwrite_flag) &&
          ((out_fp = fopen(complete_filename, "r"))))
      {
        fclose(out_fp);
        
        printf("\nError: File %s already exists.\n",
               complete_filename);
        
        return -1;
      }
      
      
      //temp
      // printf("\n%s\n", complete_filename);
      //    for (int a=0; a<strlen(complete_filename); a++) printf("%x ",complete_filename[a]);
      //    printf("\n");
      
      
      // Open the output file.
      out_fp=fopen(complete_filename, "wb+");
      if (out_fp == 0)
      {
        printf("\nError: Failure while creating file %s.\n",
               complete_filename);
        return -1;
      }
    }
    
    start = clock();
    
    (void) extract_and_explode( disk_info.fp,
                               out_fp,
                               file_info.final_length,
                               &explode_stats,
                               &new_file );
    
    stop = clock();
    
    if (!info_only)
    {
      fclose(out_fp);
      free(complete_filename);
    }
    out_fp=NULL;
    complete_filename=NULL;
    
    elapsed_time = (double)(stop - start) / CLOCKS_PER_SEC;
    
    
    if (verbose != VERBOSE_LEVEL_SILENT)
    {
      printf("   %10ld",  file_info.length+8);
      printf("     %10ld", file_info.final_length);
      printf(" %8.2f\%%", 100-(float)((file_info.length+8) * 100) / file_info.final_length);
      
      if (explode_stats.literal_mode==1) //IMPLODE_ASCII)
      {
        printf("     ASCII");
      }
      else
      {
        printf("    BINARY");
      }
      
      printf("         %4d", 1<<(explode_stats.dictionary_size+6));
      
      if (show_stats )
      {
        printf("%10ld  %10ld",
               explode_stats.literal_count, explode_stats.dictionary_count);
        
        if (explode_stats.dictionary_count!=0)
        {
          printf("     %2d, %4d     %2d, %3d",
                 explode_stats.min_offset, explode_stats.max_offset,
                 explode_stats.min_length, explode_stats.max_length);
        }
        else
        {
          printf("          N/A         N/A");
        }
        printf("     %7.3f", elapsed_time);
      }
      printf("\n");
    }
    
    while (/*info_only &&*/ archive_info.num_disks &&
           (disk_info.file_pos > archive_info.file_length ))
    {
      (void)new_file();
    }
    
    fseek(disk_info.fp, disk_info.file_pos, SEEK_SET);
    
    // check for error in new_file here...?
    if (archive_info.num_disks && !isNotEnd)
    {
      isNotEnd = true;
    }
  }
  
  if( disk_info.file_pos < archive_info.file_length ) {
    printf( "Warning: Unexpected end of file data.\n" );
  }
  
  if (verbose != VERBOSE_LEVEL_SILENT)
  {
    printf("------------------------------------------------------------------------------" );
    if (show_stats)
      printf("---------------------------------------------------------------");
    printf("\n %3d files        %10ld bytes%9ld bytes\n",
           disk_info.file_count, archive_info.total_length,
           disk_info.bytes_written_so_far );
    printf ("\n");
  }
  
  fclose(disk_info.fp);
  
  return ++disk_info.file_index;
}
