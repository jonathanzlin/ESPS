/*
 * This material contains unpublished, proprietary software of 
 * Entropic Research Laboratory, Inc. Any reproduction, distribution, 
 * or publication of this work must be authorized in writing by Entropic 
 * Research Laboratory, Inc., and must bear the notice: 
 *
 *    "Copyright (c) 1990-1993  Entropic Research Laboratory, Inc. 
 *                   All rights reserved"
 *
 * The copyright notice above does not evidence any actual or intended 
 * publication of this source code.     
 *
 * Written by:  John Shore
 * Checked by:
 * Revised by:  Stephen Marcus - adding scrolling file list
 *
 * Brief description:
 *
 *   functions for popping up buttons that invoke programs on the named file
 */

static char *sccs_id = "@(#)fbuttons.c	1.33	7/8/96	ERL";

/*
 * system include files
 */

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>
#include <xview/xview.h>
#include <xview/canvas.h>
#include <xview/icon_load.h>
#include <xview/panel.h>
#include <xview/scrollbar.h>
#include <xview/svrimage.h>
#include <xview/termsw.h>
#include <xview/text.h>
#include <xview/tty.h>
#include <xview/xv_xrect.h>
#include <xview/cms.h>


/*
 * esps include files
 */

#include <esps/esps.h>
#include <esps/unix.h>
#include <esps/exview.h>

#if defined(HP700)  || defined(OS5)
#include <regex.h>
#endif


/*
 * defines
 */

#ifndef NULL
#define NULL
#endif

#define LIST_INC 50
#define DEF_REGEXP "\\.s*d$"   /* selects files with .sd or .d suffix */

#define Fprintf (void)fprintf
#define Fflush (void)fflush

#define REQUIRE(test,text) {if (!(test)) {(void) fprintf(stderr, \
"fbuttons: %s - exiting\n", text); exit(1);}}

#define PROG Fprintf(stderr, "%s: ", ProgName)

#define DEBUG(n) if (debug_level >= n) Fprintf

#define ERROR_EXIT(text) {(void) fprintf(stderr, "%s: %s - exiting\n", \
		ProgName, text); SYNTAX; exit(1);}

#define EXIT Fprintf(stderr, "\n"); exit(1);

#define ERROR_EXIT1(fmt,a) {PROG; Fprintf(stderr, (fmt), (a)); EXIT}

#define TRYALLOC(type,num,var,msg) { \
    if (((var) = (type *) calloc((unsigned)(num),sizeof(type))) == NULL) \
    ERROR_EXIT1("Can't allocate memory--%s", (msg))}

#define SYNTAX USAGE ("fbuttons [-F file_menu] [-s max_scroll_lines] \
[-R regexp]\n\t[-L proglist_file] [-S progmenu_file] [-M mbutton_menu] [-a]\
[-c] [-v]\n\t[-q quit_button] [-Q quit_label] [-l quit_command] \
 [-b but_per_row]\n\t[-w window_title] [-i icon_title] [-X x_pos] \
[-Y y_pos]\n\t[-x debug_level] [files...]")

/*
 * system functions and variables
 */

int getopt ();
extern  optind;
extern	char *optarg;
extern int fullscreendebug;	/* global in xview library */

/*
 * global function declarations
 */

void exec_command();
#if !defined(hpux) && !defined(OS5) && !defined(DEC_ALPHA) && !defined(LINUX_OR_MAC)
char *calloc();
#endif
char *savestring();
char *find_esps_file();
void print_but_def();
#if !defined(hpux)
char *re_comp();
#endif
char **atoarrays();
void addstr();

static void run_cmd_on_file();
static void select_prog();
static void quit_proc();
static int get_commands();
static int find_files();

static void which_file();
static void *reaper();


/*
 * global variable declarations
 */

int	debug_level = 0;
int     do_color = 0;
char	*ProgName = "fbuttons";

static char *q_command = NULL;	/* command on QUIT */

static char **prog_commands = NULL;
static char **prog_names = NULL;
static int curr_prog_ind = 0;

