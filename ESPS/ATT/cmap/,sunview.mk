#  This material contains proprietary software of Entropic Speech, Inc.   
#  Any reproduction, distribution, or publication without the the prior	   
#  written permission of Entropic Speech, Inc. is strictly prohibited.
#  Any public distribution of copies of this work authorized in writing by
#  Entropic Speech, Inc. must bear the notice			
# 								
#      "Copyright (c) 1991 Entropic Research Lab.
# 				
#  makefile for cmap (Sunview version) %W% %G% ERL
 	

CFLAGS = 
SUNLIB = -lsuntool -lsunwindow -lpixrect -lm

cmap: cmap.o
	rm -f $@
	cc -o cmap cmap.o $(SUNLIB)

install: $(BINDIR)/cmap $(MANDIR)/man1/cmap.1

$(BINDIR)/cmap: cmap
	-strip $?
	-rm -f $(BINDIR)/$?
	cp $? $(BINDIR)/$?
	chmod $(PROGMOD) $(BINDIR)/$?

$(MANDIR)/man1/cmap.1: cmap.1
	-rm -f $(MANDIR)/man1/cmap.1
	cp cmap.1 $(MANDIR)/man1
	chmod $(MANMOD) $(MANDIR)/man1/cmap.1


clean:
	rm -f *.o cmap
