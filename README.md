# sdr2mseed 

##Convert (Win)SDR waveform data to Mini-SEED.

The data read by this converter is the continuous recording format used by
the [WinSDR](http://psn.quake.net/winsdr/) software.

### Building/Installing 

In most environments a simple 'make' will build the program.

In the Win32 environment the Makefile.wat can be used with Open
Watcom's wmake program.

Using GCC, running 'make static' will compile a static version
if possible.

For further installation simply copy the resulting binary and man page
(in the 'doc' directory) to appropriate system directories.

### Licensing

See the included LICENSE file
