#
# This material contains unpublished, proprietary software of 
# Entropic Research Laboratory, Inc.  Any reproduction, distribution, 
# or publication of this work must be authorized in writing by Entropic 
# Research Laboratory, Inc., and must bear the notice: 
#
#    "Copyright (c) 1987-1990  AT&T, Inc. 
#    "Copyright (c) 1986-1990  Entropic Speech, Inc. 
#    "Copyright (c) 1990-1993  Entropic Research Laboratory, Inc. 
#                   All rights reserved"
#
# The copyright notice above does not evidence any actual or intended 
# publication of this source code.     
#
# %W% %G% ERL
# 
# Written by:  
# Checked by:
# Revised by: David Talkin
# 
#


CFLAGS = -g -I$(ESPS_BASE)/include/waves $(PROGCFLAGS) -I.

LIBSIG = $(ESPS_BASE)/lib/waves/xlibsig.a $(WSPSLIB)
LBIN = $(BINDIR)

# LINE BELOW FOR TESTING ONLY.
#LIBSIG = ../../../libsig/xlibsig.a

hpplay: newplay.o dac_hpaudiolib.o hpaudiolib_stubs.o
	cc -g -o $@  newplay.o dac_hpaudiolib.o hpaudiolib_stubs.o \
	 $(LIBSIG) $(AUDIO_LIB) -lm

pure: newplay.o dac_hpaudiolib.o hpaudiolib_stubs.o
	purify cc -g -o $@  newplay.o dac_hpaudiolib.o hpaudiolib_stubs.o \
	 $(LIBSIG) $(AUDIO_LIB) -lm

newplay.c:
	cp ../../s16audio/newplay.c .


install: hpplay 
	$(STRIP) hpplay
	-rm -f $(BINDIR)/hpplay
	mv -f hpplay $(BINDIR)/hpplay
	chmod $(PROGMOD) $(BINDIR)/hpplay
	-rm -f $(BINDIR)/eplay
	ln -s $(BINDIR)/hpplay $(BINDIR)/eplay

	-rm $(MANDIR)/man1/hpplay.1
	cp hpplay.1 $(MANDIR)/man1/hpplay.1
	chmod $(MANMOD) $(MANDIR)/man1/hpplay.1

clean:
	-rm -f *.o  core


