//
//  explode.c
//  LFGDump V 1.1
//
//  Created by Kevin Seltmann on 6/12/16.
//  Copyright © 2016, 2017 Kevin Seltmann. All rights reserved.
//
//  Designed to extract the archiving used on LucasArts Classic Adventure
//  install files (*.XXX) and other archives created with the PKWARE
//  Data Compression Library from ~1990.  Implementation based on
//  specifications found on the internet.

#include <stdio.h>
#include <string.h>
#include "EXPLODE.H"
#include "DOSTYPES.H"

// -- BIT READ ROUTINES --
typedef struct {

    // Input file pointer.
    FILE* file_pointer;

    // CB function for handling when in file EOF is reached. Used for
    // multi-file archives; handler can return a new file pointer
    // but must be at correct point in the data stream.
    FILE* (*eof_reached) ( void );

    // The current byte from input stream that bits are being pulled from.
    unsigned char current_byte_value;

    // Bit position in current byte. Next bit will come from this position.
    int current_bit_position;

    // Signals a read error
    int error_flag;
    
    // Stats. Used to track total number of encoded bytes read.
    unsigned long total_bytes;
    
} read_bitstream_type;

read_bitstream_type read_bitstream;

// Read bit from bitstream byte.
unsigned int read_next_bit( void )
{
    int ch, value;

    // Start of byte, need to load new byte.
    if (read_bitstream.current_bit_position == 0)
    {
        ch = fgetc(read_bitstream.file_pointer);
        
        // Check that end of file wasn't reached.
	if ((ch==EOF) && (read_bitstream.eof_reached != NULL))
        {
            read_bitstream.file_pointer = read_bitstream.eof_reached();
            
	    if (read_bitstream.file_pointer)
            {
                // New file. Now try to get a byte.
                ch = fgetc(read_bitstream.file_pointer);
            } else {
                // No new file
                read_bitstream.error_flag = true;
	    }
        }
        
        // Error if eof still occurs or a different error is reported.
	if (ch < 0)
        {
            printf("Error: Unexpected end of file or file error.\n");
            read_bitstream.error_flag = true;
        }
        
        read_bitstream.current_byte_value = ch;
	read_bitstream.total_bytes++;

    }
    
    value  = (read_bitstream.current_byte_value >>
              read_bitstream.current_bit_position) & 0x1;
    
    read_bitstream.current_bit_position++;
    read_bitstream.current_bit_position%=8;
    
    return (unsigned int) value;
}

// General function to read bits, and assemble them with MSBs first. Note
// that bits are always read from the byte stream lsb first. What matters here
// is how they are reassembled.
unsigned int read_bits_msb_first( int bit_count )
{
    unsigned int temp = 0;
    int i;

    for (i=0; i<bit_count;i++)
    {
        temp = (temp << 1) | read_next_bit();
    }
    return temp;
}

// General function to read bits, and assemble them with LSBs first.
unsigned int read_bits_lsb_first( int bit_count )
{
    unsigned int temp = 0;
    int i;

    for (i=0; i<bit_count;i++)
    {
        temp = (read_next_bit() << i) | temp;
    }
    return temp;
}


// -- BYTE WRITE BUFFER ROUTINES --

#define WRITE_BUFF_SIZE      0x4000   // ( 16k)

typedef struct {

    // Output file pointer.
    FILE* file_pointer;

    // write position in buffer
    unsigned int buffer_position;

    // total bytes written
    unsigned long bytes_written;

    // Signals a write error
    int error_flag;

    // Write memory buffer
    // Must write out buffer every time the window fills or at file end.
    unsigned char buffer[ WRITE_BUFF_SIZE ];

} write_buffer_type;

write_buffer_type write_buffer;

// Writes output buffer to file
void write_to_file( void )
{
    if (write_buffer.file_pointer)
    {
	fwrite(write_buffer.buffer, sizeof(write_buffer.buffer[0]),
	       write_buffer.buffer_position,
	       write_buffer.file_pointer);

	if (ferror(write_buffer.file_pointer))
	{
	    write_buffer.error_flag = true;
	}
    }
    write_buffer.bytes_written += write_buffer.buffer_position;
}

