/***************************************************************************
 * sdrformat.h
 *
 * Structures and flags using in (Win)SDR format files.
 *
 * Original credit:
 *  Header file for the RecordFile.c program by Karl Cunningham
 *  Created by Larry Cochrane Data: 7/01/04
 *
 * Modified by Chad Trabant:
 *  Strip to minimum needed and convert to C99 data types.
 * 
 * modified 2011.029
 ***************************************************************************/

#include <libmseed.h>

#define MSEC            1000
#define SEC_PER_DAY     86400
#define MSEC_PER_DAY    ((int32_t)SEC_PER_DAY * MSEC)

#define BLOCK_LEN       60      // Seconds
#define MAX_BLOCKS      1440    // Per day, although the last block can have 59 seconds in it.
#define MAX_SAMPLE_RATE 200     // For each channel
#define MAX_USER_LEN	MAX_SAMPLE_RATE * BLOCK_LEN    // Maximum samples per channel per block
#define MAX_CHANNELS	8	// Maximum number of channels
#define MAX_SAMPLES     MAX_SAMPLE_RATE * MAX_CHANNELS // Per second for all channels
#define MAX_SAMPLES_PER_BLOCK MAX_SAMPLES * BLOCK_LEN  // For all channels

#define MAX_FILE_INFO	2000   	// Number of FileInfo blocks
#define HDR_VERSION	0x01	// Version ID of the record file
#define GOOD_BLK_ID	0xa55a	// Each data block has this at the beginning of block

// The flags in the InfoBlock below
#define F_ISLOCKED		0x01	// Indicates that the A/D board was locked to a reference source
#define F_WASLOCKED		0x02	// Indicates that the A/D board was locked at some point but not

/*
 * Each data block has an array of flags to tell the unpacker if the sample is 
 * stored as a 8 bit character (+-127) or a short (+- 32K). This calculates 
 * the the size of the array. WinSDR saves this array right after the InfoBlock 
 * and before the compressed data. We add one at the end in case the division had a remainder.
 */
#define flagBlkSize(samPerSec) ((samPerSec * BLOCK_LEN) / 8 + 1)

#ifdef __cplusplus
extern "C" {
#endif

/* This structure is contained in the main header block. It is used
    to find the start location of data based on the start time of the block.
    WinSDR updates the header and the information in this block every minute.
    This is the interval that the program saves the data to disk. */
typedef struct {
  uint32_t startTime;		     // Start time (UTC) of the data block
  uint32_t filePosition;      	     // The start position in the record file
  int32_t blockSize; 		     // The size of the first block in bytes
  int32_t julian;		     // The julian day of the block
} FileInfo;

/* This is the main header block at the beginning of the file */
typedef struct {
  int32_t  fileVersionFlags;	     // Holds some unused flags and the file version number
  int32_t  sampleRate;		     // Samples per second for each channel.
  int32_t  numSamples;		     // Number of samples per second for all channels.
  int32_t  numChannels;		     // Number of active channels
  int32_t  numBlocks;		     // Number of data blocks (1 minute each)
  int32_t  lastBlockSize;	     // The size of the last data block in bytes
  uint32_t startTime;
  uint32_t lastTime;                 // Start and End times of the file
  uint32_t lastBlockOffset;	     // The offset to the last data block
  FileInfo fileInfo[MAX_FILE_INFO];  // Holds information for each block
} HeaderBlock;

/* Every minute WinSDR saves the data from the A/D baord to disk. This is the structure at the beginning
   of each data block */
typedef struct {
  uint16_t goodID;     		     // Set to GOOD_BLK_ID
  uint16_t flags;      		     // See F_ flags above
  uint8_t  alarmFlags[MAX_CHANNELS]; // If TRUE WinSDR was in the alarm state for the channel
  uint32_t startTime;		     // Start time (UTC) of the block. Resolution down to the second
  uint32_t startTimeTick;      	     // Time in milliseconds past midnight UTC for the start of this block
  uint32_t blockSize;		     // The data block size in bytes
} InfoBlock;

#ifdef __cplusplus
}
#endif
