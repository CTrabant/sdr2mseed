/***************************************************************************
 * sdr2mseed.c
 *
 * Waveform data conversion from (Win)SDR timeseries to Mini-SEED.
 *
 * Written by Chad Trabant, IRIS Data Management Center
 *
 * modified 2011.146
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

#include <libmseed.h>

#include "sdrformat.h"
#include "decimate.h"

#define VERSION "0.2"
#define PACKAGE "sdr2mseed"

struct listnode {
  char *key;
  char *data;
  struct listnode *next;
};

static int parseSDR (char *sdrfile, MSTraceGroup *mstg);
static int sdr2group (FILE *ifp, MSTraceGroup *mstg, int format, char *sdrfile, int verbose);
static int decompressSDR (HeaderBlock *sh, InfoBlock *iblock, int blocknum, int16_t *msdata);
static int decimate (MSTrace *mst, int factor);
static void packtraces (MSTraceGroup *mstg, flag flush);
static void record_handler (char *record, int reclen, void *handlerdata);
static int parameter_proc (int argcount, char **argvec);
static char *getoptval (int argcount, char **argvec, int argopt);
static int readlistfile (char *listfile);
static void addnode (struct listnode **listroot, char *key, char *data);
static void usage (void);

static int   verbose     = 0;
static int   packreclen  = -1;
static int   encoding    = 11;
static int   byteorder   = -1;
static int   sdrformat   = 0;
static char  srateblkt   = 0;
static char *network     = "XX";
static char *station     = "SDR";
static char *location    = "  ";
static char *channel     = 0;
static char *outputfile  = 0;
static FILE *ofp         = 0;

/* Maximum number of decimation operations */
#define MAX_DECIMATION 8

static int   chanlist[MAX_CHANNELS];
static int   decilist[MAX_DECIMATION];

/* A list of input files */
struct listnode *filelist = 0;

static int packedtraces  = 0;
static int64_t packedsamples = 0;
static int packedrecords = 0;

int
main (int argc, char **argv)
{
  MSTraceGroup *mstg = 0;
  struct listnode *flp;
  
  /* Process given parameters (command line and parameter file) */
  if (parameter_proc (argc, argv) < 0)
    return -1;
  
  /* Init MSTraceGroup */
  mstg = mst_initgroup (mstg);
  
  /* Open the output file if specified */
  if ( outputfile )
    {
      if ( strcmp (outputfile, "-") == 0 )
        {
          ofp = stdout;
        }
      else if ( (ofp = fopen (outputfile, "wb")) == NULL )
        {
          fprintf (stderr, "Cannot open output file: %s (%s)\n",
                   outputfile, strerror(errno));
          return -1;
        }
    }
  
  /* Read input files into MSTraceGroup */
  flp = filelist;
  while ( flp != 0 )
    {
      if ( verbose )
	fprintf (stderr, "Reading %s\n", flp->data);

      parseSDR (flp->data, mstg);
      
      flp = flp->next;
    }
  
  fprintf (stderr, "Packed %d trace(s) of %lld samples into %d records\n",
	   packedtraces, (long long int)packedsamples, packedrecords);
  
  /* Make sure everything is cleaned up */
  if ( ofp )
    fclose (ofp);
  
  return 0;
}  /* End of main() */


/***************************************************************************
 * parseSDR:
 *
 * Read an SDR file and add data samples to a MSTraceGroup.  When
 * finshed reading the file write output miniSEED.
 *
 * Returns 0 on success, and -1 on failure
 ***************************************************************************/
static int
parseSDR (char *sdrfile, MSTraceGroup *mstg)
{
  FILE *ifp = 0;
  int datacnt;
  int idx;
  
  /* Open input file */
  if ( (ifp = fopen (sdrfile, "rb")) == NULL )
    {
      fprintf (stderr, "Cannot open input file: %s (%s)\n",
	       sdrfile, strerror(errno));
      return -1;
    }
  
  /* Parse input SDR file and add data to MSTraceGroup */
  if ( (datacnt = sdr2group (ifp, mstg, sdrformat, sdrfile, verbose)) < 0 )
    {
      fprintf (stderr, "Error parsing %s\n", sdrfile);
      
      return -1;
    }
  
  /* Perform decimation steps requested */
  for (idx = 0; idx < MAX_DECIMATION && decilist[idx]; idx++)
    {
      MSTrace *mst;
      
      if ( ! mstg )
	continue;
      
      mst = mstg->traces;
      while ( mst )
	{
	  if ( decimate (mst, decilist[idx]) )
	    break;
	  
	  mst = mst->next;
	}
    }
  
  /* Open output file if needed */
  if ( ! ofp )
    {
      char mseedoutputfile[1024];
      
      strncpy (mseedoutputfile, sdrfile, sizeof(mseedoutputfile)-6 );
      
      /* Add .mseed to the file name */
      strcat (mseedoutputfile, ".mseed");
      
      if ( (ofp = fopen (mseedoutputfile, "wb")) == NULL )
        {
          fprintf (stderr, "Cannot open output file: %s (%s)\n",
                   mseedoutputfile, strerror(errno));
          return -1;
        }
    }
  
  packtraces (mstg, 1);
  packedtraces += mstg->numtraces;
  
  /* Cleanup */
  fclose (ifp);
  
  if ( ofp && ! outputfile )
    {
      fclose (ofp);
      ofp = 0;
    }
  
  return 0;
}  /* End of parseSDR() */


