/*----------------------------------------------------------------------+
|									|
|   This material contains proprietary software of Entropic Speech,	|
|   Inc.  Any reproduction, distribution, or publication without the	|
|   prior written permission of Entropic Speech, Inc. is strictly	|
|   prohibited.  Any public distribution of copies of this work		|
|   authorized in writing by Entropic Speech, Inc. must bear the	|
|   notice								|
|									|
|   "Copyright (c) 1989, 1990 Entropic Speech, Inc.                     |
|                             All rights reserved."	                |
|									|
+-----------------------------------------------------------------------+
|									|
|  Module: imdevx11.c							|
|									|
|  This program displays data from an ESPS file as a half-tone		|
|  gray-scale image.							|
|									|
|  Rodney W. Johnson, Entropic Speech, Inc.				|
|									|
+----------------------------------------------------------------------*/
#ifndef lint
static char *sccs_id = "%W% %G%	ESI";
#endif

#ifdef XWIN

#include <stdio.h>
#include <esps/esps.h>
#include <esps/unix.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include "image.h"

#define GREY_SCALE_SIZE 256

void		plotimage();
void		set_margins();

static void	get_config();
static void	x11_row(), x11_initbits(), x11_plotline();
static void	setcmap();
static void 	main_loop();

Pixmap		make_icon();
static int assign();

extern int	debug_level;
extern long	width, height;
extern long	nrows, ncols;
extern long	lmarg, rmarg, tmarg, bmarg;
extern int	scale;

extern int	oflag;
extern int	dev;
extern int	gray_bits;
extern int	Bflag;

extern void	(*dev_initbits)();
extern void	(*dev_row)();
extern void	(*dev_plotline)();

extern int      Wflag;
extern char     *geometry;

extern int	Argc;
extern char	**Argv;

static int	rownum;

static Display	*display;
static int  	screen;
static Window  	window;
static GC   	context;
Colormap    	colormap;
unsigned long	color_map[GREY_SCALE_SIZE];
unsigned long	white, black;
static Pixmap	pixmap = None;
/*
static XImage	ximage;
*/


int
x11_init()
{
    int	    	avail_bits;
    int	    	win_x = 300, win_y = 300;
    int	    	border_width = 1;
    char    	*window_name = "ESPS image plot";
    char    	*icon_name = "ESPS image plot";
    Pixmap  	icon_pixmap;
    XSizeHints	size_hints;
    int	    	win_width, win_height;
    int         parse_map;

    dev_row = x11_row;
    dev_initbits = x11_initbits;
    dev_plotline = x11_plotline;

    if(!assign()) return 0;

    get_config(&avail_bits);

    if (avail_bits < gray_bits)
    {
	Fprintf(stderr, "%s: %s - exiting\n", "x11_init",
		"system hasn't enough gray-scale resolution for algorithm");
	exit(1);
    }

    switch (gray_bits)
    {
    case 4:
	setcmap(4);
	white = color_map[0];
	black = color_map[15];
	break;
    case 1:
	black = BlackPixel(display, screen);
	white = WhitePixel(display, screen);
	break;
    }

    win_width = width + lmarg + rmarg;
    win_height = height + tmarg + bmarg;

    if(Wflag)
        parse_map = XParseGeometry(geometry, &win_x, &win_y, &win_width, &win_height);

    window = XCreateSimpleWindow(display, RootWindow(display, screen),
				 win_x, win_y, win_width, win_height,
				 border_width, black, white);

    icon_pixmap = make_icon(display, screen, "IMAGE");

    size_hints.flags = PPosition | PSize | PMinSize;
    size_hints.x = win_x;
    size_hints.y = win_y;
    size_hints.width = win_width;
    size_hints.height = win_height;
    size_hints.min_width = 2 + 2*border_width;
    size_hints.min_height = 2 + 2*border_width;
    XSetStandardProperties(display, window, window_name, icon_name,
			   icon_pixmap, Argv, Argc, &size_hints);

    context = XCreateGC(display, window,
			(unsigned long) None, (XGCValues *) NULL);

    XSelectInput(display, window, ExposureMask);

    XMapWindow(display, window);

    main_loop();
}

static void
x11_initbits()
{

}

