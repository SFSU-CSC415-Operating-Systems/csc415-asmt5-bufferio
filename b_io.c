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

int get_next_LBA_block(b_io_fd fd);

int transfer_buffer(b_io_fd fd, char * buffer, int num_bytes);

// This structure is all the information needed to maintain an open file
// It contains a pointer to a fileInfo strucutre and any other information
// that you need to maintain your open file.
typedef struct b_fcb
	{
	fileInfo * fi;	//holds the low level systems file info

	// Add any other needed variables here to track the individual open file
	char * buffer; 	// buffer for open file
	int buffer_offset;	// current offset in buffer; -1 if reached EOF
	int bytes_in_buffer;

	int block_offset; 	// current offset from starting lba for file data; -1 if reached EOF
	int bytes_read;

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
	
	// EOF reached
	if (fcbArray[fd].block_offset == -1)
		{
		return 0;
		}

	// nothing in buffer, read into buffer and increment block_offset
	if (fcbArray[fd].bytes_in_buffer == 0)
		{
		get_next_LBA_block(fd);
		}

	// initialize buffer_remaining to whatever is in buffer minus the current position in buffer
	// or whatever is left in the file depending on whether we are currently in the EOF block.
	int buffer_remaining = (fcbArray[fd].fi->fileSize - fcbArray[fd].bytes_read <= B_CHUNK_SIZE) 
		? fcbArray[fd].fi->fileSize - fcbArray[fd].bytes_read 
		: B_CHUNK_SIZE - fcbArray[fd].buffer_offset;
	int bytes_copied = 0;

	// if the current block is the EOF, then set buffer_remaining to what is left in file.
	// if (fcbArray[fd].fi->fileSize - fcbArray[fd].bytes_read <= B_CHUNK_SIZE)
	// 	{
	// 	// printf("CUR BLOCK EOF:\nFilename: %s\n
	// 	// 	File Size: %d\n
	// 	//	Bytes Read: %d\n
	// 	//	Buffer Offset: %d\n",
	// 	// 	fcbArray[fd].fi->fileName, fcbArray[fd].fi->fileSize, 
	// 	//	fcbArray[fd].bytes_read, fcbArray[fd].buffer_offset);
	// 	buffer_remaining = fcbArray[fd].fi->fileSize - 
	// 		fcbArray[fd].bytes_read;
	// 	}
	
	// Guard clause: if the caller's request is less than the buffer remaining, 
	// just copy the request into caller's buffer.  Then return the bytes copied over.
	if (count <= buffer_remaining)
		{
		memcpy(buffer, fcbArray[fd].buffer + fcbArray[fd].buffer_offset, count);
		fcbArray[fd].buffer_offset += count;
		fcbArray[fd].bytes_read += count;
		return count;
		}

	// These cases are all for when the request is greater than what is remaining in buffer
	
	// Guard clause: If we are in the last block, simply copy the remaining buffer into 
	// caller's buffer because the caller's request is greater than what is left in the file.
	// Then return.
	if (fcbArray[fd].fi->fileSize - fcbArray[fd].bytes_read <= B_CHUNK_SIZE)
		{
		// printf("LAST BLOCK:\nFilename: %s\nFilesize: %d\nBytes_read: %d\n", 
		// 	fcbArray[fd].fi->fileName, 
		// 	fcbArray[fd].fi->fileSize, fcbArray[fd].bytes_read);
		memcpy(buffer, fcbArray[fd].buffer + fcbArray[fd].buffer_offset, buffer_remaining);
		fcbArray[fd].buffer_offset = fcbArray[fd].block_offset = -1;
		fcbArray[fd].bytes_read += buffer_remaining;
		return buffer_remaining;
		}

	// These are the cases if we are NOT in the last block

	// Copy whatever is remaining in the current block into caller's buffer and recalculate
	// the count find what else is left in the call.
	memcpy(buffer, fcbArray[fd].buffer + fcbArray[fd].buffer_offset, buffer_remaining);
	fcbArray[fd].bytes_read += buffer_remaining;
	bytes_copied += buffer_remaining;
	count -= buffer_remaining;

	// Blocks needed is dependent on whether the caller's request is greater than what is
	// left in the file.
	// First case (Guard clause): the call is greater than what is left; copy the rest of 
	// the file.
	int blks_needed = 0;
	if (count > fcbArray[fd].fi->fileSize - fcbArray[fd].bytes_read)
		{
		blks_needed = blocks_needed(fcbArray[fd].fi->fileSize - fcbArray[fd].bytes_read);
		fcbArray[fd].buffer_offset = 0;
		for (int i = 0; i < blks_needed - 1; i++)
			{
			get_next_LBA_block(fd);
			memcpy(buffer, fcbArray[fd].buffer, B_CHUNK_SIZE);
			fcbArray[fd].bytes_read += B_CHUNK_SIZE;
			bytes_copied += B_CHUNK_SIZE;
			count -= B_CHUNK_SIZE;
			}
		get_next_LBA_block(fd);
		memcpy(buffer, fcbArray[fd].buffer, count);
		fcbArray[fd].buffer_offset = fcbArray[fd].block_offset = -1;
		fcbArray[fd].bytes_read += count;
		bytes_copied += count;
		return bytes_copied;
		}

	// Last case: the call is less than or equal to what is left in file; copy everything 
	// the caller asks for.

	// Calculate blocks needed, copy full blocks to caller's buffer, then copy whatever is
	// remaining.
	blks_needed = blocks_needed(count);
	fcbArray[fd].buffer_offset = 0;
	for (int i = 0; i < blks_needed - 1; i++)
		{
		get_next_LBA_block(fd);
		memcpy(buffer, fcbArray[fd].buffer, B_CHUNK_SIZE);
		fcbArray[fd].bytes_read += B_CHUNK_SIZE;
		bytes_copied += B_CHUNK_SIZE;
		count -= B_CHUNK_SIZE;
		}
	get_next_LBA_block(fd);
	memcpy(buffer, fcbArray[fd].buffer, count);
	fcbArray[fd].bytes_read += count;
	fcbArray[fd].buffer_offset += count;
	bytes_copied += count;

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

int get_next_LBA_block(b_io_fd fd)
	{
	fcbArray[fd].bytes_in_buffer = LBAread(fcbArray[fd].buffer, 1, 
		fcbArray[fd].fi->location + fcbArray[fd].block_offset++);
	fcbArray[fd].buffer_offset = 0;
	return fcbArray[fd].bytes_in_buffer;
	}

int transfer_buffer(b_io_fd fd, char * buffer, int num_bytes)
	{
	memcpy(buffer, fcbArray[fd].buffer, num_bytes);
	fcbArray[fd].bytes_read += num_bytes;
	fcbArray[fd].buffer_offset += num_bytes;
	return num_bytes;
	}