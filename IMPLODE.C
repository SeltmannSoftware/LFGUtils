//
//  implode.c
//  LFGMake
//
//  Created by Kevin Seltmann on 6/17/16.
//  Copyright © 2016 Kevin Seltmann. All rights reserved.
//
//  Designed to "implode" files in a similar fashion to the PKWARE Data
//  Compression Library (DCL) from ~1990.  Implementation based on
//  specification found on the internet

#include "IMPLODE.H"
#include "DOSTYPES.H"

#define MIN(x,y)  ((x)<(y))?(x):(y)    
// Minimum of 2 values

#define ENCODE_BUFF_SIZE         0x2000
// Buffer for the file to encode.  Must be larger than dictionary and have
// at least 518 bytes of future data (1K works).
// Maximum dictionary size is 4K (0x1000); here, 8K is used.

#define ENCODE_BUFF_MASK         0x1800
// Masked MSBs of the encoder buffer are used to determine when
// new data should be loaded from the file into the buffer.
// Here, new data is loaded every 0x800 or 2K.

#define ENCODE_BUFF_LOAD_SIZE     0x800
// Amount of data to be loaded at a time into buffer.
// Effectively for 8k buffer we have 4-6K history
// and 2-4K of future data depending where encode index is.

#define ENCODE_BUFF_LOAD_DONE    0xFFFF
// Indicates that EOF has been reached on input file.
// Just needs to be > BUFF SIZE

#define ENCODE_MIN_OFFSET 0
// Minimum offset to use for encoding.
// DCL appears to have used 1.

#define ENCODE_MAX_OFFSET ( dictionary_size_bytes )
// Maximum offset to use for encoding.  Maximum possible is the
// dictionary size.
// DCL appears to have used the dictionary size - 2.
// (i.e., 0 through 4094 rather than 0 through 4095.)

#define ENCODE_MAX_LENGTH 518
// Maximum length to use for encoding.  Max possible is 518.
// DCL appears to have used 516.

unsigned char encoding_buffer[ENCODE_BUFF_SIZE];
//unsigned int encode_index;
unsigned int dictionary_size_bytes;  // 0x1000, 0x800, 0x400 for 4k, 2k, 1k
unsigned int dictionary_size_bits;   // 4,5, or 6
unsigned long bytes_encoded;
unsigned long bytes_length;
implode_literal_type literal_mode = IMPLODE_BINARY;


// -- BIT WRITE ROUTINES --
typedef struct {
  
  // Output file pointer. (Could be replaced by access function.)
  FILE* file_pointer;
  
  // Signals a write error
  int error_flag;
  
  unsigned long bytes_written;
  
  unsigned long *max_length;
  
  FILE* (*max_reached)( FILE* , unsigned long*);
  
} write_bitstream_type;

write_bitstream_type write_bitstream;

void ffputc( unsigned char val )
{
  if (write_bitstream.file_pointer)
  {
    fputc(val, write_bitstream.file_pointer);
    
    // Check if error is reported.
    if (ferror(write_bitstream.file_pointer)) {
      printf("Error: file error.\n");
      write_bitstream.error_flag = true;
    }
  }
  
  write_bitstream.bytes_written++;
  
  if ((write_bitstream.max_length) && (write_bitstream.file_pointer))
  {
    if (write_bitstream.bytes_written>=*write_bitstream.max_length)
    {
      if (write_bitstream.max_reached)
      {
        write_bitstream.file_pointer =
        write_bitstream.max_reached( write_bitstream.file_pointer,
                                    write_bitstream.max_length );
        *write_bitstream.max_length+=write_bitstream.bytes_written;
      }
    }
  }
}


/* Bit write functions and supporting structures */
struct {
  // The current byte that is being built.
  unsigned char byte_value;
  
  // Bit position in current byte for the next bit to be written.
  int bit_position;
} write_bits;