static  int file_width;		/* width of file panel */
static  int file_height;	/* height of file panel */

static char *last_file_string;
static Panel_item last_file_item;
static int new_file_item = 0;

/* more idiocy to handle events in scroll panel properly */

static int file_left_lim, file_width_lim, file_top_lim, file_height_lim;

static Rect *g_file_choice_rect; /* for notify interposer */
static Rect *sbr;			/*scrollbar area */

static int mstrcmp();

/*
 * main program
 */
main (argc, argv)
int argc;
char **argv;
{

  int c;			/* for getopt return */
  char *reg_expr=NULL;		/* regexp for selecting file names (-R)*/
  int Rflag = 0;
  char *proglist_file;		/* file containing program list (-L) */
  int Lflag = 0;		/* flag for -L */
  FILE *proglist_strm;		/* stream for program list */
  char *Mbuttons_file;		/* file from -M  */
  char *mbuttons_file;		/* file for menu buttons*/
  int Mflag = 0;		/* flag for -M */
  long n_progs = 0;		/* number of programs from file*/
  int long_prog = 0;		/* length of longest program name */
  int Sflag = 0;		/* flag for -S */
  char *Progmenu_file = NULL;	/* file from -S */
  char *progmenu_file = NULL;	/* menu file for program buttons */
  char **file_names = NULL;	/* list of files for file panel */
  int  file_count;              /* count of files in list */
  char **rexp_files = NULL;
  int  n_rexp_files = 0;
  char *title = "Run Program on File";   /* title for window */
  char *icon_title = "fbuttons"; /* icon title */
  int Fflag = 0;		/* flag for -F */
  int aflag = 0;		/* flag for -a */
  char *File_menu = NULL;	/* name of file from -F */
  char *file_menu = NULL;	/* menu file for file buttons */
  FILE *file_strm;
  int but_per_row = 10;		/* buttons per row */
#ifndef HP700
  Frame frame = (Frame *)NULL;		/* main frame */
#else
  Frame frame = 0;
#endif
  int frame_width;		/* width of main frame */
  Panel file_panel;		/* panel with file buttons */
  Frame file_panel_owner;	/* frame that owns the file button panel */
  Frame mbut_panel_owner;	/* frame that owns the general button panel */
  Panel mbut_panel;		/* panel with general menu buttons */
#ifndef HP700
  Panel prog_panel = (Panel *)NULL;	/* panel for program buttons */
#else
  Panel prog_panel = 0;	/* panel for program buttons */
#endif
  int prog_width;		/* width of program panel */
  int prog_height, mbut_height;
  int q_button = 1;		/* have quit button? (-q) */
  char *q_label = "QUIT";	/* label for quit button */
  Panel_item prog_choice, file_choice;
  int i;
  int x_pos = -1;
  int y_pos = -1;
  bbox_par *fbox_params = NULL;
  bbox_par *mbox_params = NULL;
  char *prog_label = "Program:"; /* default label for program panel choices */
  int Scrollflag = 0;		/* flag for -s (scrolling in file panel) */
  int max_scroll_lines = 15;    /* max lines displayed in scroll panel */
  int button_choice = 0;	/* if 1, panel_choices instead of menu 
				   buttons in mbuttons panel*/
  int choice_horiz = 0;		/* if 1, panel_choices are horizontal */
  Scrollbar	file_choice_scrollbar;  /*scrollbar if -s used */

  Notify_value my_file_interposer();

  fullscreendebug = 1;		/* this global prevents server grabs that
				   crash SGIs */
  if (xv_init(XV_INIT_ARGC_PTR_ARGV, &argc, argv, 0) == NULL) {
    Fprintf(stderr, "%s: XView init failed; is DISPLAY set?\n", ProgName);
    exit(1);
  }
  
  while ((c = getopt (argc, argv, "R:F:x:w:i:b:L:S:M:X:Y:s:q:Q:l:p:ach")) != EOF) {
    switch (c) {
    case 'c':
      button_choice = 1;
      break;
    case 'h':
      choice_horiz = 1;
      break;
    case 'x': 
      debug_level = atoi(optarg);
      break;
    case 'a': 
      aflag++;
      break;
    case 'b': 
      but_per_row = atoi(optarg);
      break;
    case 'w':
      title = optarg;
      break;
    case 'i':
      icon_title = optarg;
      break;
    case 'F':
      File_menu = optarg;
      Fflag++;
      if (debug_level) 
	Fprintf(stderr, "%s: used -F, will use file list from %s\n", 
	      ProgName, File_menu);
      break;
    case 'L':
      proglist_file = optarg;
      Lflag++;
      if (debug_level) 
	Fprintf(stderr, "%s: used -L, will use program list from %s\n", 
	      ProgName, proglist_file);
      break;
    case 'R':
      reg_expr = optarg;
      Rflag++;
      if (debug_level) 
	Fprintf(stderr, "%s: used -R, will use regexp %s\n", 
	      ProgName, reg_expr);
      break;
    case 'S':
      Progmenu_file = optarg;
      Sflag++;
      if (debug_level) 
	Fprintf(stderr, 
		"%s: used -S, will make program buttons from menu file %s\n", 
	      ProgName, Progmenu_file);
      break;
    case 'M':
      Mbuttons_file = optarg;
      Mflag++;
      if (debug_level) 
	Fprintf(stderr, 
		"%s: used -M, will make mbuttons panel from menu file %s\n", 
	      ProgName, Mbuttons_file);
      break;
    case 'X':
      x_pos = atoi(optarg);
      break;
    case 'Y':
      y_pos = atoi(optarg);
      break;
    case 's':
      Scrollflag++;
      if (atoi(optarg) > 0) max_scroll_lines = atoi(optarg);
      if (debug_level) 
	Fprintf(stderr, 
	    "%s: used -s, will make scrollable file panel, max_lines = %d\n",
		ProgName, max_scroll_lines);
      break;
    case 'p':
      prog_label = optarg;  /* label for prog panel buttons */
      break;
    case 'q':
      /* note that BBOX_QUIT_BUTTON environment variable forces 
	 a quit button in a global way */
      q_button = atoi(optarg);
      break;
    case 'Q':
      q_label = optarg;
      break;
    case 'l':
      q_command = optarg;
      break;
    default:
      SYNTAX;
    }
  }

  /*
   * Setup signal handler to reap dead children
   */

  signal(SIGCHLD, reaper);

  if (choice_horiz == 1 && button_choice == 0)
    Fprintf(stderr, "%s: ignoring -h since -c was not used.\n", ProgName);

  if (button_choice == 1 && Mflag == 0) 
      Fprintf(stderr, "%s: ignoring -c since -M was not used.\n", ProgName);

  if (Mflag) {
      if ((mbuttons_file = FIND_ESPS_MENU(NULL, Mbuttons_file)) == NULL) {

	  Fprintf(stderr, "%s: can't find or read menu button file %s\n", 
		  ProgName, Mbuttons_file);
	  Mflag = 0;
	}
      else
	if (debug_level) 
	  Fprintf(stderr, "%s: using menu file %s for mbuttons\n", 
		  ProgName, mbuttons_file);
    }

  if (Sflag && Lflag)
    ERROR_EXIT("Can't specify programs with both -S and -L");

  if (Sflag) {
      if ((progmenu_file = FIND_ESPS_MENU(NULL, Progmenu_file)) == NULL) {
	  Fprintf(stderr, "%s: can't find or read program menu file %s\n", 
		  ProgName, Progmenu_file);
	  if (!Lflag) {
	      /* without other options, we just exit */
	      exit(1);
	    }
	  else  /* just go with what can be read via -E */
	    Sflag = 0;
	}
      else
	if (debug_level) 
	  Fprintf(stderr, 
		  "%s: using program menu file %s\n", ProgName, progmenu_file);
    }

  /* use default regular expression if no files are given and a regular 
     expression isn't given */

  if ((optind == argc) && !Rflag && !Fflag) {
      Rflag = 1;
      reg_expr = DEF_REGEXP;
    }

  file_count = 0;
  while (optind < argc) {
      addstr(argv[optind], &file_names);
      optind++;
      file_count++;
    }

  if (Rflag) {
      n_rexp_files = find_files(".", reg_expr, &rexp_files);

      for (i = 0; i < n_rexp_files; i++) {
	addstr(rexp_files[i], &file_names);
	file_count++;
      }
    }

  if (Fflag) {
      if ((file_menu = FIND_ESPS_MENU(NULL, File_menu)) == NULL) {
	  Fprintf(stderr, "%s: can't find or read file menu file %s\n", 
		  ProgName, File_menu);
	  Fflag = 0;
	}
      else {
	  if (Scrollflag) {
	      Fprintf(stderr, "%s: can't scroll with -F (-s ignored)\n");
	      Scrollflag = 0; /* cannot scroll with -F option */
	    }
        }
    }

  if (!Fflag && ((file_names == NULL) || (file_names[0] == NULL)))
      ERROR_EXIT("no file buttons specified");


  if (aflag && (file_names != NULL))   /* sort the list */
    qsort(file_names, strlistlen(file_names), sizeof(char *), mstrcmp);

  /* set program selections */

  if (Lflag) {
      TRYOPEN (ProgName, proglist_file, "r", proglist_strm);
      prog_names = (char **) atoarrays(proglist_strm, &n_progs, &long_prog);
      if (prog_names == NULL)
	ERROR_EXIT("Couldn't create program buttons");
      fclose(proglist_strm);
    }
  else if (Sflag) {
      if (get_commands(progmenu_file, &prog_names, &prog_commands) == -1)
	ERROR_EXIT("Couldn't create program buttons");
    }
  else {
      /* use default programs */

      progmenu_file = FIND_ESPS_MENU(NULL, "fbutt.def.men");

      if ((progmenu_file != NULL) 
	&& (get_commands(progmenu_file, &prog_names, &prog_commands) != -1)) {

	  Sflag = 1;
	}
      else {

	  Fprintf(stderr, 
	     "%s: can't find or read default program buttons menu;\n\t  using internal defaults.\n"
,
		  ProgName);

	  addstr("play -r1:", &prog_names);
	  addstr("xtext psps -aD", &prog_names);
	}
    }

  /* program names and command lines are distinct only for -S */

  if (!Sflag) 
    prog_commands = prog_names;

  /* create base frame*/ 

  frame = (Frame)xv_create(NULL, FRAME, 
			   XV_SHOW, FALSE,
			   XV_LABEL, title,
			   FRAME_SHOW_FOOTER, FALSE,
			   FRAME_SHOW_RESIZE_CORNER, TRUE,
			   0);

  (void) exv_attach_icon(frame, ERL_NOBORD_ICON, icon_title, TRANSPARENT);

  /* create optional menu buttons panel */


  if (Mflag) {

      /* get default parameter set for menu button box */
      
      mbox_params = exv_bbox((bbox_par *)NULL, &mbut_panel_owner, &mbut_panel);

      /* change the relevant defaults */

      mbox_params->menu_file = mbuttons_file;
      mbox_params->owner = frame;
      mbox_params->n_per_row = but_per_row;
      mbox_params->show = 0;
      mbox_params->button_choice = button_choice;
      mbox_params->choice_horizontal = choice_horiz;

      /* create button box */

      (void) exv_bbox(mbox_params, &mbut_panel_owner, &mbut_panel);

    }

  /* create program panel */ 

  prog_panel = (Panel)xv_create(frame, PANEL, 
				OPENWIN_SHOW_BORDERS, TRUE,
				WIN_BORDER, TRUE,
				0);

  /* add the optional QUIT button here if not on buttons panel */

  if (q_button)
  {
    prog_choice = xv_create(prog_panel, PANEL_BUTTON,
			  XV_Y, (but_per_row == 1 ? 10 : 25), 
			  PANEL_LABEL_STRING, q_label,
			  PANEL_CLIENT_DATA, frame,
			  PANEL_NOTIFY_PROC, quit_proc,
			  0);
  }
  
  /* add pushbutton set for program selection; Have the QUIT button
   * and "Program" label count as one button width, except that that 
   * in the case of but_per_row == 1, we put the "Program" label above
   * the pushbuton set 
   */

  if (but_per_row == 1) 
  {
    prog_choice = xv_create(prog_panel, PANEL_CHOICE,
			      PANEL_LAYOUT, PANEL_VERTICAL,
			      PANEL_LABEL_STRING, prog_label,
			      PANEL_LABEL_BOLD, TRUE, 
			      PANEL_CHOICE_NCOLS, 1,
			      0);

    xv_set(prog_choice, PANEL_NEXT_ROW, -1, 0);
  }
  else 
  {
    prog_choice = xv_create(prog_panel, PANEL_CHOICE,
			    XV_Y, 8,
			    PANEL_CHOICE_NCOLS, (q_button ? (but_per_row - 1) : but_per_row),
			    PANEL_LAYOUT, PANEL_VERTICAL,
			    PANEL_LABEL_STRING, prog_label,
			    0);
  }

  xv_set(prog_choice, 
	 PANEL_VALUE, 0,
	 PANEL_NOTIFY_PROC, select_prog,
	 0);

  i = 0;

  while (prog_names[i] != NULL) {
	xv_set(prog_choice, PANEL_CHOICE_STRING, i, prog_names[i], 0);
	i++;
      }

  window_fit(prog_panel);

  if (!Scrollflag) {
	/* create standard file button box */

	/* get default parameter set for file button box */

        fbox_params = exv_bbox((bbox_par *)NULL, 
			       &file_panel_owner, &file_panel);

	/* change the relevant defaults */

	fbox_params->owner = frame;
	fbox_params->n_per_row = but_per_row;
	fbox_params->but_labels =   fbox_params->but_data = file_names;
	if (Fflag)
	  fbox_params->menu_file = file_menu;
	fbox_params->but_data_proc = run_cmd_on_file;
	fbox_params->title = title;
	fbox_params->show = 0;
	fbox_params->quit_button = 0;

	/* create button box */

	(void) exv_bbox(fbox_params, &file_panel_owner, &file_panel);
  }
  else {
	/* create a scrolling list for file menu */

	/* create file panel */ 

	    /* this is a workaround for the bug in OpenWindows 2.0 */
	    /* which causes the scrollbar to detach from PANEL_LIST */
	    /* if it is moved - so let's put the PANEL in the right place */
	    /* to start with, and make sure it's oversized in width. */
	    if (Mflag) {
		mbut_height = xv_get(mbut_panel, XV_HEIGHT);
	    }
	    else {
		mbut_height = 0;
	    }
	    prog_height = xv_get(prog_panel, XV_HEIGHT); 

	file_panel = (Panel)xv_create(frame, PANEL, 
				OPENWIN_SHOW_BORDERS, TRUE,
				WIN_BORDER, TRUE,
				XV_WIDTH, 1100,
				XV_X, 0,
				XV_Y, mbut_height + prog_height, /* bug fix */
				WIN_CONSUME_EVENTS, WIN_ASCII_EVENTS, NULL,
				0);

	/* interpose in front of panel's event handler */

	(void) notify_interpose_event_func(file_panel,
					   my_file_interposer, NOTIFY_SAFE);

	if (file_count < max_scroll_lines)
	  max_scroll_lines = file_count;

	/* add scrolling list */

	file_choice = xv_create(file_panel, PANEL_LIST,
				PANEL_LAYOUT, PANEL_VERTICAL,
				PANEL_READ_ONLY, TRUE,
				PANEL_LIST_DISPLAY_ROWS, max_scroll_lines,
				PANEL_CHOOSE_ONE, TRUE,
				PANEL_CHOOSE_NONE, FALSE,
				PANEL_NOTIFY_PROC, which_file,
				WIN_CONSUME_EVENTS, WIN_ASCII_EVENTS, NULL,
	0);

	/* add items to list */

	for (i=0; file_names[i] != NULL; i++) {
		xv_set(file_choice, 
		       PANEL_LIST_STRING, i, file_names[i],
		       PANEL_LIST_CLIENT_DATA, i, (caddr_t) i,
		0);
	}

	window_fit(file_panel);
  }

  file_height = xv_get(file_panel, XV_HEIGHT); 

  /* finish fitting things and then display frame*/

  frame_width = 0;

  if (Mflag) {
    frame_width = xv_get(mbut_panel, XV_WIDTH); 
  }

  prog_width = xv_get(prog_panel, XV_WIDTH);
  file_width = xv_get(file_panel, XV_WIDTH);

  if (prog_width > frame_width)
    frame_width = prog_width;

  if (file_width > frame_width)
    frame_width = file_width;

  if (Mflag) {
    xv_set(mbut_panel, 
	   XV_X, 0,
	   XV_Y, 0,
	   0);

    xv_set(mbut_panel, XV_WIDTH, frame_width, 0);

    mbut_height = xv_get(mbut_panel, XV_HEIGHT);

    /* XVIew bug; get xview warning/error on monochrome unless subtract 
       1 in the following call; get warning on IBM unless subtract 2! */

    xv_set(prog_panel, XV_Y, mbut_height - 2 , 0);

  }

  else {
    mbut_height = 0;
    xv_set(prog_panel, XV_Y, 0);
    }

  xv_set(prog_panel, XV_WIDTH, frame_width, 0);
  xv_set(prog_panel, XV_X, 0); 
  prog_height = xv_get(prog_panel, XV_HEIGHT); 

  xv_set(file_panel, XV_WIDTH, frame_width, 0);
  xv_set(file_panel, XV_X, 0); 

    /* XVIew bug; get xview warning/error on unless subtract 2 !!*/

  xv_set(file_panel, XV_Y, mbut_height + prog_height - 2 , 0); 

  window_fit(frame); 

/* the following is needed to modify the behavior of the scrolling */
/* panel_list for file selection (if used) to work on mouse-up     */
/* instead of mouse-down */

  if (Scrollflag) {

    /* save Rect of file_panel for notify_interposer */
    g_file_choice_rect = (Rect *) xv_get(file_choice, PANEL_ITEM_RECT);

    /* reduce size of rect to exclude scrollbar, and 10 pixel border */

    /* get the scrollbar */
    file_choice_scrollbar = xv_get(file_choice, PANEL_LIST_SCROLLBAR);

    /* get its Rect */
    sbr = (Rect *) xv_get(file_choice_scrollbar, WIN_RECT);

    /* is scrollbar on left? */
    if (g_file_choice_rect->r_left == sbr->r_left)
	file_left_lim = g_file_choice_rect->r_left + sbr->r_width;

    /* remove scrollbar width from file width */
    file_width_lim = g_file_choice_rect->r_width - sbr->r_width;

    /* items in the panel list have a 10 pixel border */
    /* inside the panel. Chop it off.                 */
    file_left_lim  += 10;
    file_width_lim -= 20;
    file_top_lim = g_file_choice_rect->r_top + 10;
    file_height_lim = g_file_choice_rect->r_height - 20;
  } /* if (Scrollflag) */

/* separating this from the original xv_create seems necessary to 
have some window managers pay attention to positioning */

  if ((x_pos >= 0) && (y_pos >= 0)) {
    xv_set(frame,
	   XV_X, x_pos,
	   XV_Y, y_pos,
	   XV_SHOW, TRUE,
	   0);
  } else
    xv_set(frame, XV_SHOW, TRUE, 0);

  if (frame != (Frame *)NULL) 
    xv_main_loop(frame);

  (void) exit(0);
}

