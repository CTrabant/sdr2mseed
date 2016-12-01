# <p >SDR to Mini-SEED converter</p>

1. [Name](#)
1. [Synopsis](#synopsis)
1. [Description](#description)
1. [Options](#options)
1. [List Files](#list-files)
1. [Caveats](#caveats)
1. [Author](#author)

## <a id='synopsis'>Synopsis</a>

<pre >
sdr2mseed [options] file1 [file2 file3 ...]
</pre>

## <a id='description'>Description</a>

<p ><b>sdr2mseed</b> converts (Win)SDR "seisdata" waveform data to Mini-SEED format.  By default all channels in the SDR will be converted, channel selections are possible with the <b>-C</b> option. The channels can also be decimated during convertion using the <b>-D</b> option.</p>

<p >If an input file name is prefixed with an '@' character the file is assumed to contain a list of input data files, see <i>LIST FILES</i> below.</p>

<p >The default output file names are the same as the input files with a ".mseed" suffix.  The output data may be re-directed to a single file or stdout using the -o option.</p>

## <a id='options'>Options</a>

<b>-V</b>

<p style="padding-left: 30px;">Print program version and exit.</p>

<b>-h</b>

<p style="padding-left: 30px;">Print program usage and exit.</p>

<b>-v</b>

<p style="padding-left: 30px;">Be more verbose.  This flag can be used multiple times ("-v -v" or "-vv") for more verbosity.</p>

<b>-C </b><i>chanlist</i>

<p style="padding-left: 30px;">Specify the channels to extract from the SDR data files where <i>chanlist</i> is a comma-seprated list of channel numbers to extract. For example, "-C 1,2,3" will limit the conversion to channels 1, 2 and 3.  By default all channels are converted.</p>

<b>-D </b><i>factor,factor,...</i>

<p style="padding-left: 30px;">Decimate the time series data during conversion by one or more factors.  Decimation factors must be between 2 and 7 and are specified as a comma-separated list.  For example, "-D 5,4,2" will decimate the time series by a factor of 5, then 4 and finally 2 for total decimation factor of 40 (e.g. reducing 200 sps to 5 sps).</p>

<b>-n </b><i>netcode</i>

<p style="padding-left: 30px;">Specify the SEED network code to use, maximum of 2 characters.  The default network code is "XX" indicating an experimental data set. Network codes are allocated by the Federation of Digital Seismograph Networks.  It is highly recommended to avoid making data public using unassigned or unowned network codes.</p>

<b>-s </b><i>stacode</i>

<p style="padding-left: 30px;">Specify the SEED station code to use, 1-5 characters.  The default station code is SDR.</p>

<b>-l </b><i>locid</i>

<p style="padding-left: 30px;">Specify the SEED location ID to use.  The default location ID is blank/empty.  Generally location IDs are used to logcially separate multiple instruments at the same site.</p>

<b>-c </b><i>chancodes</i>

<p style="padding-left: 30px;">Specify the SEED channel codes to use.  The default channel codes are a three digit value representing the SDR channel number, this does not follow the SEED convention which uses codes for bandwidth, instrument type and orientation.</p>

<b>-S</b>

<p style="padding-left: 30px;">Include SEED blockette 100 in each output record with the sample rate in floating point format.  The basic format for storing sample rates in SEED data records is a rational approximation (numerator/denominator).  Precision will be lost if a given sample rate cannot be well approximated.  This option should be used in those cases.</p>

<b>-r </b><i>bytes</i>

<p style="padding-left: 30px;">Specify the Mini-SEED record length in <i>bytes</i>, default is 4096.</p>

<b>-e </b><i>encoding</i>

<p style="padding-left: 30px;">Specify the Mini-SEED data encoding format, default is 11 (Steim-2 compression).  Other supported encoding formats include 10 (Steim-1 compression) and 3 (32-bit integers).</p>

<b>-b </b><i>byteorder</i>

<p style="padding-left: 30px;">Specify the Mini-SEED byte order, default is 1 (big-endian or most significant byte first).  The other option is 0 (little-endian or least significant byte first).  It is highly recommended to always create big-endian SEED.</p>

<b>-o </b><i>outfile</i>

<p style="padding-left: 30px;">Write all Mini-SEED records to <i>outfile</i>, if <i>outfile</i> is a single dash (-) then all Mini-SEED output will go to stdout.  All diagnostic output from the program is written to stderr and should never get mixed with data going to stdout.</p>

## <a id='list-files'>List Files</a>

<p >If an input file is prefixed with an '@' character the file is assumed to contain a list of file for input.  Multiple list files can be combined with multiple input files on the command line.  The last, space separated field on each line is assumed to be the file name to be read.</p>

<p >An example of a simple text list:</p>

<pre >
seisdata.013
seisdata.014
seisdata.015
</pre>

## <a id='caveats'>Caveats</a>

<p >Each input SDR file is processed independently; this means that a long, multi-file time series will not be processed as a single time series. When decimating a time series across many files the end effects of the anti-alias filter could potentially become evident.  The advantage is that only the data from a single file needs to be in memory at any given time.</p>

## <a id='author'>Author</a>

<pre >
CT
</pre>


(man page 2011/01/31)