/***************************************************************************
 * sdr2group:
 *
 * Parse a SDR "seisdata" file, populate an MSRecord structure as a
 * temporary holder and add each channel block to a MSTraceGroup.
 *
 * The format argument is interpreted as:
 * 0 : SDR format
 *
 * Returns number of data samples in file or -1 on failure.
 ***************************************************************************/
static int
sdr2group (FILE *ifp, MSTraceGroup *mstg, int format, char *sdrfile, int verbose)
{
  MSRecord *msr = 0;
  MSTrace *mst;
  struct blkt_1000_s Blkt1000;
  struct blkt_1001_s Blkt1001;
  struct blkt_100_s Blkt100;
  
  hptime_t daytime;
  BTime btime;
  
  HeaderBlock hblock;
  FileInfo *finfo;
  InfoBlock *iblock;
  char stime[50];
  char ltime[50];

  char *datablock = NULL;
  int datablocklength = 0;
  
  int16_t *msdata = NULL;
  int mssamples;
  int32_t *cdata = NULL;
  
  char chanstr[4];
  int totalsamples = 0;
  int idx;
  int cidx;
  int sidx;
  int midx;
  
  /* Argument sanity */
  if ( ! ifp || ! mstg )
    return -1;
  
  if ( ! (msr = msr_init(msr)) )
    {
      fprintf (stderr, "Cannot initialize MSRecord strcture\n");
      return -1;
    }
  
  /* Read the header block */
  if ( fread (&hblock, sizeof(HeaderBlock), 1, ifp) < 1 )
    return -1;
  
  /* Sanity check header version and sample count */
  if ( (hblock.fileVersionFlags & 0xFF) != HDR_VERSION)
    {
      fprintf (stderr, "%s: Unrecognized file type (invalid header version), skipping\n",
	       sdrfile);
      return -1;
    }
  if ( (hblock.numSamples != (hblock.sampleRate * hblock.numChannels)) )
    {
      fprintf (stderr, "%s: Unrecognized file type (sample count inconsistent), skipping\n",
	       sdrfile);
      return -1;
    }
  
  /* Report header details */
  if ( verbose )
    {
      ms_hptime2mdtimestr (MS_EPOCH2HPTIME(hblock.startTime), stime, 0);
      ms_hptime2mdtimestr (MS_EPOCH2HPTIME(hblock.lastTime), ltime, 0);
      
      if ( verbose < 2 )
	{
	  fprintf (stderr, "%s: version %d, samps/sec: %d, %s - %s\n",
		   sdrfile, (hblock.fileVersionFlags & 0xFF), hblock.sampleRate,
		   stime, ltime);
	}
      else
	{
	  fprintf (stderr, "  fileVersionFlags: %d\n", hblock.fileVersionFlags);
	  fprintf (stderr, "  sampleRate:      %d (samples/second)\n", hblock.sampleRate);
	  fprintf (stderr, "  numSamples:      %d\n", hblock.numSamples);
	  fprintf (stderr, "  numChannels:     %d\n", hblock.numChannels);
	  fprintf (stderr, "  numBlocks:       %d\n", hblock.numBlocks);
	  fprintf (stderr, "  lastBlockSize:   %d\n", hblock.lastBlockSize);
	  fprintf (stderr, "  startTime:       %d (%s)\n", hblock.startTime, stime);
	  fprintf (stderr, "  lastTime:        %d (%s)\n", hblock.lastTime, ltime);
	  fprintf (stderr, "  lastBlockOffset: %d\n", hblock.lastBlockOffset);
	}
    }
  
  /* Allocate 1-minute multiplexed "blocked" data buffer */
  if ( ! (msdata = (int16_t *) malloc (60 * hblock.numSamples * sizeof(int16_t))) )
    {
      fprintf (stderr, "%s: Error allocating muxed data buffer of %d bytes\n",
	       sdrfile, (int) (60 * hblock.numSamples * sizeof(int16_t)));
      return -1;
    }
  
  /* Allocate 1-minute demultiplexed "unblocked" channel buffer */
  if ( ! (cdata = (int32_t *) malloc (60 * hblock.sampleRate * sizeof(int32_t))) )
    {
      fprintf (stderr, "%s: Error allocating channel data buffer of %d bytes\n",
	       sdrfile, (int) (60 * hblock.numSamples * sizeof(int32_t)));
      return -1;
    }
  
  /* Populate MSRecord structure with header details */
  ms_strncpclean (msr->network, network, 2);
  ms_strncpclean (msr->station, station, 5);
  ms_strncpclean (msr->location, location, 2);
  
  msr->samprate = hblock.sampleRate;
  msr->sampletype = 'i';
  
  /* Loop through file info blocks */
  for (idx = 0; idx < hblock.numBlocks; idx++)
    {
      finfo = &(hblock.fileInfo[idx]);
      
      if ( ! finfo->startTime && ! finfo->filePosition && ! finfo->blockSize && ! finfo->julian )
	continue;
      
      /* Report details */
      if ( verbose > 1 )
	{
	  ms_hptime2mdtimestr (MS_EPOCH2HPTIME(finfo->startTime), stime, 0);
	  
	  if ( verbose == 2 )
	    {
	      fprintf (stderr, "  Data block (%d): %s, offset: %d, size: %d, julian: %d\n",
		       idx+1, stime, finfo->filePosition, finfo->blockSize, finfo->julian);
	    }
	  else
	    {
	      fprintf (stderr, "  Data block (%d):\n", idx+1);
	      fprintf (stderr, "    startTime:     %d (%s)\n", finfo->startTime, stime);
	      fprintf (stderr, "    filePosition:  %d\n", finfo->filePosition);
	      fprintf (stderr, "    blockSize:     %d\n", finfo->blockSize);
	      fprintf (stderr, "    julian:        %d\n", finfo->julian);
	    }
	}
      
      /* Seek to data block position in file */
      if ( fseek (ifp, finfo->filePosition, SEEK_SET) )
	{
	  fprintf (stderr, "%s: Error seeking to offset %d\n",
		   sdrfile, finfo->filePosition);
	  break;
	}
      
      /* (Re)allocate data buffer if needed */
      if ( ! datablock || datablocklength < finfo->blockSize )
	{
	  if ( ! (datablock = (char *)realloc (datablock, finfo->blockSize)) )
	    {
	      fprintf (stderr, "%s: Error (re)allocating data buffer of %d bytes\n",
		       sdrfile, finfo->blockSize);
	      break;
	    }
	  
	  datablocklength = finfo->blockSize;
	}
      
      /* Read data block from file */
      if ( fread (datablock, finfo->blockSize, 1, ifp) < 1 )
	{
	  fprintf (stderr, "%s: Error reading data block, %d bytes from offset %d\n",
		   sdrfile, finfo->blockSize, finfo->filePosition);
	  break;
	}
      
      iblock = (InfoBlock *) datablock;
      
      /* Sanity check ID */
      if ( iblock->goodID != GOOD_BLK_ID )
	{
	  fprintf (stderr, "%s: Error reading data block, good ID not found at offset %d\n",
		   sdrfile, finfo->filePosition);
	  continue;
	}
      
      /* Report details */
      if ( verbose > 1 )
	{
	  ms_hptime2mdtimestr (MS_EPOCH2HPTIME(iblock->startTime), stime, 0);
	  
	  fprintf (stderr, "    Info block: %s, mstick: %d, size: %d\n",
		   stime, iblock->startTimeTick, iblock->blockSize);
	}
      
      /* Decompress data block */
      mssamples = decompressSDR (&hblock, iblock, idx+1, msdata);
      
      totalsamples += mssamples;
      
      /* Determine time at day boundary */
      ms_hptime2btime (MS_EPOCH2HPTIME(iblock->startTime), &btime);
      btime.hour = btime.min = btime.sec = btime.fract = 0;
      daytime = ms_btime2hptime (&btime);
      
      /* Determine start time, day boundary plus tick in milliseconds */
      msr->starttime = daytime + ((hptime_t)iblock->startTimeTick * (HPTMODULUS / 1000));
      
      /* Demultiplex data samples into channels and add to group */
      for (cidx = 0; cidx < hblock.numChannels; cidx++ )
	{
	  /* Skip channel if not requested */
	  if ( chanlist[cidx] == 0 )
	    continue;
	  
	  /* Extract channel samples from multiplexed array */
	  for (sidx = 0, midx = cidx; midx < mssamples; sidx++, midx += hblock.numChannels )
	    {
	      cdata[sidx] = msdata[midx];
	    }
	  
	  /* Set channel codes */
	  if ( channel )
	    ms_strncpclean (msr->channel, channel, 3);
	  else
	    {
	      snprintf (chanstr, sizeof(chanstr), "%03d", cidx+1);
	      ms_strncpclean (msr->channel, chanstr, 3);
	    }
	  
	  /* Set data array and sample count */
	  msr->datasamples = cdata;
	  msr->samplecnt = msr->numsamples = sidx;
	  
	  if ( verbose > 2 )
	    {
	      fprintf (stderr, "[%s] %lld samps @ %.6f Hz for N: '%s', S: '%s', L: '%s', C: '%s'\n",
		       sdrfile, (long long int)msr->numsamples, msr->samprate,
		       msr->network, msr->station, msr->location, msr->channel);
	    }
	  
	  /* Add data to Group */
	  if ( ! (mst = mst_addmsrtogroup (mstg, msr, 0, -1.0, -1.0)) )
	    {
	      fprintf (stderr, "[%s] Error adding samples to MSTraceGroup\n", sdrfile);
	    }
	  
	  /* Create an MSRecord template for the MSTrace by copying the current holder */
	  if ( ! mst->prvtptr )
	    {
	      mst->prvtptr = msr_duplicate (msr, 0);
	      
	      if ( ! mst->prvtptr )
		{
		  fprintf (stderr, "[%s] Error duplicate MSRecord for template\n", sdrfile);
		  return -1;
		}
	      
	      /* Add blockettes 1000 & 1001 to template */
	      memset (&Blkt1000, 0, sizeof(struct blkt_1000_s));
	      msr_addblockette ((MSRecord *) mst->prvtptr, (char *) &Blkt1000,
				sizeof(struct blkt_1001_s), 1000, 0);
	      memset (&Blkt1001, 0, sizeof(struct blkt_1001_s));
	      msr_addblockette ((MSRecord *) mst->prvtptr, (char *) &Blkt1001,
				sizeof(struct blkt_1001_s), 1001, 0);
	      
	      /* Add blockette 100 to template if requested */
	      if ( srateblkt )
		{
		  memset (&Blkt100, 0, sizeof(struct blkt_100_s));
		  Blkt100.samprate = (float) msr->samprate;
		  msr_addblockette ((MSRecord *) mst->prvtptr, (char *) &Blkt100,
				    sizeof(struct blkt_100_s), 100, 0);
		}
	    }
	}
    } /* Done looping through file info blocks */
  
  if ( datablock )
    free (datablock);
  
  if ( msdata )
    free (msdata);
  
  if ( cdata )
    free (cdata);
  
  if ( msr )
    {
      msr->datasamples = 0;
      msr_free (&msr);
    }
  
  return totalsamples;
}  /* End of sdr2group() */