unsigned long read_buffer_get_bytes_read( void )
{
    return read_bitstream.total_bytes;
}

unsigned long write_buffer_get_bytes_written( void )
{
    return write_buffer.bytes_written + write_buffer.buffer_position;
}

// Write a byte out to the output stream.
void write_byte( unsigned char next_byte )
{
    write_buffer.buffer[write_buffer.buffer_position++] = next_byte;

    if (write_buffer.buffer_position == WRITE_BUFF_SIZE)
    {
	write_to_file();
    }

    write_buffer.buffer_position %= WRITE_BUFF_SIZE;

}

// Read byte from the *output* stream
unsigned char read_byte_from_write_buffer( int offset )
{
    return write_buffer.buffer[(write_buffer.buffer_position-offset)
				  % WRITE_BUFF_SIZE];
}

// -- EXPLODE IMPLEMENTATION --

struct {

    // length for copying from dictionary.
    int length;

    // offset for copying from dictionary.
    int offset;

    // Flags when the end of file marker is read (when length = 519)
    bool end_marker;

    // Statistics
    unsigned long literal_count;
    unsigned long dictionary_count;
    int max_offset;
    int min_offset;
    int max_length;
    int min_length;
    long length_histogram[520];

} explode;

// Header info
struct {
    uint8_t literal_mode;
    uint8_t dictionary_size;
} header;


// Copy offset table; indexed by bit length
struct {
    // Number of codes of this length (number of bits)
    unsigned int count;

    // Base output value for this length (number of bits)
    unsigned int base_value;

    // Base input bits for this number of bits.
    unsigned int base_bits;
    
} offset_bits_to_value_table[] =
{
    { 0, 0x00, 0x00},
    { 0, 0x00, 0x00},
    { 1, 0x00, 0x03},
    { 0, 0x00, 0x00},
    { 2, 0x02, 0x0A},
    { 4, 0x06, 0x10},
    {15, 0x15, 0x11},
    {26, 0x2F, 0x08},
    {16, 0x3F, 0x00}
};