static void
x11_row(row)
    char    *row;
{
    int	    v;

    if (oflag)
    {
	switch (gray_bits)
	{
	case 4:
	    for (v = 0; v < ncols; v++)
	    {
		XSetForeground(display, context, color_map[row[v]]);
		XDrawPoint(display, pixmap, context,
			   v + lmarg, tmarg + rownum);
	    }
	    break;
	case 1:
	    XSetForeground(display, context, black);
	    for (v = 0; v < ncols; v++)
	    {
		if (row[v])
		    XDrawPoint(display, pixmap, context,
			       v + lmarg, tmarg + rownum);
	    }
	    break;
	}

	if (rownum%8 == 7)
	    XCopyArea(display, pixmap, window, context,
		      (int) lmarg, (int) tmarg + rownum - 7,
		      (unsigned) ncols, (unsigned) 8,
		      (int) lmarg, (int) tmarg + rownum - 7);
    }
    else
    {
	switch (gray_bits)
	{
	case 4:
	    for (v = 0; v < ncols; v++)
	    {
		XSetForeground(display, context, color_map[row[v]]);
		XDrawPoint(display, pixmap, context,
			   rownum + lmarg, tmarg + ncols - v - 1);
	    }
	    break;
	case 1:
	    XSetForeground(display, context, black);
	    for (v = 0; v < ncols; v++)
	    {
		if (row[v])
		    XDrawPoint(display, pixmap, context,
			       rownum + lmarg, tmarg + ncols - v - 1);
	    }
	    break;
	}

	if (rownum%8 == 7)
	    XCopyArea(display, pixmap, window, context,
		      rownum + (int) lmarg - 7, (int) tmarg,
		      (unsigned) 8, (unsigned) ncols,
		      rownum + (int) lmarg - 7, (int) tmarg);
    }

    rownum++;
}

void
x11_fin()
{
}

void
x11_default_size(w, h)
    long    *w, *h;
{
    if (oflag)
    {
	*w = 150;
	*h = 600;
    }
    else
    {
	*w = 600;
	*h = 150;
    }
}

int
x11_depth()
{
    int	    avail_bits;

    if(assign()) {
    	get_config(&avail_bits);
    	return avail_bits;
    }
    else
	return 0;
}

static int
assign()
{
    static int	dsplassigned = NO;

    if (!dsplassigned)
    {
	display = XOpenDisplay((char *) NULL);
	if(display == NULL) return 0;
	screen = DefaultScreen(display);
	dsplassigned = YES;
	return 1;
    }
    return 1;
}

static void
x11_plotline(n, h, v)
    long    n;
    long    *h, *v;
{
    XPoint  points[100];
/*!*//* use malloc; keep track of alloc size, etc. */
    int	    i;
    int	    h_off = lmarg;
    int	    v_off = height - 1 + tmarg;

    XSetForeground(display, context, black);

    for (i = 0; i < n; i++)
    {
	points[i].x = h_off + h[i];
	points[i].y = v_off - v[i];
    }

    XDrawLines(display, pixmap, context, points, (int) n, CoordModeOrigin);
}

/*
 * Code for create_grey_scale() taken from Speechbench module sb_colors.c
 *   by Scott Katz.  Copyright (c) 1988, 1989 University of Colorado.
 */
/* create a grey scale of the specified number of entries. index 0 will be
	white, and the highest index will be black */
create_grey_scale(num_entries)
    int	    num_entries;
{
    int	    i, intens, status;
    XColor  color;
	
    for(i=0; i < num_entries; i++)
    {
	intens = i * 65535/(num_entries-1);
	color.red = (short) intens;
	color.green = (short) intens;
	color.blue= (short) intens;
	status = XAllocColor(display, colormap, &color);
	if(status == 0)
	    Fprintf(stderr,
		    "create_grey_scale: could not allocate grey %d\n",i);
	else
	    color_map[num_entries-i-1] = color.pixel;
    }
}

/* Create a color map with the specified number of entries from the
 * cmap_len rgb triples in cmap.
 * (Extern variables cmap_len and cmap are set in image.c.)
 * Return NO on failure, YES on success.
 * Function modeled on create_grey_scale() above.
 */