/***************************************************************************
 * decompressSDR:
 *
 * Decompress SDR data block and place in supplied integer array.
 *
 * The InfoBlock buffer contains the entire data block starting with
 * the InfoBlock, then the flag block and finally the compressed data.
 * Each reading is either 8- or 16-bit.  The flagBlk array contains a
 * flag for each data point telling whether it is 8- or 16-bit data.
 *
 * Routine from sdrmanip source code (windsdr.c) by Karl Cunningham.
 * Originally (probably) by Larry Cochrane.
 *
 * Returns number of samples decompressed on success and -1 on error.
 ***************************************************************************/
int decompressSDR (HeaderBlock *hblock, InfoBlock *iblock, int blocknum,
		   int16_t *msdata)
{
  int bitCount = 0;
  int samplesDecoded = 0;
  int byteCnt;
  int8_t *inPtr;
  int16_t *outPtr;
  int8_t *flagBlk;
  int8_t tmpFlag;
  double ratio;
  int tooShort = 0;
  int numShort = 0;
  int numChar = 0;
  
  if ( ! hblock || ! iblock || ! msdata )
    return -1;
  
  /* Decompress one minute's worth of data. Compute the data block
   * size in bytes, and stop when we get to the end of the valid  data. */
  
  byteCnt = iblock->blockSize - flagBlkSize(hblock->numSamples) - sizeof(InfoBlock);
  
  if ( verbose > 1 )
    fprintf (stderr, "  Decompressing block size: %d, flag block size: %d, bytes: %d\n",
	     iblock->blockSize, flagBlkSize(hblock->numSamples), byteCnt);
  
  outPtr = msdata;
  flagBlk = (int8_t *)iblock + sizeof(InfoBlock);
  tmpFlag = *flagBlk;
  inPtr = (int8_t *)flagBlk + flagBlkSize(hblock->numSamples);
  
  while ( byteCnt > 0 )
    {
      if ( tmpFlag & 1 ) // 16-bit integer
	{
	  *outPtr++ = *((int16_t *)inPtr);
	  
	  if ( (verbose > 1) && (*(outPtr-1)) && (*((outPtr-1)) < 128) && (*((outPtr-1)) > -127) )
	    {
	      tooShort++;
	    }
	  
	  inPtr += 2;
	  byteCnt -= 2;
	  numShort++;
	}
      else               // 8-bit integer
	{
	  *outPtr++ = *((int8_t *)inPtr);
	  
	  inPtr++;
	  byteCnt--;
	  numChar++;
	}
      
      samplesDecoded++;
      
      if ( ++bitCount >= 8 )
	{                // next bit
	  bitCount = 0;
	  tmpFlag = *(++flagBlk);
	}
      else
	{
	  tmpFlag >>= 1;
	}
    }
  
  if ( byteCnt < 0 )
    {
      fprintf (stderr, "WARNING -- Possible Data Corruption.\n");
      fprintf (stderr, "  Flags did not match the number of data bytes when decompressing data in Block No %d\n",
	       blocknum);
    }
  
  if ( verbose > 1 )
    {
      ratio = (double)samplesDecoded * 2.0 / (iblock->blockSize - flagBlkSize(hblock->numSamples) - sizeof(InfoBlock));
      
      fprintf (stderr, "  Decompressed %d samples (for %d channels)  Compression: %.2f:1  numShort: %d  numChar: %d Too Short: %d\n",
	       samplesDecoded, hblock->numChannels, ratio, numShort, numChar, tooShort);
    }
  
  if ( samplesDecoded > (hblock->numChannels * hblock->sampleRate * BLOCK_LEN) )
    {
      fprintf (stderr, "WARNING -- Possible Data Corruption.\n");
      fprintf (stderr, "  More than one minute's samples for %d channels * %d sps rate * %d seconds per block\n",
	       hblock->numChannels, hblock->sampleRate, BLOCK_LEN);
    }
  
  return samplesDecoded;
}  /* End of decompressSDR() */


