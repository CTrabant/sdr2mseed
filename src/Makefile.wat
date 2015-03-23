#
# Wmake File - for Watcom's wmake
# Use 'wmake -f Makefile.wat'

.BEFORE
	@set INCLUDE=.;$(%watcom)\H;$(%watcom)\H\NT
	@set LIB=.;$(%watcom)\LIB386

cc     = wcc386
cflags = -zq
lflags = OPT quiet OPT map LIBRARY ..\libmseed\libmseed.lib
cvars  = $+$(cvars)$- -DWIN32

BIN = ..\sdr2mseed.exe

INCS = -I..\libmseed 

all: $(BIN)

$(BIN):	decimate.obj sdr2mseed.obj
	wlink $(lflags) name $(BIN) file {decimate.obj sdr2mseed.obj}

# Source dependencies:
decimate.obj:	decimate.h decimate.c
sdr2mseed.obj:	sdr2mseed.c

# How to compile sources:
.c.obj:
	$(cc) $(cflags) $(cvars) $(INCS) $[@ -fo=$@

# Clean-up directives:
clean:	.SYMBOLIC
	del *.obj *.map $(BIN)
