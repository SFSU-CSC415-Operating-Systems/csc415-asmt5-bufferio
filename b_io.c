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

int calc_blocks_needed (int bytes);

int get_next_LBA_block(b_io_fd fd);

int transfer_buffer(b_io_fd fd, char * buffer, int num_bytes, int offset);

// This structure is all the information needed to maintain an open file
// It contains a pointer to a fileInfo strucutre and any other information
// that you need to maintain your open file.
typedef struct b_fcb
	{
	fileInfo * fi;	//holds the low level systems file info

	// Add any other needed variables here to track the individual open file
	char * buffer; 		// buffer for open file
	int buffer_offset;	// current offset in buffer
	
	// number of bytes in the buffer; normally B_CHUNK_SIZE unless at EOF 
	// (bytes left in file).
	int bytes_in_buffer;

	// current offset from starting lba for file data; -1 if reached EOF
	int tot_full_blocks;
	int block_offset;
	int bytes_read;		// current bytes read from file

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
	
	// Get next free file control block and file descriptor
	b_io_fd fd = b_getFCB();

	if (fd == -1) 
		{
		perror("No free file control blocks available.");
		return fd;
		}
	
	// Allocate buffer for file
	fcbArray[fd].buffer = malloc(B_CHUNK_SIZE);

	if (fcbArray[fd].buffer == NULL)
		{
		perror("Could not allocate buffer");
		return EXIT_FAILURE;
		}
	
	// Initialize file descriptor
	fcbArray[fd].fi = GetFileInfo(filename);
	fcbArray[fd].tot_full_blocks = (int)(fcbArray[fd].fi->fileSize/B_CHUNK_SIZE);
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
	
	// Your Read code here - the only function you call to get data is LBAread.
	// Track which byte in the buffer you are at, and which block in the file	
	
	// Check if EOF has been read
	if (fcbArray[fd].block_offset == -1)
		{
		return 0;
		}

	if (count > fcbArray[fd].fi->fileSize - fcbArray[fd].bytes_read)
		{
		count = fcbArray[fd].fi->fileSize - fcbArray[fd].bytes_read;
		}
	
	// Nothing in buffer, get next block
	if (fcbArray[fd].bytes_in_buffer == 0)
		{
		get_next_LBA_block(fd);
		}

	int bytes_copied = 0;
	int bytes_remaining = 0;

	if (fcbArray[fd].block_offset < fcbArray[fd].tot_full_blocks)
		{
		bytes_remaining = B_CHUNK_SIZE - fcbArray[fd].buffer_offset;
		}
	else
		{
		bytes_remaining = fcbArray[fd].fi->fileSize - fcbArray[fd].bytes_read;
		}

	if (count <= bytes_remaining)
		{
		if (count == bytes_remaining && fcbArray[fd].block_offset >= fcbArray[fd].tot_full_blocks)
			fcbArray[fd].block_offset = -1;
		return transfer_buffer(fd, buffer, count, fcbArray[fd].buffer_offset);
		}

	transfer_buffer(fd, buffer, bytes_remaining, fcbArray[fd].buffer_offset);
	bytes_copied += bytes_remaining;
	count -= bytes_remaining;

	int blocks_needed = calc_blocks_needed(count);
	for (int i = 0; i < blocks_needed; i++)
		{
		get_next_LBA_block(fd);
		transfer_buffer(fd, buffer, B_CHUNK_SIZE, 0);
		bytes_copied += B_CHUNK_SIZE;
		count -= B_CHUNK_SIZE;
		}
	
	if (count != 0)
		{
		get_next_LBA_block(fd);
		transfer_buffer(fd, buffer, count, 0);
		bytes_copied += count;
		count = 0;
		}

	return bytes_copied;
	
// b_close frees and allocated memory and places the file control block back 
// into the unused pool of file control blocks.
int b_close (b_io_fd fd)
	{
		free(fcbArray[fd].buffer);
		fcbArray[fd].buffer = NULL;
		fcbArray[fd].fi = NULL;
		return 0;
	//*** TODO ***:  Release any resources
	}
	
// calc_blocks_needed calculates the number of blocks needed for the bytes
// requested
int calc_blocks_needed (int bytes) 
	{
	return bytes/B_CHUNK_SIZE;
	}

// get_next_LBA_block reads in the next block into the b_fcb buffer and
// increments the block_offset and resets the buffer_offset to 0, then returns
// the number of bytes in the b_fcb buffer.
// This function should probably be where the EOF should be calculated.
int get_next_LBA_block(b_io_fd fd)
	{
	fcbArray[fd].bytes_in_buffer = LBAread(fcbArray[fd].buffer, 1, 
		fcbArray[fd].fi->location + fcbArray[fd].block_offset++);
	fcbArray[fd].buffer_offset = 0;
	return fcbArray[fd].bytes_in_buffer;
	}

// transfer_buffer copies the bytes requested into the caller's buffer and
// updates all the file and buffer tracking in fhe b_fcb.
int transfer_buffer(b_io_fd fd, char * buffer, int num_bytes, int offset)
	{
	memcpy(buffer, fcbArray[fd].buffer + offset, num_bytes);
	fcbArray[fd].bytes_read += num_bytes;
	fcbArray[fd].buffer_offset += num_bytes;
	return num_bytes;
	}