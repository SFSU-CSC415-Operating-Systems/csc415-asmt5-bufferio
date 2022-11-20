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
* This is a buffered input output assignment.  This code initializes a file
* control block array and loads files into it.  Then it loads and reads
* the file into the file control block buffer.  Finally, it transfers
* the data from the fcb buffer into the caller's buffer.
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
	char * buf; 		// buffer for open file
	int bufOff;		// current offset/position in buffer
	int bufLen;		// number of bytes in the buffer
	int curBlock;	// current block number
	int blockLen;	// number of blocks in file

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

	fileInfo * fi = GetFileInfo(filename);
	if (fi == NULL)
		{
		return -2;
		}

	char * buf = malloc(B_CHUNK_SIZE);
	if (buf == NULL)
		{
		return -1;
		}

	b_io_fd fd = b_getFCB();
	if (fd == -1) 
		{
		perror("No free file control blocks available.");
		return fd;
		}
	
	// Allocate buffer for file
	
	// Initialize file descriptor
	fcbArray[fd].fi = fi;
	fcbArray[fd].buf = buf;
	fcbArray[fd].bufOff = 0;
	fcbArray[fd].bufLen = 0;
	fcbArray[fd].curBlock = 0;
	fcbArray[fd].blockLen = (fi->fileSize + B_CHUNK_SIZE - 1)/B_CHUNK_SIZE;

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
		
	if (startup == 0) b_init();  //Initialize our system

	// check fd is within the fcb size
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return -1;
		}
	
	// check to see if the fcb exists in this location
	if (fcbArray[fd].fi == NULL)
		{
		return -1;
		}

	// available bytes in buffer
	int availBytesInBuf = fcbArray[fd].bufLen - fcbArray[fd].bufOff;

	// number of bytes already delivered
	int bytesDelivered = (fcbArray[fd].curBlock * B_CHUNK_SIZE) - availBytesInBuf;

	// limit count to file length
	if ((count + bytesDelivered) > fcbArray[fd].fi->fileSize)
		{
		count = fcbArray[fd].fi->fileSize - bytesDelivered;

		if (count < 0)
			{
			printf("Error: Count: %d   Delivered: %d   CurBlock: %d", count,
							bytesDelivered, fcbArray[fd].curBlock);
			}
		}
	
	int first, mid, last, numBlocksToCopy, bytesRead;
	if (availBytesInBuf >= count)
		{
    // first is the first section of the data
    // it is the amount of bytes that will fill up the available bytes in buffer
    // if the amount of data is less than the remaining amount in the buffer, we
    // just copy the entire amount into the buffer.
		first = count;
		mid = 0;
    last = 0;
		}
  else
    {
    // the file is too big, so the first section is just what is left in buffer
    first = availBytesInBuf;

    // set the last section to all the bytes left in the file
    last = count - availBytesInBuf;

    // divide the last section by the chunk size to get the total number of
    // complete blocks to copy and multiply by the chunk size to get the bytes
    // that those blocks occupy
    numBlocksToCopy = last / B_CHUNK_SIZE;
    mid = numBlocksToCopy * B_CHUNK_SIZE;

    // finally subtract the complete bytes in mid from the total bytes left to
    // get the last bytes to put in buffer
    last = last - mid;
    }

  // memcopy first section
  if (first > 0)
    {
    memcpy(buffer, fcbArray[fd].buf + fcbArray[fd].bufOff, first);
    fcbArray[fd].bufOff += first;
    }

  // LBAread all the complete blocks into the buffer
  if (mid > 0)
    {
    bytesRead = LBAread(buffer + first, numBlocksToCopy, 
      fcbArray[fd].curBlock + fcbArray[fd].fi->location);
    fcbArray[fd].curBlock += numBlocksToCopy;
    mid = bytesRead;
    }

  // LBAread remaining block into the fcb buffer, and reset buffer offset
  if (last > 0)
    {
    bytesRead = LBAread(fcbArray[fd].buf, 1, 
      fcbArray[fd].curBlock + fcbArray[fd].fi->location);
    fcbArray[fd].curBlock += 1;
    fcbArray[fd].bufOff = 0;
    fcbArray[fd].bufLen = bytesRead;

    // if the bytesRead is less than what is left in the calculated amount,
    // reset last to the smaller amount
    if (bytesRead < last)
      {
      last = bytesRead;
      }
    
    // if the number of bytes is more than zero, copy the fd buffer to the
    // buffer and set the offset to the position after the data amount.
    if (last > 0)
      {
      memcpy(buffer + first + mid, fcbArray[fd].buf + fcbArray[fd].bufOff, 
        fcbArray[fd].curBlock + fcbArray[fd].fi->location);
      fcbArray[fd].bufOff += last;
      }
    }

    return first + mid + last;
	}
	
// b_close frees and allocated memory and places the file control block back 
// into the unused pool of file control blocks.
int b_close (b_io_fd fd)
	{
		free(fcbArray[fd].buf);
		fcbArray[fd].buf = NULL;
		fcbArray[fd].fi = NULL;
		return 0;
	}