// Write a single bit.
void write_next_bit( unsigned int bit )
{
  
  write_bits.byte_value |=
  (bit & 0x1) << write_bits.bit_position;
  
  if (write_bits.bit_position == 7)
  {
    ffputc(write_bits.byte_value);
    
    write_bits.byte_value = 0;
  }
  
  write_bits.bit_position++;
  write_bits.bit_position%=8;
}

// Flush remaining bits to next byte.
void write_flush( void )
{
  int j = (8-write_bits.bit_position) % 8;
  int i;
  
  for (i=0; i<j; i++){
    write_next_bit(0);
  }
}

// Write some bits, msb first.
// Max bit_count is (sizeof(int)/8). No error checking performed.
void write_bits_msb_first( unsigned int bit_count,
                          unsigned int bits)
{
  int i;
  
  for (i=bit_count-1; i>=0;i--)
  {
    write_next_bit((bits >> i) & 1);
  }
}

// Write some bits, lsb first.
// Max bit_count is (sizeof(int)/8). No error checking is performed.
void write_bits_lsb_first( unsigned int bit_count,
                           unsigned int bits)
{
  unsigned int i;
  
  for (i=0; i<bit_count;i++)
  {
    write_next_bit((bits >> i) & 1);
  }
}


// --- Routines for finding encoded literal ---
// Lookup table for converting dictionary offset to bit codes.
struct {
  unsigned int lookup_min;
  unsigned int bit_count;
  unsigned int bit_value;
} literal_to_bits_table[] =
{
  { 182, 13, 0x49 },
  {  91, 12, 0x7F },
  {  81, 11, 0x49 },
  {  76, 10, 0x29 },
  {  69,  9, 0x1b },
  {  53,  8, 0x1d },
  {  32,  7, 0x23 },
  {  12,  6, 0x25 },
  {  01,  5, 0x1d },
  {  00,  4, 0x0f }
};