int
create_color_map(num_entries)
    int		num_entries;
{
    extern int	(*cmap)[3];	/* array of RGB triples */
    extern int	cmap_len;	/* number of colormap entries */
    int		i, status;
    XColor	color;
	
    if (!cmap) return NO;
    if (num_entries != cmap_len) return NO;
    for(i=0; i < cmap_len; i++)
    {
	color.red = (short) cmap[i][0] * 65535 / 255;
	color.green = (short) cmap[i][1] * 65535 / 255;
	color.blue = (short) cmap[i][2] * 65535 / 255;
	status = XAllocColor(display, colormap, &color);
	if(status == 0)
	{
	    Fprintf(stderr,
		    "create_color_map: could not allocate entry %d\n", i);
	    return NO;
	}
	else
	    color_map[i] = color.pixel;
    }
    return YES;
}

static void
setcmap(n)
    int	    n;
{
    switch (n)
    {
    case 1:
	break;
    case 4:
	colormap = DefaultColormap(display, screen);
	if (!create_color_map(16))
	    create_grey_scale(16);
	break;
    default:
	break;
    }
}

static void
get_config(avail)
    int	    *avail;
{
    *avail = DisplayPlanes(display, screen);
}

static void
main_loop()
{
    XEvent  event;
    int	     dum;
    unsigned int w, h, d, dumu;
    unsigned int	  win_width = 0, win_height = 0;
    int	    window_too_small = NO;
    Window root_return;

    for (;;)
    {
	XNextEvent(display, &event);
	switch (event.type & 0x7f)
	{
	case Expose:

	    if (debug_level)
	    	Fprintf(stderr,"Event expose\n");
	    XGetGeometry(display, window, &root_return,
			 &dum, &dum, &w, &h, &dumu, &d);
	    if (w != win_width || h != win_height)
	    {
		win_width = w;
		win_height = h;
		width = win_width - lmarg - rmarg;
		height = win_height - tmarg - bmarg;

		if (pixmap != None) XFreePixmap(display, pixmap);
		pixmap = XCreatePixmap(display, window, w, h, d);

		XSetForeground(display, context, white);
		XFillRectangle(display, pixmap, context, 0, 0, w, h);

		if (debug_level)
		    Fprintf(stderr, "New size %dx%d\n", w, h);

		if (Bflag)
		{
		    window_too_small = width < 2 || height < 2;
		    if (window_too_small)
		    {
			width = win_width;	height = win_height;
			Bflag = NO;
			lmarg = rmarg = tmarg = bmarg = 0;
		    }
		}
		nrows = oflag ? height : width;
		ncols = oflag ? width : height;

		rownum = 0;
		plotimage();
		if (window_too_small)
		{
		    window_too_small = NO;
		    Bflag = YES;
		    set_margins(Bflag, scale, &lmarg, &rmarg, &tmarg, &bmarg);
		}
	    }

	    if (debug_level)
		Fprintf(stderr,
			"Event type %d\n x %d\ty %d\n w %d\th %d\n c %d\n",
			event.xexpose.type, event.xexpose.x, event.xexpose.y,
			event.xexpose.width, event.xexpose.height,
			event.xexpose.count);

	    XCopyArea(display, pixmap, window, context,
		      event.xexpose.x, event.xexpose.y,
		      (unsigned) event.xexpose.width,
		      (unsigned) event.xexpose.height,
		      event.xexpose.x, event.xexpose.y);

	    break;
	default:
	    break;
	}
    }
}

#else
/* Dummy function definitions for systems without X Windows. */

#include <stdio.h>
#include <esps/esps.h>

void
x11_init()
{
    Fprintf(stderr, "%s: %s - exiting\n", "x11_init",
	    "no X Windows support");
    exit(1);
}


void
x11_fin()
{
    Fprintf(stderr, "%s: %s - exiting\n", "x11_fin",
	    "no X Windows support");
    exit(1);
}

void
x11_default_size(w, h)
    long    *w, *h;
{
    Fprintf(stderr, "%s: %s - exiting\n", "x11_default_size",
	    "no X Windows support");
    exit(1);
}

int
x11_depth()
{
    Fprintf(stderr, "%s: %s - exiting\n", "x11_depth",
	    "no X Windows support");
    exit(1);
}

#endif