/***************************************************************************
 * decimate:
 *
 * Decimates the time-series by a given factor.  Integer and float
 * samples will be converted to doubles.
 *
 * Returns 0 on success and non-zero on error.
 ***************************************************************************/
static int
decimate (MSTrace *mst, int factor)
{
  int numsamples;
  
  if ( ! mst )
    return -1;
  
  /* Check for integer data */
  if ( mst->sampletype != 'i' )
    {
      fprintf (stderr, "decimate(): Unexpected sample type: '%c'\n",
	       mst->sampletype);
      return -1;
    }
  
  if ( verbose )
    fprintf (stderr, "Decimating time-series by a factor of %d (%g -> %g sps)\n",
             factor, mst->samprate, mst->samprate/factor);
  
  /* Perform the decimation and filtering on int32_t samples */
  numsamples = idecimate (mst->datasamples, mst->numsamples, factor, NULL, -1, -1);
  
  if ( numsamples >= 0 )
    {
      /* Adjust sample rate, sample count, end time and sample buffer size */
      mst->samprate /= factor;
      mst->samplecnt = numsamples;
      mst->numsamples = numsamples;
      mst->endtime = mst->starttime +
        (((double)(mst->numsamples-1)/mst->samprate * HPTMODULUS) + 0.5);
      
      /* Reduce sample buffer to new size */
      if ( ! (mst->datasamples = realloc (mst->datasamples, numsamples*sizeof(int32_t))) )
        {
          fprintf (stderr, "decimate(): Error reallocating sample buffer\n");
          return -1;
        }
    }
  else
    {
      fprintf (stderr, "decimate(): Error decimating time-series\n");
      return -1;      
    }
  
  return 0;
}  /* End of decimate() */


