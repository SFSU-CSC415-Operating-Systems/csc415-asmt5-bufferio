/**************************************************************
* Class:  CSC-415-02 Fall 2022
* Name: Mark Kim
* Student ID: 918204214
* GitHub UserID: mkim797
* Project: Assignment 5 – Buffered I/O
*
* File: b_io.c
*
* Description:
* 
*
**************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "b_io.h"
#include "fsLowSmall.h"

#define MAXFCBS 20	//The maximum number of files open at one time

int blocks_needed (int bytes);

int next_LBA_block(b_io_fd fd);

// This structure is all the information needed to maintain an open file
// It contains a pointer to a fileInfo strucutre and any other information
// that you need to maintain your open file.
typedef struct b_fcb
	{
	fileInfo * fi;	//holds the low level systems file info

	// Add any other needed variables here to track the individual open file
	char * buffer; 		// buffer for open file
	int buffer_offset;	// current offset in buffer; -1 if reached EOF
	int bytes_in_buffer;// number of bytes loaded into buffer

	int block_offset; 	// current offset from starting lba for file data; -1 if reached EOF
	int bytes_read;		// to track how many bytes have been read

	} b_fcb;
	
//static array of file control blocks
b_fcb fcbArray[MAXFCBS];

// Indicates that the file control block array has not been initialized
int startup = 0;	

// Method to initialize our file system / file control blocks
// Anything else that needs one time initialization can go in this routine
void b_init ()
	{
	if (startup)
		return;			//already initialized

	//init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
		{
		fcbArray[i].fi = NULL; //indicates a free fcbArray
		}
		
	startup = 1;
	}

//Method to get a free File Control Block FCB element
b_io_fd b_getFCB ()
	{
	for (int i = 0; i < MAXFCBS; i++)
		{
		if (fcbArray[i].fi == NULL)
			{
			fcbArray[i].fi = (fileInfo *)-2; // used but not assigned
			return i;		//Not thread safe but okay for this project
			}
		}

	return (-1);  //all in use
	}

// b_open is called by the "user application" to open a file.  This routine is 
// similar to the Linux open function.  	
// You will create your own file descriptor which is just an integer index into an
// array of file control blocks (fcbArray) that you maintain for each open file.  

b_io_fd b_open (char * filename, int flags)
	{
	if (startup == 0) b_init();  //Initialize our system

	//*** TODO ***:  Write open function to return your file descriptor
	//				 You may want to allocate the buffer here as well
	//				 But make sure every file has its own buffer
	
	// This is where you are going to want to call GetFileInfo and b_getFCB
	b_io_fd fd = b_getFCB();

	if (fd == -1) 
		{
		perror("No free file control blocks available.");
		return fd;
		}
	
	fcbArray[fd].buffer = malloc(B_CHUNK_SIZE);

	if (fcbArray[fd].buffer == NULL)
		{
		perror("Could not allocate buffer");
		return EXIT_FAILURE;
		}
	
	fcbArray[fd].fi = GetFileInfo(filename);
	fcbArray[fd].buffer_offset = 0;
	fcbArray[fd].block_offset = 0;
	fcbArray[fd].bytes_in_buffer = 0;
	fcbArray[fd].bytes_read = 0;

	return fd;
	}

// b_read functions just like its Linux counterpart read.  The user passes in
// the file descriptor (index into fcbArray), a buffer where thay want you to 
// place the data, and a count of how many bytes they want from the file.
// The return value is the number of bytes you have copied into their buffer.
// The return value can never be greater then the requested count, but it can
// be less only when you have run out of bytes to read.  i.e. End of File	
int b_read (b_io_fd fd, char * buffer, int count)
	{
	//*** TODO ***:  
	// Write buffered read function to return the data and # bytes read
	// You must use LBAread and you must buffer the data in B_CHUNK_SIZE byte chunks.
		
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}

	// and check that the specified FCB is actually in use	
	if (fcbArray[fd].fi == NULL)		//File not open for this descriptor
		{
		return -1;
		}
	
	if (fcbArray[fd].bytes_read == fcbArray[fd].fi->fileSize)
		{
		fcbArray[fd].bytes_read = 0;
		return 0;
		}

	int buffer_remaining = B_CHUNK_SIZE - fcbArray[fd].buffer_offset;
	int bytes_copied = 0;

	if (fcbArray[fd].bytes_in_buffer == 0)
		{
		// printf("File: %s\nLoc: %d\nBlock Offset: %d\n", fcbArray[fd].fi->fileName, fcbArray[fd].fi->location, fcbArray[fd].block_offset);
		fcbArray[fd].bytes_in_buffer = LBAread(fcbArray[fd].buffer, 1, 
			fcbArray[fd].fi->location + fcbArray[fd].block_offset++);
		fcbArray[fd].buffer_offset = 0;
		if (fcbArray[fd].bytes_in_buffer < count)
			{
			memcpy(buffer, fcbArray[fd].buffer, fcbArray[fd].bytes_in_buffer);
			bytes_copied = fcbArray[fd].bytes_in_buffer;
			fcbArray[fd].bytes_read += bytes_copied;
			fcbArray[fd].buffer_offset = fcbArray[fd].block_offset = 0;
			return bytes_copied;
			}
		}

	if (count < buffer_remaining)
		{
		memcpy(buffer, fcbArray[fd].buffer + fcbArray[fd].buffer_offset, count);
		fcbArray[fd].buffer_offset += count;
		bytes_copied = count;
		}

	else
		{
		// calculate the number of blocks needed after depleting the remaining buffer
		int num_blocks = blocks_needed(count - buffer_remaining);

		// copy the portion of the count that will deplete the fcb buffer
		memcpy(buffer, fcbArray[fd].buffer + fcbArray[fd].buffer_offset, buffer_remaining);

		// track the number of bytes copied from fcb buffer to provided buffer
		bytes_copied = buffer_remaining;

		// read next block and increment the block to the next block
		fcbArray[fd].bytes_in_buffer = LBAread(fcbArray[fd].buffer, 1, 
			fcbArray[fd].fi->location + fcbArray[fd].block_offset++);
		fcbArray[fd].buffer_offset = 0;

		if (fcbArray[fd].bytes_in_buffer < B_CHUNK_SIZE)
			{
			memcpy(buffer, fcbArray[fd].buffer, fcbArray[fd].bytes_in_buffer);
			bytes_copied += fcbArray[fd].bytes_in_buffer;
			fcbArray[fd].buffer_offset = fcbArray[fd].block_offset = 0;
			fcbArray[fd].bytes_read += bytes_copied;
			return bytes_copied;
			}
			

		// calculate the final number of bytes to copy over after supplying complete blocks
		int last_bytes = (count - buffer_remaining) % B_CHUNK_SIZE;

		// copy full blocks into the buffer
		for (int i = 0; i < num_blocks - 1; i++)
			{
			memcpy(buffer, fcbArray[fd].buffer, B_CHUNK_SIZE);
			bytes_copied += B_CHUNK_SIZE;
			fcbArray[fd].bytes_in_buffer = LBAread(fcbArray[fd].buffer, 1, 
				fcbArray[fd].fi->location + fcbArray[fd].block_offset++);
	
			if (fcbArray[fd].bytes_in_buffer < B_CHUNK_SIZE)
				{
				memcpy(buffer, fcbArray[fd].buffer, fcbArray[fd].bytes_in_buffer);
				bytes_copied += fcbArray[fd].bytes_in_buffer;
				fcbArray[fd].buffer_offset = fcbArray[fd].block_offset = -1;
				fcbArray[fd].bytes_read += bytes_copied;
				return bytes_copied;
				}
			}

		// copy remaining bytes into buffer
		memcpy(buffer, fcbArray[fd].buffer, last_bytes);
		bytes_copied += last_bytes;
		fcbArray[fd].buffer_offset = last_bytes;
		}
	
	fcbArray[fd].bytes_read += bytes_copied;
	return bytes_copied;
	// Your Read code here - the only function you call to get data is LBAread.
	// Track which byte in the buffer you are at, and which block in the file	
	}
	
// b_close frees and allocated memory and places the file control block back 
// into the unused pool of file control blocks.
int b_close (b_io_fd fd)
	{
		// for (int i = 0; i < )
		return 0;
	//*** TODO ***:  Release any resources
	}
	
int blocks_needed (int bytes) 
	{
	return (bytes + B_CHUNK_SIZE - 1)/B_CHUNK_SIZE;
	}

int bytes_needed (int blocks) 
	{
	return (blocks + 7)/8;
	}

int next_LBA_block(b_io_fd fd)
	{
	fcbArray[fd].bytes_in_buffer = LBAread(fcbArray[fd].buffer, 1, 
		fcbArray[fd].fi->location + fcbArray[fd].block_offset++);
	fcbArray[fd].buffer_offset = 0;
	return fcbArray[fd].bytes_in_buffer;
	}