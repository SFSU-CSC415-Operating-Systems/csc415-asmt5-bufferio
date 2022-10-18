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

// This structure is all the information needed to maintain an open file
// It contains a pointer to a fileInfo strucutre and any other information
// that you need to maintain your open file.
typedef struct b_fcb
	{
	fileInfo * fi;	//holds the low level systems file info

	// Add any other needed variables here to track the individual open file
	char * buffer;


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
		return fd;
		}
	
	fcbArray[fd].fi = GetFileInfo(filename);
	fcbArray[fd].buffer = malloc(B_CHUNK_SIZE);
	// printf("Filename: '%s'\nFile Size: %d\nLocation: %d\n", 
	// 	fcbArray[fd].fi->fileName, fcbArray[fd].fi->fileSize, fcbArray[fd].fi->location);
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

	int num_blocks = blocks_needed(count);
	int rem_blocks = num_blocks;
	int bytes_read = 0;
	int tot_bytes_read = 0;
	int rem_bytes = count;
	int loc = fcbArray[fd].fi->location;
	printf("Filename: %s\nLoc: %d\nNum_Bytes: %d\nNum_Blocks: %d\n", 
		fcbArray[fd].fi->fileName, loc, count, num_blocks);

	for (int i = 0; i < num_blocks; i++)
	{
		bytes_read = LBAread(fcbArray[fd].buffer, 1, loc);
		loc++;
		tot_bytes_read += bytes_read;
		// rem_blocks = blocks_needed(--count);
		// if (rem_bytes <= B_CHUNK_SIZE)
		// {
		// 	memcpy(buffer, fcbArray[fd].buffer, rem_bytes);
		// } 
		// else
		// {
		// 	memcpy(buffer, fcbArray[fd].buffer, bytes_read);
		// }
	}
	// bytes_requested modulo 512 gives final byte count.
	// printf("bytes: %d\n", tot_bytes_read);
	return 0;
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
	
int blocks_needed (int bytes) {
	return (bytes + B_CHUNK_SIZE - 1)/B_CHUNK_SIZE;
}

int bytes_needed (int blocks) {
	return (blocks + 7)/8;
}