static int
find_files(f_dir, f_regexp, f_names)
char *f_dir;
char *f_regexp;
char ***f_names;
{
  int size = 0;
  DIR *dirfd; 
  struct dirent *direntp; 
  char **file_names = NULL;
#if !defined(hpux) && !defined(OS5)
  char *regexp_err = NULL;
#endif
#if defined(HP700)  || defined(OS5)
  int regexp_err;
  regex_t re;
#endif
#if defined(HP400)
  int regexp_err;
  char *re;
#endif
  char *dirname = (f_dir == NULL ? "." : f_dir);

#if !defined(hpux) && !defined(OS5)
  regexp_err = re_comp(f_regexp);

  if (regexp_err != NULL) {
      Fprintf(stderr, "find_files: bad regexp: %s\n%s\n", 
	      f_regexp, regexp_err);
      return -1;
    }
#endif
#if defined(HP700) || defined(OS5)
  regexp_err = regcomp(&re, f_regexp, 0);
  
  if (regexp_err != 0) {
      Fprintf(stderr, "find_files: bad regular expression: %s\n", f_regexp);
      return -1;
    }
#endif
#if defined(HP400) 
  regexp_err = regcmp(&re, f_regexp, 0);
  
  if (regexp_err != 0) {
      Fprintf(stderr, "find_files: bad regular expression: %s\n", f_regexp);
      return -1;
    }
#endif
  if ((dirfd = opendir(dirname)) == NULL) {
      fprintf(stderr,"find_files: Can't open %s\n",dirname);
      exit(1);
    }

#if !defined(hpux)  && !defined(OS5)
  while ((direntp = readdir(dirfd)) != NULL) {
      if (re_exec(direntp->d_name) && can_read(direntp->d_name)) {
	  addstr(savestring(direntp->d_name), &file_names);
	  size++;
	}
    }
#endif
#if defined(HP700) || defined(OS5)
  while ((direntp = readdir(dirfd)) != NULL) {
      if (!regexec(&re, direntp->d_name, (size_t) 0, NULL, 0) && 
	can_read(direntp->d_name)) {
	  addstr(savestring(direntp->d_name), &file_names);
	  size++;
	}
    }
    regfree(&re);
#endif
#if defined(HP400)
  while ((direntp = readdir(dirfd)) != NULL) {
      if (!regex(&re, direntp->d_name, (size_t) 0, NULL, 0) && 
	can_read(direntp->d_name)) {
	  addstr(savestring(direntp->d_name), &file_names);
	  size++;
	}
    }
  if (re!=NULL)
      free(re);
#endif

  *f_names = file_names;

  closedir(dirfd);

  return(size);

}