unsigned char literal_table[256] = {
  0x20,                                             // 0
  0x45, 0x61, 0x65, 0x69, 0x6c, 0x6e, 0x6f,   // 1
  0x72, 0x73, 0x74, 0x75,
  0x2d, 0x31, 0x41, 0x43,   // 12
  0x44, 0x49, 0x4c, 0x4e, 0x4f, 0x52, 0x53, 0x54,
  0x62, 0x63, 0x64, 0x66, 0x67, 0x68, 0x6d, 0x70,
  0x0a, 0x0d, 0x28, 0x29, 0x2c, 0x2e, 0x30, 0x32,   // 32
  0x33, 0x34, 0x35, 0x37, 0x38, 0x3d, 0x42, 0x46,
  0x4d, 0x50, 0x55, 0x6b, 0x77,
  0x09, 0x22, 0x27,   // 53
  0x2a, 0x2f, 0x36, 0x39, 0x3a, 0x47, 0x48, 0x57,
  0x5b, 0x5f, 0x76, 0x78, 0x79,
  0x2b, 0x3e, 0x4b,  // 69
  0x56, 0x58, 0x59, 0x5d,
  0x21, 0x24, 0x26, 0x71,   // 76
  0x7a,
  0x00, 0x3c, 0x3f, 0x4a, 0x51, 0x5a, 0x5c,   // 81
  0x6a, 0x7b, 0x7c,
  0x01, 0x02, 0x03, 0x04, 0x05,   // 91
  0x06, 0x07, 0x08, 0x0b, 0x0c, 0x0e, 0x0f, 0x10,
  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
  0x19, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x23, 0x25,
  0x3b, 0x40, 0x5e, 0x60, 0x7d, 0x7e, 0x7f, 0xb0,
  0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
  0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0,
  0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
  0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
  0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
  0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe1,
  0xe5, 0xe9, 0xee, 0xf2, 0xf3, 0xf4,
  0x1a, 0x80,  // 182
  0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
  0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90,
  0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
  0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0,
  0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8,
  0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xe0,
  0xe2, 0xe3, 0xe4, 0xe6, 0xe7, 0xe8, 0xea, 0xeb,
  0xec, 0xed, 0xef, 0xf0, 0xf1, 0xf5, 0xf6, 0xf7,
  0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

unsigned char literal_lookup[256];

void literal_init( void )
{
  int i;
  for (i = 0; i < 256; i++)
  {
    literal_lookup[literal_table[i]]=i;
  }
}

// Search through table to return number of bits and value of bits
// for encoding given literal. Literal to be found is a single byte (8 bits).
void find_literal_codes(unsigned int literal,
                        unsigned int * literal_bits,
                        unsigned int * literal_code)
{
  int i, delta;
  unsigned int literal_index = literal_lookup[literal];
  
  // Look through table for entry in range of the literal we want.
  for (i = 0; literal_index < literal_to_bits_table[i].lookup_min; i++ );
  
  // Use table data to find number of bits and the bit sequence to
  // represent the value.
  delta = literal_index - literal_to_bits_table[i].lookup_min;
  *literal_bits = literal_to_bits_table[i].bit_count;
  *literal_code = literal_to_bits_table[i].bit_value - delta;
}


void write_literal( unsigned int literal_val )
{
  unsigned int literal_bits;
  unsigned int literal_code;
  
  write_next_bit(0);
  
  if( literal_mode == IMPLODE_BINARY )
  {
    write_bits_lsb_first(8, literal_val);
  }
  else
  {
    find_literal_codes(literal_val, &literal_bits, &literal_code);
    write_bits_msb_first(literal_bits, literal_code);
  }
};

int length_literal( unsigned int literal_val )
{
  unsigned int literal_bits;
  unsigned int literal_code;
  
  if( literal_mode == IMPLODE_BINARY )
  {
    literal_bits = 8;
  }
  else
  {
    find_literal_codes(literal_val, &literal_bits, &literal_code);
  }
  
  return literal_bits+1;
};

// --- Routines for finding length and offset ---

// Lookup table for coverting dictionary offset to bit codes.
struct {
  unsigned int lookup_min;
  unsigned int bit_count;
  unsigned int bit_value;
} offset_to_bits_table[] =
{
  { 0x30, 8, 0x0F },
  { 0x16, 7, 0x21 },
  { 0x07, 6, 0x1F },
  { 0x03, 5, 0x13 },
  { 0x01, 4, 0x0B },
  { 0x00, 2, 0x03 }
};

// Lookup table for converting dictionary length to bit codes.
struct {
  unsigned int lookup_min;
  unsigned int bit_count;
  unsigned int bit_value;
  unsigned int lsb_count;
} length_table[] =
{
  { 264, 7, 0, 8},
  { 136, 7, 1, 7},
  {  72, 6, 1, 6},
  {  40, 6, 2, 5},
  {  24, 6, 3, 4},
  {  16, 5, 2, 3},
  {  12, 5, 3, 2},
  {  10, 5, 4, 1},
  {   9, 5, 5, 0},
  {   8, 4, 3, 0},
  {   7, 4, 4, 0},
  {   6, 4, 5, 0},
  {   5, 3, 3, 0},
  {   4, 3, 4, 0},
  {   3, 2, 3, 0},
  {   2, 3, 5, 0}
};

// Search through table to return number of bits and value of bits
// for encoding given offset.
void find_offset_codes(unsigned int offset,
                       unsigned int * offset_bits,
                       unsigned int * offset_code)
{
  int i, delta;
  
  // Look through table for entry in range of the offset we want.
  for (i = 0; offset < offset_to_bits_table[i].lookup_min; i++ );
  
  delta = offset - offset_to_bits_table[i].lookup_min;
  *offset_bits = offset_to_bits_table[i].bit_count;
  *offset_code = offset_to_bits_table[i].bit_value - delta;
}

// Search through table to return number of bits, value of bits,
// length of lsb bits, and value of lsb bits for given length.
void find_length_codes(unsigned int length,
                       unsigned int * length_bits,
                       unsigned int * length_code,
                       unsigned int * length_lsb_count,
                       unsigned int * length_lsb_value )
{
  int i;
  
  // Look through table for entry in range of the length we want.
  for (i = 0;length < length_table[i].lookup_min; i++ );
  
  *length_bits = length_table[i].bit_count;
  *length_code = length_table[i].bit_value;
  *length_lsb_count = length_table[i].lsb_count;
  *length_lsb_value = length - length_table[i].lookup_min;
}

// Find offset bits, length bits and write to file.
void write_dictionary_entry( int offset, int length )
{
  unsigned int low_offset_bits;
  unsigned int high_offset_bits;
  unsigned int offset_msb_code;
  unsigned int length_bits;
  unsigned int length_code;
  unsigned int length_lsb_bits;
  unsigned int length_lsb_value;
  
  if (length!= 2)
  {
    low_offset_bits = dictionary_size_bits;
  }
  else
  {
    low_offset_bits = 2;
  }
  
  find_length_codes(length,
                    &length_bits,
                    &length_code,
                    &length_lsb_bits,
                    &length_lsb_value);
  
  write_next_bit(1);
  write_bits_msb_first( length_bits, length_code);
  write_bits_lsb_first( length_lsb_bits, length_lsb_value);
  
  find_offset_codes(offset>>low_offset_bits,
                    &high_offset_bits,
                    &offset_msb_code);
  
  write_bits_msb_first( high_offset_bits, offset_msb_code);
  write_bits_lsb_first( low_offset_bits, offset);
}

// Find the bit length of a offset, length pair without writing it.
int length_dictionary_entry( int offset, int length)
{
  unsigned int low_offset_bits;
  unsigned int high_offset_bits;
  unsigned int offset_msb_code;
  unsigned int length_bits;
  unsigned int length_code;
  unsigned int length_lsb_bits;
  unsigned int length_lsb_value;
  
  int bit_length = 1;
  
  if (length != 2)
  {
    low_offset_bits = dictionary_size_bits;
  }
  else
  {
    low_offset_bits = 2;
  }
  
  bit_length+=low_offset_bits;
  
  find_length_codes(length,
                    &length_bits,
                    &length_code,
                    &length_lsb_bits,
                    &length_lsb_value);
  
  bit_length += length_bits + length_lsb_bits;    
  
  find_offset_codes(offset>>low_offset_bits,
                    &high_offset_bits,
                    &offset_msb_code);
  
  bit_length += high_offset_bits;
  
  return bit_length;
}


// Check dictionary for a byte sequence match at a particular position.
// Target sequence is already in buffer (as those are the bytes being encoded).
// Takes the buffer (bytes), two positions within the buffer,
// the max length of the byte sequence to compare, and
// the circular buffer size.
int compare_in_circular( unsigned char * buffer,
                        unsigned int position1,
                        unsigned int position2,
                        long max_length,
                        long circular_buffer_size)
{
  int i = 0;
  
  do
  {
    position1 %= circular_buffer_size;
    position2 %= circular_buffer_size;
    
    if (buffer[position1] != buffer[position2])
    {
      break;
    }
    
    position1++;
    position2++;
    i++;
  } while (i<max_length);
  
  return i;
}


// Look in dictionary for sequence
// TRUE if sequence is found
bool check_dictionary( unsigned int* length,           // length found
                      unsigned int* offset,           // offset found
                      unsigned char *encoding_buffer, // dictionary
                      unsigned int encoding_index)    // start index
{
  bool match_found = false;
  long search_size = MIN(ENCODE_MAX_OFFSET, bytes_encoded);
  unsigned int offset_val = 0;
  int final_length = 1;
  int length_now;
  int i;
  long max_length = MIN(bytes_length - bytes_encoded, ENCODE_MAX_LENGTH);
  
  for (i=(ENCODE_MIN_OFFSET+1); i<=search_size; i++)
  {
    length_now = compare_in_circular( encoding_buffer,
                                     encoding_index,
                                     encoding_index-i,
                                     max_length,
                                     ENCODE_BUFF_SIZE );
    
    // If the found length is greater than the length found so far,
    // update length, remember the offset, continue looking.
    // 
    if (length_now > final_length)
    {
      final_length=length_now;
      offset_val = i-1;
      match_found = true;
    }
  }
  
  // Fill in the offset and length of the match
  if (match_found)
  {
    *offset = offset_val;
    *length = final_length;
  }
  
  // Validate length of 2
  if ((final_length == 2) && (offset_val > 255))
    match_found = false;
  
  return match_found;
}

unsigned long implode(FILE * in_file,
                      FILE * out_file,
                      unsigned long length,
                      implode_literal_type literal_encode_mode,
                      implode_dictionary_size_type dictionary_size,
                      unsigned int optimization_level,
                      implode_stats_type* implode_stats,
                      unsigned long *max_length,
                      FILE* (*max_reached)(FILE* , unsigned long*) )
{
  unsigned int encode_length = 0;
  long bytes_loaded;
  int optimize_type = optimization_level;
  unsigned int next_load_point = 0;
  unsigned int encode_index = 0;
  literal_mode = literal_encode_mode;
  //encode_index = 0;
  bytes_encoded = 0;
  bytes_length = length;
  
  // Init bitstream data
  write_bitstream.bytes_written = 0;
  write_bitstream.max_length = max_length;
  write_bitstream.max_reached = max_reached;
  write_bitstream.file_pointer = out_file;
  
  // range check dictionary
  dictionary_size_bytes = 1 << (dictionary_size + 6);
  dictionary_size_bits = dictionary_size;
  
  // Initialize statistics.
  if (implode_stats)
  {
    implode_stats->literal_count = 0;
    implode_stats->lookup_count = 0;
    implode_stats->max_offset = 0;
    implode_stats->min_offset = dictionary_size_bytes;
    implode_stats->max_length = 0;
    implode_stats->min_length = 1024;
  }
  
  literal_init();
  
  bytes_loaded = fread( encoding_buffer,
                       sizeof encoding_buffer[0],
                       ENCODE_BUFF_LOAD_SIZE,
                       in_file);
  
  // File is shorter than our buffer. Mark no more loads.
  if ( bytes_loaded != ENCODE_BUFF_LOAD_SIZE )
  {
    next_load_point = ENCODE_BUFF_LOAD_DONE;
  }
  else
  {
    next_load_point = 0;
  }
  
  ffputc(literal_mode);
  ffputc(dictionary_size_bits);
  
  // While there are bytes to encode...
  while (bytes_encoded < length)
  {
    unsigned int offset;
    bool use_literal = true;
    
    // Check if data should be loaded into buffer.
    if ((encode_index & ENCODE_BUFF_MASK) ==
        (next_load_point & ENCODE_BUFF_MASK))
    {
      next_load_point+=ENCODE_BUFF_LOAD_SIZE;
      next_load_point%=ENCODE_BUFF_SIZE;
      
      if (fread(&encoding_buffer[next_load_point],
                sizeof encoding_buffer[0],
                ENCODE_BUFF_LOAD_SIZE,
                in_file)
          != ENCODE_BUFF_LOAD_SIZE)
      {
        // Hit end of file. Signal no more data reads.
        next_load_point = ENCODE_BUFF_LOAD_DONE;
      }
    }
    
    encode_index %= ENCODE_BUFF_SIZE;
    
    // Encoding buffer and dictionary are one and the same.
    // Dictionary is simply bytes that have already been encoded.
    // Check for the longer run of next bytes in the dictionary.
    if (check_dictionary(&encode_length, &offset,
                         encoding_buffer, encode_index))
    {
      // Versions A,B,D -- different attempts to improve
      //  compression. Common code start.
      
      // By the time we are here, encode length must be >= 2
      // and encode_length + bytes already encoded should not
      // exceed the file length
      
      use_literal = false;
      
      if (optimize_type>0)
      {
        unsigned int literal_length, literal_offset;
        bool literal_check;
        
        literal_check = check_dictionary(&literal_length,
                                         &literal_offset,
                                         encoding_buffer,
                                         (encode_index+1) %
                                         ENCODE_BUFF_SIZE);
        
        // Version B - only the below code. Version D uses also.
        if (optimize_type>1)
        {
          int sequence_bits, sequence_length;
          int possible_bitcount, bitcount_with_literal;
          float bits_per_byte,bits_per_byte_lit;
          
          // A sequence was found, but now see if a better match can be
          // found if we use a literal for next byte, then sequence.
          if ( literal_check )
          {
            // Compare the overall bit ratio for each case.
            possible_bitcount =
            length_dictionary_entry(offset, encode_length);
            bitcount_with_literal =
            length_dictionary_entry(literal_offset, literal_length);
            
            bits_per_byte = (float)possible_bitcount / encode_length;
            bits_per_byte_lit = (float)(bitcount_with_literal + length_literal(encoding_buffer[encode_index])) //9)
            / (literal_length + 1);
            
            // For some reason, better results are produced when
            // bias towards using literal. (<= rather than <)
            if (bits_per_byte_lit <= bits_per_byte)
            {
              use_literal = true;
              
              // Now we can check if this same sequence can do better if
              // used with the original sequence.
              sequence_length = literal_length + 1 - encode_length;
              if ( sequence_length > 0 )
              {
                if ( sequence_length == 1 )
                {
                  sequence_bits = length_literal(encoding_buffer[encode_index + encode_length]); //9;
                }
                else
                {
                  if ((sequence_length == 2) &&
                      (literal_offset > 255))
                  {
                    sequence_bits = length_literal(encoding_buffer[encode_index + encode_length]) +
                    length_literal(encoding_buffer[encode_index + encode_length+1]);//18;
                  }
                  else
                  {
                    sequence_bits = length_dictionary_entry(
                                                            literal_offset,
                                                            sequence_length);
                  }
                }
                
                if (( possible_bitcount + sequence_bits) <=
                    ( bitcount_with_literal + length_literal(encoding_buffer[encode_index]))) //9))
                {
                  use_literal = false;
                }
                
              }
            }
          }
        }
        
        if ((optimize_type==1) || (optimize_type ==3))
        {
          // Version A is below only.  Version C,D combines with above.
          unsigned int next_length, next_offset;
          
          if (!literal_check)
          {
            literal_length = 1;
          }
          literal_length += 1;
          
          if ((encode_length==2)  && (offset > 255))
          {
            next_length=0;
          }
          else
          {
            if (!check_dictionary(&next_length,
                                  &next_offset,
                                  encoding_buffer,
                                  (encode_index+encode_length) %
                                  ENCODE_BUFF_SIZE))
            {
              next_length = 1;
            }
            next_length += encode_length;
          }
          // Version A includes else.
          if (next_length > literal_length)
          {
            use_literal = false;
          }
          else
          {    // For version A
            if (optimize_type==1)
              use_literal = true;
          }
        }
      }
    }
    
    // End Versions A,B,D
    
    // If flag for literal is set, use literal.
    // Otherwise, use dictionary.
    if (use_literal)
    {
      write_literal(encoding_buffer[encode_index]);
      encode_index++;
      bytes_encoded++;
      
      if (implode_stats) implode_stats->literal_count++;
    }
    else
    {
      write_dictionary_entry(offset, encode_length);
      encode_index += encode_length;
      bytes_encoded += encode_length;
      
      if (implode_stats)
      {
        implode_stats->lookup_count++;
        
        if (encode_length > implode_stats->max_length)
          implode_stats->max_length = encode_length;
        if (encode_length < implode_stats->min_length)
          implode_stats->min_length = encode_length;
        if (offset > implode_stats->max_offset)
          implode_stats->max_offset = offset;
        if (offset < implode_stats->min_offset)
          implode_stats->min_offset = offset;
      }
    }
    encode_length=0;
    
  }
  
  // Write end-of-data marker (Length 519) and zero bits for final byte.
  write_next_bit(1);
  write_bits_msb_first(7, 0);
  write_bits_lsb_first(8, 0xFF);
  write_flush();
  
  // fix
  if (max_length)
    *max_length-=write_bitstream.bytes_written;
  
  return write_bitstream.bytes_written;
}