/***************************************************************************
 * packtraces:
 *
 * Pack all traces in a group using per-MSTrace templates.
 *
 * Returns 0 on success, and -1 on failure
 ***************************************************************************/
static void
packtraces (MSTraceGroup *mstg, flag flush)
{
  MSTrace *mst;
  int64_t trpackedsamples = 0;
  int trpackedrecords = 0;
  
  mst = mstg->traces;
  while ( mst )
    {
      
      if ( mst->numsamples <= 0 )
	{
	  mst = mst->next;
	  continue;
	}
      
      trpackedrecords = mst_pack (mst, &record_handler, 0, packreclen, encoding, byteorder,
				  &trpackedsamples, flush, verbose-3, (MSRecord *) mst->prvtptr);
      
      if ( trpackedrecords < 0 )
	{
	  fprintf (stderr, "Error packing data\n");
	}
      else
	{
	  packedrecords += trpackedrecords;
	  packedsamples += trpackedsamples;
	}
      
      mst = mst->next;
    }
}  /* End of packtraces() */


/***************************************************************************
 * record_handler:
 * Saves passed records to the output file.
 ***************************************************************************/
static void
record_handler (char *record, int reclen, void *handlerdata)
{
  if ( fwrite(record, reclen, 1, ofp) != 1 )
    {
      fprintf (stderr, "Error writing to output file\n");
    }
}  /* End of record_handler() */