static void
run_cmd_on_file(file_name, button)
char *file_name;
Panel_item button;
{
/* This is the function passed to exv_bbox as the but_data_proc. 
 *
 *  Here we want to run (exec) a command (stored in prog_commands) on 
 *  the file name stored in as the buttons data.
 *
 *  We ignore the handle to the button.  
 */

  char command[250];

  sprintf(command, "%s %s", prog_commands[curr_prog_ind], file_name);
  
  if (debug_level > 1) 
    Fprintf(stderr, "%s: Running command:\n %s\n", ProgName, command);

  exec_command(command);
}

static void
select_prog(item, event)
     Panel_item item;		/* parameter panel item */
     Event      *event;
{
  curr_prog_ind = xv_get(item, PANEL_VALUE);
  
  if (debug_level > 2) 
    Fprintf(stderr, "%s: Selected program %s\n", 
	    ProgName, prog_names[curr_prog_ind]);
}

static void
quit_proc(item)
Panel_item item;
{
  Frame frame = (Frame)xv_get(item, PANEL_CLIENT_DATA);

  if (q_command) 
    exec_command(q_command);

  xv_destroy_safe(frame);
}

static int
get_commands(mfile, names, progs)
char *mfile;			/* file with single-level OLWM menu */
char ***names;			/* list of names for program buttons */
char ***progs;			/* corresponding programs to run */
{
  menudata *butmenu;
  menudata *submenu;
  buttondata *butdata;
  int n_buttons = 0;

  butmenu = read_olwm_menu(mfile); 

  if (butmenu == NULL)
    return(-1);

  butdata = butmenu->bfirst; 
  
  while (butdata != NULL) {
      if (butdata->submenu == NULL) {
	  if ((butdata->name != NULL) && (butdata->exec != NULL)) {
	
	      addstr(butdata->name, names);
	      addstr(butdata->exec, progs);
	      
	      if (debug_level > 2) 
		Fprintf(stderr, 
		"%s: added program choice with\n\tlabel: %s\n\tprogram: %s\n", 
		    ProgName, butdata->name, butdata->exec);

	      n_buttons++;
	  }
	  else {
	    Fprintf(stderr, 
	      "%s: skipped button with (nil) name or exec function\n",
		    ProgName);
	  }
	}
      else {	  /* it's a submenu, which isn't allowed here */

	  submenu = (menudata *) butdata->submenu;

	  if (submenu->label != NULL) 
	    Fprintf(stderr, "%s: sub-menus not allowed in %s, skipping %s.\n",
		  ProgName, mfile, submenu->label);
	  else
	    Fprintf(stderr, "%s: skipping sub-men (not allowed).\n", ProgName);
	}
      butdata = butdata->next;
    }
  if (n_buttons > 1) 
    return(1);
  else
    return(-1);
}