uint8_t literal_table[256] = {
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
    0x5b, 0x5f, 0x76, 0x78, 0x79,   //69
    0x2b, 0x3e, 0x4b,  0x56, 0x58, 0x59, 0x5d, //76
    0x21, 0x24, 0x26, 0x71, 0x7a, //
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


// Literal table; indexed by bit length
struct {
    // Number of codes of this length (number of bits)
    unsigned int count;

    // Base output value for this length (number of bits)
    unsigned int base_value;

    // Base input bits for this number of bits.
    unsigned int base_bits;

} literal_bits_to_index_table[] =
{
    { 0,   0, 0x00},
    { 0,   0, 0x00},
    { 0,   0, 0x00},
    { 0,   0, 0x00},
    { 1,   0, 0x0F},
    {11,  11, 0x13},
    {20,  31, 0x12},
    {21,  52, 0x0F},
    {16,  68, 0x0E},
    { 7,  75, 0x15},
    { 5,  80, 0x25},
    {10,  90, 0x40},
    {91, 181, 0x25},
    {74, 255, 0x00}
};


// Read copy length codes. A fairly brute force method as there is an odd
// switching of msb first to lsb first in the interpretation and the
// values 2 and 3 do not follow the natural huffman-like coding.
int read_copy_length( void )
{
    int length = 0;

    // First two bits (xx)
    switch (read_bits_msb_first(2)) {

	case 0:
	    // Next 2 bits (00xx)
	    switch (read_bits_msb_first( 2)) {

		case 0:
		    // Next 2 bits (0000xx)
		    switch (read_bits_msb_first( 2)) {

			case 0:
			    // Next bit (000000x)
			    if (read_next_bit())
				// 0000001xxxxxxx
				length = 136 + read_bits_lsb_first(7);
			    else
				// 0000000xxxxxxxx
				length = 264 + read_bits_lsb_first(8);
			    break;

			case 1:
			    // 000001xxxxxx
			    length = 72 + read_bits_lsb_first(6);
			    break;

			case 2:
			    // 000010xxxxx
			    length = 40 + read_bits_lsb_first(5);
			    break;

			case 3:
			    // 0000
			    length = 24 + read_bits_lsb_first(4);
		    }
		    break;

		case 1:
			// Next bit (0001x)
		    if (read_next_bit())
			// 00011xx
			length = 12 + read_bits_lsb_first(2);
		    else
			// 00010xxx
			length = 16 + read_bits_lsb_first(3);
		    break;

		case 2:
			// Next bit (0010x)
		    if (read_next_bit()) {
			// 00101
			length = 9;
		    } else {
			// 00100x
			length = 10 + read_next_bit();
		    }
		    break;

		case 3:
		    // 0011
		    length = 8;
	    }
	    break;

	case 1:
			// Next bit (01x)
	    if (read_next_bit()) {
		// 011
		length = 5;
	    } else {
		// Next bit (010x)
		if (read_next_bit()) {
		    // 0101
		    length = 6;
		} else {
		    // 0100
		    length = 7;
		}
	    }
	    break;

	case 2:
		// Next bit (10x)
	    if (read_next_bit()) {
		// 101
		length = 2;
	    } else {
		// 100
		length = 4;
	    }
	    break;

	case 3:
	    // 11
	    length = 3;
    }

    if (length == 0) printf("Error: Copy length returned zero value.\n");

    return length;

}

// Read the offset part of a length/offset reference
int read_copy_offset( void )
{
    int offset = 0;         // The offset value we are looking for.
    int offset_bits = 0;    // Input bits for offset.
    int diff;               // Difference used in calulating with table.
    int num_lsbs;           // Number of lsbs to use.
    int length;             //

    // Get 6 MS bits of the resulting offset
    offset_bits = read_bits_msb_first(2);

    // Go through table by length to see if there is a match.
    for (length = 2; length<9; length++) {

	diff = offset_bits - offset_bits_to_value_table[length].base_bits;

	if ( ( diff >=0 ) &&
	    (diff < offset_bits_to_value_table[length].count) )
	{
	    offset = offset_bits_to_value_table[length].base_value - diff;
	    break;
	}

	offset_bits = (offset_bits << 1) | read_next_bit();
    }

    if (length == 9) printf("\nError: Copy offset value not found.\n");

    // Now get low order bits and append. Length 2 is a special case.
    if (explode.length == 2)
	num_lsbs = 2;
    else
	num_lsbs = header.dictionary_size;

    offset = (offset << num_lsbs) | read_bits_lsb_first(num_lsbs);

    return offset;
}

// Read a literal
unsigned char read_literal( void )
{
    int literal = 0;        // The offset value we are looking for.
    int literal_bits = 0;   // Input bits for offset.
    int diff;               // Difference used in calulating with table.
    int length;             // Bit length

    if (header.literal_mode == 0x1)
    {

    // Get 4
    literal_bits = read_bits_msb_first(4);

    // Go through table by length to see if there is a match.
    for (length = 4; length<14; length++) {

	diff = literal_bits - literal_bits_to_index_table[length].base_bits;

	if ( ( diff >=0 ) &&
	    (diff < literal_bits_to_index_table[length].count) )
	{
	    literal = literal_bits_to_index_table[length].base_value - diff;
	    break;
	}

	literal_bits = (literal_bits << 1) | read_next_bit();
    }

    return literal_table[literal];

    }
    else
    {
      return read_bits_lsb_first(8);
    }

}

void write_dict_data( void )
{
    int i;
    int offset = explode.offset+1;   // +1 since zero should reference the
				     // previous byte.

    // Do this length times. Offset does not change since one byte is
    // added each iteration and we are counting from the end.
    for (i = 0; i < explode.length; i++) {
	write_byte( read_byte_from_write_buffer(offset) );
    }
}

/* Extract a file from an archive file and explode it.
   in_fp:           Pointer to imploded data start in archive file.
   out_filename:    Output filename [consider making this fp_out].
   expected_length: Expected length of file (0 if not provided).
   eof_reached():   Callback that indicates archive EOF is reached.
		    Callback should return new file pointer with
		    the continued data for the imploded file.
 */
long extract_and_explode( FILE* in_fp,
			  FILE* out_fp,
			  long expected_length,
			  explode_stats_type* explode_stats,
			  FILE* (*eof_reached)(void))
{
    // Set up read parameters. [Consider making this a function.]
    read_bitstream.file_pointer = in_fp;
    read_bitstream.eof_reached = eof_reached;
    read_bitstream.current_bit_position = 0;
    read_bitstream.error_flag = 0;
    read_bitstream.total_bytes = 0;

    // Reset write parameters. [ Consider making this a function. ]
    write_buffer.bytes_written = 0;
    write_buffer.error_flag = 0;
    write_buffer.buffer_position = 0;
    write_buffer.file_pointer=out_fp;

    // Reset counters/markers.
    explode.end_marker = false;
    explode.length = 0;
    explode.offset = 0;

    // Initialize statistics.
    explode.literal_count = 0;
    explode.dictionary_count = 0;
    explode.max_offset = 0;
    explode.min_offset = 0x7FFF;
    explode.max_length = 0;
    explode.min_length = 0x7FFF;
    memset(explode.length_histogram, 0, sizeof(explode.length_histogram));

    // Read two header bytes.
    if ( fread( (uint8_t*) &header, sizeof (uint8_t), 2, in_fp ) != 2 ) {
	printf("Error: Unable to read header info.\n");
        return -1;
    }
    
    // Check literal mode value. Only 0 currently supported (1 is also defined)
    if (header.literal_mode > 0x1) {
	printf("Error: Literal mode %d not supported.\n", header.literal_mode);
        return -1;
    }

    // Check dictionary size value. Supports values of 4 through 6.
    // Dictionary size is 1 << (6 + val) (or 2^(6+val) ): 1024, 2048, or 4096
    if ((header.dictionary_size < 4) || (header.dictionary_size > 6)) {
        printf("Error: Bad dictionary size value (%d) in header.\n",
               header.dictionary_size);
        return -1;
    }
    
    // Read until EOF is detected.
    do
    {
        // Next bit indicates a literal or dictionary lookup.
	if (read_next_bit() == 0)
        {
            // -- Literal --
            unsigned char value;
            
            value = read_literal();
            write_byte( value );
            
	    // Stats update
	    explode.literal_count++;
	}
	else
	{
	    // -- Dictionary Look Up --

	    // Dictionary look up.  Find length and offset.
	    explode.length = read_copy_length();

	    // Length of 519 indicates end of file.
	    if (explode.length == 519)
	    {
		explode.end_marker = true;
	    }
	    else // otherwise,
	    {
		// Find offset.
		explode.offset = read_copy_offset();

		// Use copy length and offset to copy data from dictionary.
		write_dict_data();

		// Statistics update
		explode.dictionary_count++;
		explode.length_histogram[explode.length]++;

		if (explode.length > explode.max_length)
		    explode.max_length = explode.length;
		if (explode.length < explode.min_length)
		    explode.min_length = explode.length;
		if (explode.offset > explode.max_offset)
		    explode.max_offset = explode.offset;
		if (explode.offset < explode.min_offset)
		    explode.min_offset = explode.offset;

	    }
	}
    } while ( !explode.end_marker &&
	      !read_bitstream.error_flag && !write_buffer.error_flag );

    write_to_file();

    // If expected length was passed in, check it.
    if ((expected_length) &&
	(write_buffer.bytes_written != expected_length))
    {
	printf( "\nWarning: Number of bytes written (%ld) doesn't match expected value (%ld).\n",
                write_buffer.bytes_written, expected_length);
    }
    
    if (explode_stats != NULL)
    {
        explode_stats->dictionary_size = header.dictionary_size;
        explode_stats->literal_mode = header.literal_mode;
	explode_stats->dictionary_count = explode.dictionary_count;
	explode_stats->literal_count = explode.literal_count;
        explode_stats->max_length = explode.max_length;
	explode_stats->min_length = explode.min_length;
        explode_stats->max_offset = explode.max_offset;
        explode_stats->min_offset = explode.min_offset;
    }
    
    return write_buffer.bytes_written;
}