/***************************************************************************
 * parameter_proc:
 * Process the command line parameters.
 *
 * Returns 0 on success, and -1 on failure.
 ***************************************************************************/
static int
parameter_proc (int argcount, char **argvec)
{
  int optind;
  int idx;
  char *chanliststr = NULL;
  char *deciliststr = NULL;
  
  /* Process all command line arguments */
  for (optind = 1; optind < argcount; optind++)
    {
      if (strcmp (argvec[optind], "-V") == 0)
	{
	  fprintf (stderr, "%s version: %s\n", PACKAGE, VERSION);
	  exit (0);
	}
      else if (strcmp (argvec[optind], "-h") == 0)
	{
	  usage();
	  exit (0);
	}
      else if (strncmp (argvec[optind], "-v", 2) == 0)
	{
	  verbose += strspn (&argvec[optind][1], "v");
	}
      else if (strcmp (argvec[optind], "-C") == 0)
	{
	  chanliststr = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-D") == 0)
	{
	  deciliststr = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-n") == 0)
	{
	  network = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-s") == 0)
	{
	  station = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-l") == 0)
	{
	  location = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-c") == 0)
	{
	  channel = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-S") == 0)
	{
	  srateblkt = 1;
	}
      else if (strcmp (argvec[optind], "-r") == 0)
	{
	  packreclen = strtoul (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-e") == 0)
	{
	  encoding = strtoul (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-b") == 0)
	{
	  byteorder = strtoul (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-o") == 0)
	{
	  outputfile = getoptval(argcount, argvec, optind++);
	}
      else if (strncmp (argvec[optind], "-", 1) == 0 &&
	       strlen (argvec[optind]) > 1 )
	{
	  fprintf(stderr, "Unknown option: %s\n", argvec[optind]);
	  exit (1);
	}
      else
	{
	  addnode (&filelist, NULL, argvec[optind]);
	}
    }

  /* Make sure an input files were specified */
  if ( filelist == 0 )
    {
      fprintf (stderr, "No input files were specified\n\n");
      fprintf (stderr, "%s version %s\n\n", PACKAGE, VERSION);
      fprintf (stderr, "Try %s -h for usage\n", PACKAGE);
      exit (1);
    }

  /* Report the program version */
  if ( verbose )
    fprintf (stderr, "%s version: %s\n", PACKAGE, VERSION);

  /* Parse channel list */
  if ( chanliststr )
    {
      char *sarr[MAX_CHANNELS];
      char *ptr;
      int count = 1;
      int chan;

      /* Parse comma separated list */
      ptr = chanliststr;
      idx = 0;
      sarr[idx] = ptr;
      while ( (ptr = strchr (ptr, ',')) != NULL )
	{
	  idx++;
	  *ptr++ = '\0';
	  sarr[idx] = ptr;
	  count++;
	}
      
      /* Initialize for no channels */
      for (idx = 0; idx < MAX_CHANNELS; idx++)
	chanlist[idx] = 0;
      
      /* Populate channel selections */
      for (idx = 0; idx < count; idx++)
	{
	  chan = strtoul (sarr[idx], NULL, 10);
	  
	  if ( chan < 1 || chan > MAX_CHANNELS )
	    {
	      fprintf (stderr, "Error, invalid channel number: %d, must be 1-%d\n",
		       chan, MAX_CHANNELS);
	      exit (1);
	    }
	  
	  chanlist[chan-1] = 1;
	}
    }
  /* Otherwise initialize for all channels */
  else
    {
      for (idx = 0; idx < MAX_CHANNELS; idx++)
	chanlist[idx] = 1;
    }
  
  /* Initialize for no decimation */
  for (idx = 0; idx < MAX_DECIMATION; idx++)
    decilist[idx] = 0;
  
  /* Parse decimation list */
  if ( deciliststr )
    {
      char *sarr[MAX_DECIMATION];
      char *ptr;
      int count = 1;
      int factor;
      
      /* Parse comma separated list */
      ptr = deciliststr;
      idx = 0;
      sarr[idx] = ptr;
      while ( (ptr = strchr (ptr, ',')) != NULL )
	{
	  idx++;
	  *ptr++ = '\0';
	  sarr[idx] = ptr;
	  count++;
	}
      
      /* Populate decimation list */
      for (idx = 0; idx < count && idx < MAX_DECIMATION; idx++)
	{
	  factor = strtoul (sarr[idx], NULL, 10);
	  
	  if ( factor < 2 || factor > 7 )
	    {
	      fprintf (stderr, "Error, invalid decimation factor: %d, must be 2-7\n", factor);
	      exit (1);
	    }
	  
	  decilist[idx] = factor;
	}
    }
  
  /* Check the input files for any list files, if any are found
   * remove them from the list and add the contained list */
  if ( filelist )
    {
      struct listnode *prevln, *ln;
      char *lfname;
      
      prevln = ln = filelist;
      while ( ln != 0 )
	{
	  lfname = ln->data;
	  
	  if ( *lfname == '@' )
	    {
	      /* Remove this node from the list */
	      if ( ln == filelist )
		filelist = ln->next;
	      else
		prevln->next = ln->next;
	      
	      /* Skip the '@' first character */
	      if ( *lfname == '@' )
		lfname++;

	      /* Read list file */
	      readlistfile (lfname);
	      
	      /* Free memory for this node */
	      if ( ln->key )
		free (ln->key);
	      free (ln->data);
	      free (ln);
	    }
	  else
	    {
	      prevln = ln;
	    }
	  
	  ln = ln->next;
	}
    }

  return 0;
}  /* End of parameter_proc() */


/***************************************************************************
 * getoptval:
 * Return the value to a command line option; checking that the value is 
 * itself not an option (starting with '-') and is not past the end of
 * the argument list.
 *
 * argcount: total arguments in argvec
 * argvec: argument list
 * argopt: index of option to process, value is expected to be at argopt+1
 *
 * Returns value on success and exits with error message on failure
 ***************************************************************************/
static char *
getoptval (int argcount, char **argvec, int argopt)
{
  if ( argvec == NULL || argvec[argopt] == NULL ) {
    fprintf (stderr, "getoptval(): NULL option requested\n");
    exit (1);
    return 0;
  }
  
  /* Special case of '-o -' usage */
  if ( (argopt+1) < argcount && strcmp (argvec[argopt], "-o") == 0 )
    if ( strcmp (argvec[argopt+1], "-") == 0 )
      return argvec[argopt+1];
  
  if ( (argopt+1) < argcount && *argvec[argopt+1] != '-' )
    return argvec[argopt+1];
  
  fprintf (stderr, "Option %s requires a value\n", argvec[argopt]);
  exit (1);
  return 0;
}  /* End of getoptval() */


/***************************************************************************
 * readlistfile:
 *
 * Read a list of files from a file and add them to the filelist for
 * input data.  The filename is expected to be the last
 * space-separated field on the line.
 *
 * Returns the number of file names parsed from the list or -1 on error.
 ***************************************************************************/
static int
readlistfile (char *listfile)
{
  FILE *fp;
  char  line[1024];
  char *ptr;
  int   filecnt = 0;
  
  char  filename[1024];
  char *lastfield = 0;
  int   fields = 0;
  int   wspace;
  
  /* Open the list file */
  if ( (fp = fopen (listfile, "rb")) == NULL )
    {
      if (errno == ENOENT)
        {
          fprintf (stderr, "Could not find list file %s\n", listfile);
          return -1;
        }
      else
        {
          fprintf (stderr, "Error opening list file %s: %s\n",
		   listfile, strerror (errno));
          return -1;
        }
    }
  
  if ( verbose )
    fprintf (stderr, "Reading list of input files from %s\n", listfile);
  
  while ( (fgets (line, sizeof(line), fp)) !=  NULL)
    {
      /* Truncate line at first \r or \n, count space-separated fields
       * and track last field */
      fields = 0;
      wspace = 0;
      ptr = line;
      while ( *ptr )
	{
	  if ( *ptr == '\r' || *ptr == '\n' || *ptr == '\0' )
	    {
	      *ptr = '\0';
	      break;
	    }
	  else if ( *ptr != ' ' )
	    {
	      if ( wspace || ptr == line )
		{
		  fields++; lastfield = ptr;
		}
	      wspace = 0;
	    }
	  else
	    {
	      wspace = 1;
	    }
	  
	  ptr++;
	}
      
      /* Skip empty lines */
      if ( ! lastfield )
	continue;
      
      if ( fields >= 1 && fields <= 3 )
	{
	  fields = sscanf (lastfield, "%s", filename);
	  
	  if ( fields != 1 )
	    {
	      fprintf (stderr, "Error parsing file name from: %s\n", line);
	      continue;
	    }
	  
	  if ( verbose > 1 )
	    fprintf (stderr, "Adding '%s' to input file list\n", filename);
	  
	  addnode (&filelist, NULL, filename);
	  filecnt++;
	  
	  continue;
	}
    }
  
  fclose (fp);
  
  return filecnt;
}  /* End readlistfile() */


/***************************************************************************
 * addnode:
 *
 * Add node to the specified list.
 ***************************************************************************/
static void
addnode (struct listnode **listroot, char *key, char *data)
{
  struct listnode *lastlp, *newlp;
  
  if ( data == NULL )
    {
      fprintf (stderr, "addnode(): No file name specified\n");
      return;
    }
  
  lastlp = *listroot;
  while ( lastlp != 0 )
    {
      if ( lastlp->next == 0 )
        break;
      
      lastlp = lastlp->next;
    }
  
  newlp = (struct listnode *) malloc (sizeof (struct listnode));
  memset (newlp, 0, sizeof (struct listnode));
  if ( key ) newlp->key = strdup(key);
  else newlp->key = key;
  if ( data) newlp->data = strdup(data);
  else newlp->data = data;
  newlp->next = 0;
  
  if ( lastlp == 0 )
    *listroot = newlp;
  else
    lastlp->next = newlp;
  
}  /* End of addnode() */


/***************************************************************************
 * usage:
 * Print the usage message and exit.
 ***************************************************************************/
static void
usage (void)
{
  fprintf (stderr, "%s version: %s\n\n", PACKAGE, VERSION);
  fprintf (stderr, "Convert SDR waveform data to Mini-SEED.\n\n");
  fprintf (stderr, "Usage: %s [options] file1 [file2 file3 ...]\n\n", PACKAGE);
  fprintf (stderr,
	   " ## Options ##\n"
	   " -V              Report program version\n"
	   " -h              Show this usage message\n"
	   " -v              Be more verbose, multiple flags can be used\n"
	   " -C chanlist     List of channel numbers to extract (1-8), e.g. 1,2,3\n"
	   " -D fact,fact,.. Decimate data by various factors (2-7), e.g. 5,4\n"
	   "\n"
	   " -n netcode      Specify the SEED network code, default is XX\n"
	   " -s stacode      Specify the SEED station code, default is SDR\n"
	   " -l locid        Specify the SEED location ID, default is blank (space-space)\n"
	   " -c chancodes    Specify the SEED channel codes, default is channel number\n"
	   " -S              Include SEED blockette 100 for very irrational sample rates\n"
	   " -r bytes        Specify record length in bytes for packing, default: 4096\n"
	   " -e encoding     Specify SEED encoding format for packing, default: 11 (Steim2)\n"
	   " -b byteorder    Specify byte order for packing, MSBF: 1 (default), LSBF: 0\n"
	   "\n"
	   " -o outfile      Specify the output file, default is <inputfile>.mseed\n"
	   "\n"
	   " file(s)         File(s) of SDR input data\n"
	   "                   If a file is prefixed with an '@' it is assumed to contain\n"
	   "                   a list of data files to be read\n"
	   "\n"
	   "Supported Mini-SEED encoding formats:\n"
           " 3  : 32-bit integers\n"
           " 10 : Steim 1 compression of 32-bit integers\n"
           " 11 : Steim 2 compression of 32-bit integers\n"
	   "\n");
}  /* End of usage() */