int
can_read(file_name)
char *file_name;
{
  FILE *fstrm = fopen(file_name,"r");
  if (fstrm == NULL) {

      return(0);
  }
  else {
      fclose(fstrm);
      return(1);
    }
}


/* notification proc for scrolling file list */
static void
which_file(item, string, client_data, op, event)
Panel_item    item; /* panel list item */
char          *string;
caddr_t       client_data;
Panel_list_op op;
Event         *event;
{

/*  printf("item = %s (%d), op = %d, ie_code = %d\n", 
	 string, client_data, op, event->ie_code); */

  /* only act on select (op == 1), not deselect notification */
  /* from mouse, not keyboard*/
  /* all we do is assign to globals so that my_file_interposer
     can do the right thing on the mouse up event */

  if ( (op == 1) && !event_is_ascii(event) ) {
      last_file_string = string;
      last_file_item = item;
      new_file_item = 1;
  }
}

Notify_value
my_file_interposer(frame, event, arg, type)
Frame             frame;
Event             *event;
Notify_arg        arg;
Notify_event_type type;
{
  short xloc, yloc;

  Notify_value value = notify_next_event_func(frame, event, arg, type);

/* this code is here to try make panel_list items activate on
 * select-up instead of select-down. This ugliness is needed because 
 * the panel_list widget is meant to be a choice instead
 * of a button.
 */

/*  printf("ie_win = %ld, x = %d, y = %d\n", event->ie_win, 
	 event->ie_locx, event->ie_locy);  */

  xloc = event->ie_locx;
  yloc = event->ie_locy;

  /* return if event is outside file_choice panel list? */
  /* surely there MUST be a function or macro for this?? */

/*   switch (event_action(event)) {
	case ACTION_SELECT: printf("- SELECT");
	  break;
	case ACTION_ADJUST: printf("- ADJUST");
	  break;
	case ACTION_MENU: printf("- MENU");
	  break;

        default: printf("- event_action(%d)", event_action(event));
	  break;
  }

  if (event_is_down(event)) printf(" down\n");
*/

 if ( (xloc < file_left_lim) ||
    (xloc > file_left_lim + file_width_lim) ||
    (yloc < file_top_lim) ||
    (yloc > file_top_lim + file_height_lim)) {
      return(value);
  }

  if (event_is_up(event)) {
/*      printf(" up\n");*/
      if (new_file_item) {
	  run_cmd_on_file(last_file_string, last_file_item);
	  new_file_item = 0;
	}
    }


/* send event on to regular event handler */

  return(value);
}
	
static int 
mstrcmp(s1, s2)
char **s1, **s2;
{
  return(strcmp(*s1,*s2));
}

/*
 * reaper - "reap" zombie children
 */
static void *reaper() {
        int status;
#ifndef OS5
	wait3(NULL, WNOHANG, NULL);
#else
	waitpid((pid_t)-1, status, WNOHANG);
#endif
	signal(SIGCHLD, reaper);
}
