//
//  LFGMake.c
//
//  Created by Seltmann Software on 6/17/16.
//  Copyright © 2016,2017,2018,2020 Seltmann Software. All rights reserved.
//

#include "DOSTYPES.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PACK_LFG.H"

#define LFG_MAKE_VERSION_MAJOR 1
#define LFG_MAKE_VERSION_MINOR 6

void print_usage ( void )
{
  printf("Usage: LFGMake [options] archive_name archive_file_1 archive_file_2 ... \n");
  printf("Creates an LFG-type archive.\n\n");
  printf("Options:\n");
  printf("  -f filelist           Use filelist (text file) as archive file list\n");
  printf("  -h                    Display this help\n");
  printf("  -m initial_size size  Set max size for first and subsequent archive files\n");
  printf("  -o optimize level     0-5 (0 is fast; 1,3 look ahead versions; 5 find best)\n");
  printf("  -s                    Print stats\n");
  printf("  -t                    Use ASCII (text) mode encoding of literals\n");
  printf("  -v                    Print version info\n");
  printf("  -w N                  Force sliding window size of N k (where N=1,2,4)\n\n");
}

void print_version ( void )
{
  printf("\nLFGMake V%d.%d\n",
         LFG_MAKE_VERSION_MAJOR,
         LFG_MAKE_VERSION_MINOR);
  printf("(c) Seltmann Software, 2016-2020\n\n");
}

int main (int argc, const char * argv[])
{    
  int file_arg = 1;
  lfg_window_size_type dictionary_size = LFG_DEFAULT;
  const char* file_list = NULL;
  FILE * list_ptr = NULL;
  char buffer[256];
  char* file_list_ptr[256];
  unsigned long disk_size = 0xFFFFFFFF;
  unsigned long first_disk = 0xFFFFFFFF;
  bool verbose = false;
  unsigned int literal_mode = 0;
  unsigned int optimize_level = 3;
  int i,j;
  int file_count = 0;
  
  for (j = 1; j<argc; j++)
  {
    if (strcmp(argv[j], "-t") == 0)
    {
      file_arg++;
      literal_mode = 1;
    }
    else if (strcmp(argv[j], "-o") == 0)
    {
      j++;
      file_arg+=2;
      if (j >= argc)
      {
        print_version();
        return 0;
      }
      optimize_level = atoi(argv[j]);
    }
    else if (strcmp(argv[j], "-f") == 0)
    {
      j++;
      file_arg+=2;
      if (j >= argc)
      {
        print_version();
        return 0;
      }
      file_list = argv[j];
    }
    else if (strcmp(argv[j], "-m") == 0)
    {
      j++;
      file_arg+=3;
      if (j >= argc-1)
      {
        print_version();
        return 0;
      }
      first_disk = atol(argv[j++]);
      disk_size = atol(argv[j]);
    }
    else if (strcmp(argv[j], "-v") == 0)
    {
      print_version();
      return 0;
    }
    else if (strcmp(argv[j], "-h") == 0)
    {
      print_usage();
      return 0;
    }
    else if (strcmp(argv[j], "-s") == 0)
    {
      verbose = true;
      file_arg++;
    }
    else if (strcmp(argv[j], "-w") == 0)
    {
      file_arg+=2;
      j++;
      
      if (j<argc)
      {
        int value;
        value = atoi(argv[j]);
        switch ( value )
        {
          case 1:
            dictionary_size = LFG_WINDOW_1K;
            break;
            
          case 2:
            dictionary_size = LFG_WINDOW_2K;
            break;
            
          case 4:
            dictionary_size = LFG_WINDOW_4K;
            break;
            
          default:
            print_usage();
            return 0;
        }
      }
    }
  }
  
  if (file_arg >= argc)
  {
    print_usage();
    return 0;
  }
  
  // if read from file
  if (file_list != NULL)
  {
    list_ptr = fopen(file_list, "r");
    
    if (!list_ptr)
    {
      printf("%s not found!\n", file_list);
      return 0;
    }
    
    while ( fgets (buffer, 255, list_ptr)!=NULL )
    {
      long length;
      
      buffer[strcspn(buffer, "\r\n")] = 0;
      length = (long)strlen( buffer );
      if (length)
      {
        file_list_ptr[file_count] = (char*)malloc( sizeof(char) * length + 1);
        strcpy(file_list_ptr[file_count], buffer);
        file_count++;
      }
    }
    
    fclose(list_ptr);
  }
  else
  {
    for (i=file_arg+1; i<argc; i++)
    {
      long length = (long)strlen( argv[i] );
      file_list_ptr[file_count] = (char *)malloc( sizeof(char) * length + 1);
      strcpy(file_list_ptr[file_count], argv[i]);
      file_count++;
    }
  }
  
  //if -debug
  //printf("LFGMake will compress the following files:\n");
  //for (int i = 0; i< file_count; i++)
  //{
  //    printf(" %d: %s\n", i+1, file_list_ptr[i]);
  //}
  
  pack_lfg(dictionary_size,
           literal_mode,
           argv[file_arg],
           file_list_ptr,
           file_count,
           first_disk,
           disk_size,
           optimize_level,
           verbose);
  
  // Free file list
  for (i=0; i< file_count; i++)
  {
    free(file_list_ptr[i]);
  }
  
  return 0;
}

