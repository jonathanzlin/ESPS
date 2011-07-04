/*
 * This material contains unpublished, proprietary software of
 * Entropic Research Laboratory, Inc. Any reproduction, distribution,
 * or publication of this work must be authorized in writing by Entropic
 * Research Laboratory, Inc., and must bear the notice:
 *
 *    "Copyright (c) 1990-1997 Entropic Research Laboratory, Inc.
 *                   All rights reserved"
 *
 * The copyright notice above does not evidence any actual or intended
 * publication of this source code.
 *
 * Written by:  Derek Lin
 * Checked by:
 * Revised by:
 *
 * Brief description:
 *        This Program converts sampling frequency of FEA_SD data.  Sfconvert
 *        consists of three main modules.  gen_filter() designs the lowpass
 *        filter for interpolation.  gen_index() creates indexing array for
 *        both filter and sampled data.  convert() only computes points
 *        of interests for output by using the arrays created by last
 *        two modules.
 *
 */

static char *sccs_id = "@(#)sfconvert.c	1.14	24 Mar 1997	ERL";

#include <math.h>
#include <esps/esps.h>
#include <esps/constants.h>
#include <esps/fea.h>
#include <esps/feasd.h>
#include <esps/feafilt.h>
#include <esps/window.h>

#define DEBUG 0
#define BUF_SIZ 1000
#define MAX_NO_CHANNELS 512

#define SYNTAX USAGE("sfconvert [-P param_file] [-s new_sample_freq] [-r range][-v deviation] [-c corner_freq] [[-R rej_db][-t trans_band]] [-w sfwin_type -l sfwin_len] [-e channels] [-o output_type] [-f] [-F filt_file] [-d] [-x debug_level] [intput.SD] output.SD")

#define ERROR_EXIT(text) {(void) fprintf(stderr, "%s: %s - exiting\n", \
					 argv[0], text); exit(1);}

/*
 * Global declaration
 */

long *grange_switch();
long get_fea_siz();
short get_fea_type();
int debug_level = 0;
char *arr_alloc();
static double i0();       /* Modified Bessel function, order 0 */

union data                /* pointer for diffrent types of input &   */
{                                /* output data array.                      */
  float *fsr;                    /*   f/d: float or double                  */
  float_cplx *fsc;               /*   s/m: single or multi channels         */
  float **fmr;                   /*   r/c: real or complex                  */
  float_cplx **fmc;
  double *dsr;
  double_cplx *dsc;
  double **dmr;
  double_cplx **dmc;
};

union filt                /* pointer to filter data, float or double */
{
  float *f;
  double *d;
};

/*
 * main program
 */

main(argc, argv)
     int argc;
     char **argv;
{
  char *Version = "1.0";
  char *Date = "9/10/92";
  char *param_file = NULL;           /* parameter file name */
  FILE *isdfile = stdin,             /* input and output file streams */
       *osdfile = stdout;
  char *iname = NULL,                /* input and output file names */
       *oname = NULL,
       *fname = NULL;
  struct header *ihd,                /* input and output file headers */
                *ohd;
  struct feasd *isd_rec,             /* record for input and output data */
               *osd_rec;

  extern int optind;
  extern char *optarg;
  extern char *window_types[];       /* window type code words, window.h */
  extern char *type_codes[];         /* data type code words. */
  int ch;

  int gen_index();                   /* prepares indexes for filtering*/
  long **kernel,                     /* filter kernels */
       *jumpsd;                      /* # of sample to jump for each conv. */
  long dimk[2];                      /* kernel dimension for arr_alloc argum.*/
  long ibuffer_len,                  /* input buffer length */
       obuffer_len,                  /* output buffer length */
       patch = 0;                    /* for some patch work for get_feasd_orecs
					in returning number of data points
					read at the end of file */
  long step1,                        /* 1st step argument for get_feasd_orecs*/
       stepn;                        /* subsequent step in get_feasd_orecs */
  long step;
  long actsize;                      /* # of valid samples read in on buffer */

  void gen_filter();                 /* lowpass design by Kaiser window */
  void write_filter_out();
  short win_type = WT_KAISER;        /* window type */
  float trans = 200;                 /* transistion band in the low pass */
  float db = 60;                     /* number of db down for alias */
  float len_s = 0.5;                 /* filter length in seconds */
  long len;                          /* filter length in samples*/
  union filt filter;
  double cor_freq = 0;               /* corner frequency, defualt is Nyquist*/
  double omega_c;                    /* corner frequency in radian */

  void convert();                    /* core for convolution computation */
  static union data
    obuffer,                         /* output sd buffer */
    ibuffer;                         /* input sd buffer */
  long up = 1;                       /* upsampling factor */
  long down = 1;                     /* downsampling factor */

  long no_samples = 0;               /* number of samples in output file */
  double *istart_time,               /* input and output start_time */
         *ostart_time;
  double *max_val;                   /* maximum value of output data file */
  double samp_rate;                  /* input sampling rate */
  double new_samp_rate = 0;          /* new sampling rate */
  void find_ratio();                 /* finds conversion factors up, down
					from 2 sampling freqs.      */
  float dev = 0;                     /* maximum tolerable deviation from
					new sampling rate in percentage */
  int sflag = 0;                     /* flag for new_samp_rate */
  int rflag = 0;                     /* flag for selecting a range */
  int cflag = 0;                     /* flag for corner frequency */
  int dflag = 0;                     /* flag for double precision compute */
  int oflag = 0;                     /* flag for output data type */
  int vflag = 0;                     /* flag of maximum allowable deviation
					from desired sampling frequency */
  int Rflag = 0;                     /* flag of rejection ratio in db from
					pass to stop band */
  int tflag = 0;                     /* flag of transition bandwith in Hertz */
  int eflag = 0;                     /* flag of channels selection */
  int wflag = 0;                     /* flag of window selection  */
  int lflag = 0;                     /* flag of window length */
  int Kaiserflag = 1;                /* flag for Kaiser window in params file*/
  int fflag = 0;                     /* flag for saving filter coeff */
  long *channels = NULL;             /* array for multichannels */
  void get_range();                  /* get range function */
  char *range=NULL;                  /* string from -r option */
  long s_rec, e_rec;                 /* start and ending position of range*/
  long skip_rec = 0;                 /* number of records to skip */
  long no_samp_rf_left;              /* number of output samples left
					remaining to be written if range
					is specified */
  long num_channels = 0;             /* number of channels */
  int common_file = 0;               /* common processing flag */
  int input_type, output_type;       /* input and output data types */
  int code = 0;                      /* code word for data type and structure
				        default is 'fsr': float,single channels
					and real */
  char *sym;                         /* array to hold data from paramter file*/
  int i, type, size, check = 1;      /* utility variables */

/*
 * process command line options
 */

  while((ch = getopt(argc, argv, "P:s:r:v:c:R:t:w:l:e:o:F:x:df")) != EOF)
    switch (ch) {
    case 'd':
      dflag++;
      break;
    case 'f':
      fflag++;
      break;
    case 'F':
      fname = optarg;
      break;
    case 'P':
      param_file = optarg;
      break;
    case 's':
      new_samp_rate = atof(optarg);
      sflag++;
      break;
    case 'r':
      range = optarg;
      rflag++;
      break;
    case 'v':
      dev = atof(optarg) / 100.0;
      vflag++;
      break;
    case 'c':
      cor_freq = atof(optarg);
      cflag++;
      break;
    case 'R':
      db = atof(optarg);
      Rflag++;
      break;
    case 't':
      trans = atof(optarg);
      tflag++;
      break;
    case 'w':
      win_type = win_type_from_name(optarg);
      wflag++;
      break;
    case 'l':
      len_s = atof(optarg);
      lflag++;
      break;
    case 'e':
      channels = grange_switch(optarg, &num_channels);
      eflag++;
      break;
    case 'o':
      output_type = lin_search(type_codes, optarg);
      oflag++;
      break;
    case 'x':
      debug_level = atoi(optarg);
      break;
    default:
      SYNTAX;
      break;
    }

/*
 * Determine and open input/output file
 */

  if (argc - optind < 2) {
    Fprintf(stderr, "%s: no output file specified.\n",argv[0]);
    SYNTAX;
  }
  iname = eopen(argv[0],argv[argc-2],"rb", FT_FEA, FEA_SD, &ihd, &isdfile);
  oname = eopen(argv[0], argv[argc - 1], "wb", NONE, NONE, &ohd, &osdfile);

  if (debug_level){
    Fprintf(stderr, "%s: output file is %s\n", argv[0], oname);
    Fprintf(stderr, "%s: input file is %s\n", argv[0], iname);
  }

/*
 * Get parameter values
 */

  if (oflag == 1 && output_type == -1)
    ERROR_EXIT(" Unrecognized output data type with -o option.");
  if (wflag == 1 && win_type == WT_NONE)
    ERROR_EXIT(" Unrecognized window type with -w option.");

  (void) read_params(param_file, SC_CHECK_FILE, iname);

  if (!sflag && symtype("new_sample_freq") != ST_UNDEF)
    new_samp_rate = getsym_i("new_sample_freq");
  get_range( &s_rec, &e_rec, range, rflag, 0, ihd);
  if (symtype("start") != ST_UNDEF || symtype("nan") != ST_UNDEF)
    rflag = 1;
  if (!vflag && symtype("deviation") != ST_UNDEF)
    dev = getsym_d("deviation") / 100.0;
  if (!cflag && symtype("corner_freq") != ST_UNDEF)
    cor_freq = getsym_d("corner_freq");
  if (!Rflag && symtype("rej_db") != ST_UNDEF)
    db = getsym_d("rej_db");
  if (!tflag && symtype("trans_band") != ST_UNDEF)
    trans = getsym_d("trans_band");
  if (!wflag && symtype("sfwin_type") != ST_UNDEF)
  {
    sym = getsym_s("sfwin_type");
    win_type = win_type_from_name(sym);
    if (win_type == WT_NONE)
	ERROR_EXIT(" Unrecognized window type in param file.");
  }
  if( !lflag && symtype("sfwin_len") != ST_UNDEF)
    len_s = getsym_d("sfwin_len");
  if( !eflag && symtype("channels") != ST_UNDEF)
  {
    channels = grange_switch(getsym_s("channels"), &num_channels);
  }
  if( !oflag && symtype("output_type") != ST_UNDEF)
  {
    sym = getsym_s("output_type");
    output_type = lin_search(type_codes, sym);
  }
  if( !dflag)
    if( symtype("dflag") != ST_UNDEF)
      if ( getsym_i("dflag") == 1) dflag = 1;
  if ( symtype("Kaiserflag") != ST_UNDEF )
    Kaiserflag = getsym_i("Kaiserflag");

/*
 *  check for inconsistency
 */

  /* up to now, all values of -l/-w/-R/-t are all defined either by command
     line, paramter files, or defaults.  To resolve conflicts of window type,
     win_type == non Kaiser only if -w exists || (sfwin_type exists &&
     Kaiserflag == 0 ).  All other case, win_type = kaiser */

  if( ! (wflag || symtype("sfwin_type") != ST_UNDEF && Kaiserflag == 0 ) )
    win_type = WT_KAISER;

  if (win_type == WT_ARB)
    ERROR_EXIT("Sorry, window type ARB not supported");

  if (win_type != WT_KAISER && len_s <= 0)
    ERROR_EXIT("sfwin_len must be > zero.");
  if ( new_samp_rate < 0 ) ERROR_EXIT("new_sample_freq must >= zero.");
  if ( dev < 0 ) ERROR_EXIT("deviation must be >= zero.");
  if ( cflag && cor_freq < 0 ) ERROR_EXIT("corner_freq must be >= zero.");
  if ( db <= 0 ) ERROR_EXIT("rej_db must be > zero.");
  if ( trans <= 0 ) ERROR_EXIT("trans_band must be > zero.");

/*
 *   determine real/complex, single/multichannel, or float/double
 *   input data kind for computation.  code 0 corresponds to float
 *   single channel, real input data
 */

  if (( input_type = get_fea_type( "samples", ihd )) == UNDEF )
    ERROR_EXIT(" Input file has no field named samples!");
  if ( input_type == FLOAT_CPLX || input_type == DOUBLE_CPLX ||
      input_type == LONG_CPLX || input_type == SHORT_CPLX ||
      input_type == BYTE_CPLX ) code += 1;
  if ( input_type == DOUBLE || input_type == DOUBLE_CPLX || dflag == 1)
  {
    code +=100;
    dflag = 1;
  }
  if ( (size = get_fea_siz("samples", ihd, (short *)NULL,(long **)NULL)) >
      MAX_NO_CHANNELS && num_channels > MAX_NO_CHANNELS )
    ERROR_EXIT(" Input file exceeds 512 maximum allowable channels");
  if ( size > 1 ) code +=10;
  if ( num_channels > size )
    ERROR_EXIT(" Input file has less channels than specified");
  if ( num_channels >= 1)
    if ( channels[num_channels-1] >= size )
      ERROR_EXIT("  Non-existent channel specified");
  if ( num_channels == 0)
  {
    num_channels = size;
    channels = (long *) malloc( (unsigned)num_channels * sizeof(int));
    spsassert(channels,"malloc failed in main");
    for( i= 0; i<num_channels; i++) channels[i] = i;
  }

  if(debug_level)
  {
    if (1 <= code%10) Fprintf(stderr, "\nComplex input data.\n");
    if (1 > code%10 ) Fprintf(stderr, "\nReal input data.\n");
    if (10 > code%100 ) Fprintf(stderr, "Single channel input data.\n");
    if ( code < 100  ) Fprintf(stderr, "Floating point computation.\n\n");
    if ( code >=100  ) Fprintf(stderr, "Double precision computation.\n\n");
    if (10 <= code%100)
    {
      Fprintf(stderr, "Multichannel input data.\n");
      Fprintf(stderr, "Number of output channels: %i\n",num_channels);
    }
  }

/*
 *   find up/down ratio, change new sampling rate accordingly within
 *   deviation level to get the smallest conversion factor
 */

  if( (type = genhd_type("record_freq",&size,ihd)) == HD_UNDEF)
    ERROR_EXIT("record_freq header item is missing from input file");
  samp_rate = *get_genhd_d("record_freq", ihd);
  if( new_samp_rate ==0 ) new_samp_rate = samp_rate;
  find_ratio( &new_samp_rate, samp_rate, dev, &up, &down);
  len = (long) (len_s * samp_rate * MAX(up,down));
  if (win_type != WT_KAISER && len > 5000 && debug_level)
    Fprintf(stderr,"%s: A window of %d data points is used.\n", argv[0], len);
  if (len%2 == 0) len++;              /* make sure odd window length */


/*
 *   Compute low pass filter using Kaiser or other window design method
 */

  if( cor_freq > (samp_rate) / 2){
    Fprintf(stderr, "%s: Corner frequency violates the Nyquist rate.\n",
	    argv[0]);
    exit(1);
  }
  omega_c =( cor_freq !=0 )? 2*PI*cor_freq/(up*samp_rate) :  PI / MAX(up,down);
  (void) gen_filter( dflag, win_type, up, 2*PI*trans/(up*samp_rate),
		    db, omega_c, &len, &filter);
  if(fname){                   /* write out filter file */
    FILE *fpout;
    struct header *oh;
    struct feafilt *frec;
    filtdesparams *fdp=NULL;
    double filter_sf, filter_cut;
    filter_sf = up * samp_rate;
    filter_cut = (cor_freq) ? cor_freq : filter_sf/(2*MAX(up, down));
    fname = eopen(argv[0], fname, "w", NONE, NONE, &oh, &fpout);
    oh = new_header(FT_FEA);
    (void) strcpy (oh->common.prog, argv[0]);
    (void) strcpy (oh->common.vers, Version);
    (void) strcpy (oh->common.progdate, Date);
    oh->common.tag = NO;
    init_feafilt_hd( oh, (long)(len), 0L, fdp);
    add_comment(oh,(char *)get_cmd_line(argc,argv));
    (void) add_genhd_d("samp_freq", &filter_sf, 1, oh);
    (void) add_genhd_d("cutoff_freq", &filter_cut, 1, oh);
    if(win_type == WT_KAISER) (void) add_genhd_f("trans_band", &trans, 1, oh);
    if(win_type == WT_KAISER) (void) add_genhd_f("rej_db", &db, 1, oh);
    (void) add_genhd_d("cutoff_freq", &filter_cut, 1, oh);
    (void) add_genhd_d("cutoff_freq", &filter_cut, 1, oh);
    (void) write_header(oh, fpout);
    frec = allo_feafilt_rec(oh);
    *frec->num_size = len;
    for(i=0;i<len;i++)
      frec->re_num_coeff[i] = (dflag)? filter.d[i]:filter.f[i];
    put_feafilt_rec( frec, oh, fpout);
  }

  if(debug_level)
  {
    Fprintf(stderr,"input sampling rate: %10.2f\n",samp_rate);
    Fprintf(stderr,"new sampling rate: %10.2f\n", new_samp_rate);
    Fprintf(stderr,"conversion factor: up=%d down=%d\n", up, down);
    i = new_samp_rate * dev;
    Fprintf(stderr,"Deviation from new sampling frequency: %10.2f Hz\n",i);
    Fprintf(stderr,"Stopband rejection: %10.2f db\n", db);
    Fprintf(stderr,"Transition Bandwith: %10.2f Hz\n", trans);
    Fprintf(stderr,"Window type: %d (6 for Kaiser) \n", win_type);
    i = new_samp_rate / 2;
    if( cor_freq == 0)
      Fprintf(stderr,"corner frequency: %d Hz\n",i);
    else
      Fprintf(stderr,"corner frequency: %f Hz\n",cor_freq);
    Fprintf(stderr,"filter length: %d\n\n", len);
  }


/*
 *   Generate all needed paramters for convolution: indexing arrays, input/
 *    output buffer length, step in get_feasd_orecs(), filter kernels
 */

  dimk[0] = up;
  dimk[1] = 2;
  kernel = (long **) arr_alloc( 2, dimk, LONG, 0);
  jumpsd = (long *) arr_alloc( 1, &up, LONG, 0);
  if( -1 == gen_index( up, down, len, kernel, jumpsd, &ibuffer_len,
		      &obuffer_len, &step1, &stepn, s_rec, &skip_rec) )
    ERROR_EXIT("Window length by -l is too short, for convolution");
  if(debug_level >= 3)
  {
    Fprintf(stderr,"input data buffer length: %d,output buffer length: %d\n",
	    ibuffer_len, obuffer_len);
    Fprintf(stderr,"step1: %d, stepn: %d\n", step1, stepn);
  }

 /*
  * prepare output header
  */

  ohd = new_header(FT_FEA);
  (void) strcpy (ohd->common.prog, argv[0]);
  (void) strcpy (ohd->common.vers, Version);
  (void) strcpy (ohd->common.progdate, Date);
  ohd->common.tag = NO;
  add_source_file(ohd,iname,ihd);
  add_comment(ohd,(char *)get_cmd_line(argc,argv));

  /* start_time generic header item */

  if( (type = genhd_type("start_time",&size,ihd)) == HD_UNDEF)
      Fprintf(stderr,"%s: start_time in input file is undefIned.\n", argv[0]);
  if( type != DOUBLE )
    Fprintf(stderr, "%s: start_time in input file is not type DOUBLE.\n",
	    argv[0]);
  if( type == DOUBLE && size >1)
  {
    ostart_time = (double *) arr_alloc(1, &num_channels, DOUBLE, 0);
    istart_time = get_genhd_d( "start_time", ihd);
    for( i = 0; i < num_channels; i ++)
      ostart_time[i] = istart_time[channels[i]] + (s_rec-1)/samp_rate;
  }
  else if (type == DOUBLE && size ==1)
  {
    ostart_time = (double *) malloc(sizeof(double));
    spsassert(ostart_time,"malloc failed in main");
    istart_time = get_genhd_d( "start_time", ihd);
    *ostart_time = *istart_time + (s_rec-1)/samp_rate;
  }
  else
    Fprintf(stderr, "%s: start_time is given value zero.\n", argv[0]);

  /* max_val generic header item:
     max_val not stored since true max_value in ouput may differ from true
     max_value of input, although the difference may be small */
  /*if( (type = genhd_type("max_value",&size,ihd)) != HD_UNDEF)
    {
      max_val = get_genhd_d( "max_value", ihd);
      (void) add_genhd_d("max_value", max_val, size, ohd);
    }*/

  /* add sfconvert exclusive paramters and filter coeffiecients */

  (void) add_genhd_d("source_freq", &samp_rate, 1, ohd);
  if ( cflag || symtype("corner_freq") != ST_UNDEF )
      (void) add_genhd_d("corner_freq", &cor_freq, 1, ohd);
  if( win_type == WT_KAISER)
  {
    (void) add_genhd_f("rej_db", &db, 1, ohd);
    (void) add_genhd_f("trans_band", &trans, 1, ohd);
  }
  else
  {
    (void) add_genhd_e("sfwin_type", &win_type, 1, window_types, ohd);
    (void) add_genhd_f("sfwin_len", &len_s, 1, ohd);
  }
  (void) add_genhd_c("source_file", iname, 0, ohd);
  if ( (size = get_fea_siz("samples", ihd, (short *)NULL,(long **)NULL)) > 1 )
    (void) add_genhd_l("channels", channels, num_channels, ohd);
  (void) add_genhd_l("filter_siz", (long *) &len, 1, ohd);
  if( dflag == 1 && fflag == 1 )
    (void) add_genhd_d("filter", filter.d, len, ohd);
  if( dflag == 0 && fflag == 1 )
    (void) add_genhd_f("filter", filter.f, len, ohd);

  /* output data type defaults to input data type */
  if ( oflag==0 && symtype("output_type")==ST_UNDEF) output_type = input_type;

  if(debug_level)
  {
    Fprintf(stderr,"Available data types: \nDOUBLE 1\nFLOAT 2\nLONG 3\n");
    Fprintf(stderr,"SHORT 4\nBYTE 8\nDOUBLE_CPLX 11\nFLOAT_CPLX 12\n");
    Fprintf(stderr,"LONG_CPLX 13\nSHORT_CPLX 14\nBYTE_CPLX 15\n");
    Fprintf(stderr,"Data type for input: %i output: %i\n",
	    input_type, output_type);
  }

/*
 * write header
 */

  init_feasd_hd(ohd, output_type,num_channels,ostart_time ,NO, new_samp_rate);
  (void) write_header ( ohd, osdfile );


/*
 *   allocating input, output sd records with right data type and rank
 */
  switch (code)
  {
  case 0:			/* float single real */
    isd_rec = allo_feasd_recs(ihd, FLOAT, ibuffer_len, (char *) NULL,NO);
    osd_rec = allo_feasd_recs(ohd, FLOAT, obuffer_len, (char*) NULL, NO);
    ibuffer.fsr = (float *) isd_rec->data;
    obuffer.fsr = (float *) osd_rec->data;
    break;
  case 1:			/* float single complex */
    isd_rec =allo_feasd_recs(ihd, FLOAT_CPLX, ibuffer_len, (char *) NULL,NO);
    osd_rec =allo_feasd_recs(ohd, FLOAT_CPLX, obuffer_len, (char*) NULL, NO);
    ibuffer.fsc = (float_cplx *) isd_rec->data;
    obuffer.fsc = (float_cplx *) osd_rec->data;
    break;
  case 10:			/* float multi real */
    isd_rec = allo_feasd_recs(ihd, FLOAT, ibuffer_len, (char*) NULL,YES);
    osd_rec = allo_feasd_recs(ohd, FLOAT, obuffer_len, (char*) NULL,YES);
    ibuffer.fmr = (float **) isd_rec->ptrs;
    obuffer.fmr = (float **) osd_rec->ptrs;
    break;
  case 11:			/* float multi complex */
    isd_rec =allo_feasd_recs(ihd, FLOAT_CPLX, ibuffer_len, (char*) NULL,YES);
    osd_rec =allo_feasd_recs(ohd, FLOAT_CPLX, obuffer_len, (char*) NULL,YES);
    ibuffer.fmc = (float_cplx **) isd_rec->ptrs;
    obuffer.fmc = (float_cplx **) osd_rec->ptrs;
    break;
  case 100:			/* double single real */
    isd_rec = allo_feasd_recs(ihd, DOUBLE, ibuffer_len, (char *) NULL,NO);
    osd_rec = allo_feasd_recs(ohd, DOUBLE, obuffer_len, (char*) NULL, NO);
    ibuffer.dsr = (double *) isd_rec->data;
    obuffer.dsr = (double *) osd_rec->data;
    break;
  case 101:			/* double single complex */
    isd_rec =allo_feasd_recs(ihd, DOUBLE_CPLX, ibuffer_len,(char *) NULL,NO);
    osd_rec =allo_feasd_recs(ohd, DOUBLE_CPLX,obuffer_len, (char*) NULL, NO);
    ibuffer.dsc = (double_cplx *) isd_rec->data;
    obuffer.dsc = (double_cplx *) osd_rec->data;
    break;
  case 110:			/* double multi real */
    isd_rec = allo_feasd_recs(ihd, DOUBLE, ibuffer_len, (char*) NULL,YES);
    osd_rec = allo_feasd_recs(ohd, DOUBLE, obuffer_len, (char*) NULL,YES);
    ibuffer.dmr = (double **) isd_rec->ptrs;
    obuffer.dmr = (double **) osd_rec->ptrs;
    break;
  case 111:			/* double multi complex */
    isd_rec =allo_feasd_recs(ihd, DOUBLE_CPLX,ibuffer_len, (char*) NULL,YES);
    osd_rec =allo_feasd_recs(ohd, DOUBLE_CPLX,obuffer_len, (char*) NULL,YES);
    ibuffer.dmc = (double_cplx **) isd_rec->ptrs;
    obuffer.dmc = (double_cplx **) osd_rec->ptrs;
    break;
  }

  if( isd_rec == NULL || osd_rec == NULL)
    ERROR_EXIT(" Can't allocate space for input/output records.")

/*
 * read and write loop
 */

 if( rflag ) no_samp_rf_left = (e_rec - s_rec ) * up/down +1;

 step = step1;
 (void) fea_skiprec ( isdfile , skip_rec, ihd);
  while(check)
  {
    double  checkVal;
    actsize = get_feasd_orecs( isd_rec, 0L, ibuffer_len,
			     step, ihd, isdfile );

    (void) convert( code, num_channels, channels, &ibuffer, obuffer_len,
		   up, &filter, len, kernel,jumpsd, &obuffer);

    /* determine exactly what output buffer length is */
    /* if range is specified, we keep track of # of output samples written */
    checkVal = (actsize-((len-1.0)/2.0)/up)*up/(1.0*down);
    if((actsize!=ibuffer_len) && (checkVal<=obuffer_len))  {
      obuffer_len = (long) ceil((actsize-patch-((len-1.0)/2.0)/up)
				*up/(down*1.0));
      /* (int) ceil... is to take care of the crazy case like only
	 one data sample is present in input.sd, otherwise, no record
	 is put_feasd_resed due to integer truncation */
      /* (len-1)/2/up is the extra sd pts appended to the beginning */
      if( rflag && no_samp_rf_left - obuffer_len <= 0 )
	obuffer_len = no_samp_rf_left;
      check = 0;
    }    else    {
      if( rflag )
      {
	if( no_samp_rf_left - obuffer_len <= 0 )
	{
	  obuffer_len = no_samp_rf_left;
	  check = 0;
	}
	else
	  no_samp_rf_left -= obuffer_len;
      }
      if( actsize != ibuffer_len) patch += ibuffer_len - actsize;
      /* get_feasd_orecs doesn't return the 'right' number for actual
	 sd points read when last multiple overlapped buffers exceeding
	 the end of file are read, use patch for patch work */
    }

    put_feasd_recs( osd_rec, 0L, obuffer_len, ohd, osdfile );
    if(step == step1) step = stepn;
    no_samples += obuffer_len;
  }

  (void) fclose (isdfile);
  (void) fclose (osdfile);

/*
 * put output file info in ESPS common
 */
   if (strcmp(oname, "<stdout>") != 0) {
     (void)putsym_s("filename", oname);
     (void)putsym_s("prog",argv[0]);
     (void)putsym_i("start",1 );
     (void)putsym_i("nan",(int) no_samples);
   }
   if (debug_level) Fprintf(stderr, "%s: normal exit\n", argv[0]);
   exit(0);
}


void
convert( code, no_chan, chan, buffer, Nout, up, filter, flen, kernel,
	     jumpsd, output)
     int code;                  /* codeword for different data kinds */
     int no_chan;               /* number of channels */
     int *chan;                /* array of channels */
     union data *buffer;         /* a buffer to be processed */
     int Nout;                  /* number of ouput points produced */
     long up;                    /* up and ratio in sfconvert */
     union filt *filter;        /* interpolating filter */
     long flen;                  /* filter length - odd number */
     long **kernel;        /* index matrix for filter kernels */
     long *jumpsd;         /* an array, each element contains the number
				   of jumps in sd points when performing
				   one convolution to the next */
     union data *output;        /* output sd data */
{
  register
    int whkernel = 0;             /* index for which filter kernel to use */
  register
    int whjumpsd = 0;             /* index jumpsd */		
  register
    long sdstart = 0;
  register
    int i,l,m,c;

  /* every output point is produced by convoluting appropriate filter kernel
     and an array of data in 'frame'.  An indexing scheme is used.  Basically
     there are only 'up' many filter kernels.  Which 'kernel' to use is
     determined by 'whkernel'.  Which input sd point to use for 1st point
     in convolution is determined by 'jumpsd'. */

  switch (code)
  {
  case 0:			/* float, single, real */
    {
      register float sum;
      register int end;
      for( m = 0; m < Nout; m++)
      {
	for( sum= 0.0, i = sdstart, l = kernel[whkernel][0],
	    end=kernel[whkernel][1];
	    l <= end; i++,l+=up)
	  sum += buffer->fsr[i] * filter->f[l];
	output->fsr[m]= sum;
	sdstart += jumpsd[whjumpsd];
	if( ++whkernel == up ) whkernel = 0;
	if( ++whjumpsd == up ) whjumpsd = 0;
      }
    }
    break;
  case 1:			/* float, single, complex */
    {
      register float sumr;
      register float sumi;
      register int end;
      for( m = 0; m < Nout; m++)
      {
	for( sumr=0.0, sumi=0.0, i = sdstart, l = kernel[whkernel][0],
	    end = kernel[whkernel][1];
	    l <=end; i++,l+=up)
	{
	  sumr += buffer->fsc[i].real * filter->f[l];
	  sumi += buffer->fsc[i].imag * filter->f[l];
	}	
	output->fsc[m].real = sumr;
	output->fsc[m].imag = sumi;
	sdstart += jumpsd[whjumpsd];
	if( ++whkernel == up ) whkernel = 0;
	if( ++whjumpsd == up ) whjumpsd = 0;
      }
    }
    break;
  case 10:			/* float, multi, real */
    {
      register float sum;
      register int end;
      for( m = 0; m < Nout; m++)
      {
	for (c = 0; c < no_chan; c++)
	{
	  for( sum=0.0, i = sdstart, l = kernel[whkernel][0],
	      end= kernel[whkernel][1];
	      l <= end; i++,l+=up)
	    sum += buffer->fmr[i][chan[c]] * filter->f[l];
	  output->fmr[m][c] = sum;
	}
	sdstart += jumpsd[whjumpsd];
	if( ++whkernel == up ) whkernel = 0;
	if( ++whjumpsd == up ) whjumpsd = 0;
      }
    }
    break;
  case 11:			/* float, multi, complex */
    {
      register float sumr;
      register float sumi;
      register int end;
      for( m = 0; m < Nout; m++)
      {
	for (c = 0; c < no_chan; c++)
	{
	  for( sumr=0.0, sumi=0.0, i = sdstart, l = kernel[whkernel][0],
	      end=kernel[whkernel][1];
	      l <= end; i++,l+=up)
	  {
	    sumr +=buffer->fmc[i][chan[c]].real * filter->f[l];
	    sumi +=buffer->fmc[i][chan[c]].imag * filter->f[l];
	  }
	  output->fmc[m][c].real =sumr;
	  output->fmc[m][c].imag =sumi;
	}
	sdstart += jumpsd[whjumpsd];
	if( ++whkernel == up ) whkernel = 0;
	if( ++whjumpsd == up ) whjumpsd = 0;
      }
    }
    break;
  case 100:			/* double, single, real */
    {
      register double sum;
      register int end;
      for( m = 0; m < Nout; m++)
      {
	for( sum=0.0, i = sdstart, l = kernel[whkernel][0],
	    end = kernel[whkernel][1];
	    l <=end; i++,l+=up)
	  sum += buffer->dsr[i] * filter->d[l];
	output->dsr[m] = sum;
	sdstart += jumpsd[whjumpsd];
	if( ++whkernel == up ) whkernel = 0;
	if( ++whjumpsd == up ) whjumpsd = 0;
      }
    }
    break;
  case 101:			/* double, single, complex */
    {
      register double sumr;
      register double sumi;
      register int end;
      for( m = 0; m < Nout; m++)
      {
	for( sumr=0.0, sumi=0.0, i = sdstart, l = kernel[whkernel][0],
	    end = kernel[whkernel][1];
	    l<=end; i++,l+=up)
	{
	  sumr += buffer->dsc[i].real * filter->d[l];
	  sumi += buffer->dsc[i].imag * filter->d[l];
	}	
	output->dsc[m].real = sumr;
	output->dsc[m].imag = sumi;
	sdstart += jumpsd[whjumpsd];
	if( ++whkernel == up ) whkernel = 0;
	if( ++whjumpsd == up ) whjumpsd = 0;
      }
    }
    break;
  case 110:			/* double, multi, real */
    {
      register double sum;
      register int end;
      for( m = 0; m < Nout; m++)
      {
	for (c = 0; c < no_chan; c++)
	{
	  for( sum=0.0, i = sdstart, l = kernel[whkernel][0],
	      end=kernel[whkernel][1];
	      l <= end; i++,l+=up)
	    sum += buffer->dmr[i][chan[c]] * filter->d[l];
	  output->dmr[m][chan[c]] = sum;
	}
	sdstart += jumpsd[whjumpsd];
	if( ++whkernel == up ) whkernel = 0;
	if( ++whjumpsd == up ) whjumpsd = 0;
      }
    }
    break;
  case 111:			/* double, multi, complex */
    {
      register double sumr;
      register double sumi;
      register int end;
      for( m = 0; m < Nout; m++)
      {
	for (c = 0; c < no_chan; c++)
	{
	  for( sumr=0.0, sumi=0.0, i = sdstart, l = kernel[whkernel][0],
	      end = kernel[whkernel][1];
	      l <= end; i++,l+=up)
	  {
	    sumr +=buffer->dmc[i][chan[c]].real * filter->d[l];
	    sumi +=buffer->dmc[i][chan[c]].imag * filter->d[l];
	  }
	  output->dmc[m][chan[c]].real = sumr;
	  output->dmc[m][chan[c]].imag=sumi;
	}
	sdstart += jumpsd[whjumpsd];
	if( ++whkernel == up ) whkernel = 0;
	if( ++whjumpsd == up ) whjumpsd = 0;
      }
    }
    break;
  }
}

/* generate index arrays used for convert.c to compute only the needed
   points */

int
gen_index( up, down, len,  kernel, jumpsd, ibuffer_len,
	       obuffer_len, step1, stepn, s_rec, skip_rec)
     long up, down;          /* up/down conversion ratio */
     long len;               /* length of lowpass filter, always odd! */
     long **kernel;          /* pointer to 2-d array of up x 2, each row
				has starting and end point index of lowpass
				filter for convolution of the filter and
				sampled data points.  There are only up filter
				kernels */
     long *jumpsd;           /* pointer to an array of increments in sampled
			       data for next starting sampled data point
			       to use for convlution */
     long *ibuffer_len;      /* input/output sd buffer length */
     long *obuffer_len;
     long *step1, *stepn;    /* step argument in get_feasd_orecs(), step1
			       for first buffer, step2 for all subsequent
			       buffers */
     long s_rec;         /* starting position of sampled data file */
     long *skip_rec;     /* argument for skiprec() */
{
  long hlen;                 /* half of filter length */

  long x, y, uphalf;
  long xl,yl;
  int i;
  int retry = 0;
  s_rec -= 1;                 /* index starts at 0 */

  hlen = ( len -1 ) / 2;

  x = hlen % up;
  for ( i = 0; i <up; i++ )
  {
    if (hlen-(i*down)%up < 0) return(-1);
    kernel[i][0]= ( hlen - ( i*down )%up ) % up;
    kernel[i][1]=(len - 1) - (hlen + ( i * down )%up ) % up;

    y = (i+1) * down + (hlen - ((i+1) * down)%up ) % up;
    jumpsd[i] = (int) (y -x)/up;
    x = y;
  }

  /* BUF_SIZ is the most effecient size for computation, use this size
     if you can. */
  i = 1;
  while(i)
  {
    yl = BUF_SIZ * i;
    xl = (down > up) ? 1 + yl / down : 1 + yl/ up;
    *ibuffer_len = xl * down;	/* the closest number it can get to BUF_SIZ*i*/
    *obuffer_len = xl * up;
    if( down+2*len/up > *ibuffer_len )
    {
      i++;
      retry = 1;
    }
    else
      i = 0;
  }

  if(retry && debug_level>0)
  {
    Fprintf(stderr, "Large data buffer is allocated ");
    Fprintf(stderr, "You may run out of memory.\n");
    Fprintf(stderr, "In that case, decrease filter rejection ratio,\n");
    Fprintf(stderr, "increase transition bandwidth, decrease window \n");
    Fprintf(stderr, "length, or increase maximum frequency deviation.\n");
  }
  /* in one cycle, need down # input data to produce up # output data */

  *ibuffer_len += hlen / up;    /* add some points to take care of beginning */
  x = (up - 1) * down;                /* # sd travelled for center of filter
					 so far in one cycle */
  x += kernel[up-1][1] - hlen;        /* # sd on its right + last x, now
					 x is # sd to the end of filter */
  y = ( x%up == 0) ? 1 + x/up : x/up; /* # input data needed for one cycle */
  uphalf = (y - down>0) ? y-down : 0; /* this is how many to take care end */
                                      /* uphalf = 0 in crazy case like
					 of extremely short filter, its
					 length < down */
  *ibuffer_len += uphalf;       /* add some point to take care the end */

  *step1 = *ibuffer_len -  hlen / up;  /* good for s_rec==0 & ==hlen/up */
  if ( s_rec !=0 && s_rec - hlen/up > 0)
  {
    *skip_rec = s_rec - hlen/up;
    *step1 = *ibuffer_len;
  }
  if ( s_rec !=0 && s_rec - hlen/up < 0) *step1 +=  s_rec;
  *stepn = *ibuffer_len - uphalf -  (hlen / up);
          /* (ibuffer_len-uphalf) makes next buffer starts in zeroth position*/
          /* (heln/up) for take care beginning */
  return 0;
}

  	

/* generates lowpass filter by various method.
   win_type determines which method to use.  If WT_KAISER method is used,
   del_omega, A, and omegac are required paramters in computing, M and filter
   are output.  If other window method are used, the only required paramter
   is M, output is filter.
   For Kaiser windo method, see Discrete Time Signal Processing by
   Oppenheim and Schafer.  See window() (3-ESPSsp) for more information
   on use of other windowing methods */

void
gen_filter( dflag, win_type, up, del_omega, A, omegac, M, filter)
     int dflag;             /* flag for double operation */
     long up;                /* up conversion factor */
     double del_omega;      /* transition band in radian */
     float A;               /* stopband in db*/
     double omegac;         /* cutoff frequency in radian */
                            /* if default case, omegac is PI/MAX(up,down) */
     short win_type;
     long *M;               /* length of filter: to be determined by kaiser */
     union filt *filter;     /* the resulting filter */
{
  double alpha, beta;
  int i;
  double sinc0;             /* sinc(0) */
  double y;
  double *sincd,             /* double sinc function */
         *kparam;           /* double Kaiser parameters */
  float *sincf;
  double gain;

  /* generate Kaiser parameters */
  if( win_type == WT_KAISER )                /* beta, length generation */
  {
    if ( A > 50 )
      beta = 0.1102 * ( A - 8.7);
    else if ( A < 21 )
      beta = 0;
    else
      beta = 0.5842 * pow((A - 21), 0.4) + 0.07886 * (A - 21);
    y = ceil((A - 8)/(2.285 * del_omega));

    *M = ( (int) y  % 2  == 0 ) ? y+1 : y; /* make sure length is odd */
  }

  alpha = (*M -1 ) / 2;                    /* make sure type I filter */
  sinc0 = omegac / PI;

  gain = up;

  /* generate sinc function */
  if( dflag)
  {
    sincd = (double *) malloc( *M * sizeof(double));
    spsassert(sincd,"malloc failed in gen_filter for sinc");
    for (i = 0; i < *M; i++)
    {
      if ( i - alpha == 0 )
	sincd[i] = sinc0 * gain;
      else
	sincd[i] =  gain * sin( (i - alpha)* omegac )/(PI * (i-alpha));
    }
  }
  else
  {
    sincf = (float *) malloc( *M * sizeof(float));
    spsassert(sincf,"malloc failed in gen_filter for sinc");
    for (i = 0; i < *M; i++)
    {
      if ( i - alpha == 0 )
	sincf[i] = sinc0 * gain;
      else
	sincf[i] =  gain * sin( (i - alpha)* omegac )/(PI * (i-alpha));
    }
  }

  /* call float or double routine for windowing */
  if(dflag)
  {
    filter->d = (double *) malloc( *M * sizeof(double));
    spsassert(filter->d,"malloc failed in gen_filter for filter");
    windowd( *M, sincd, filter->d, win_type, &beta);
  }
  else
  {
    filter->f = (float *) malloc( *M * sizeof(float));
    spsassert(filter->f,"malloc failed in gen_fitler for filter");
    window( *M, sincf, filter->f, win_type, &beta);
  }

}


/* Modified Bessel function, from Kaiser(1974) */
/* there is a fast version of it in Numerical Recepi for C, but it is
   coyrighted */

static double
i0( x )
     register double x;
{
  register double d, ds, s;

  ds = 1;
  d = 2;
  s = ds;
  ds = (x*x)/4.0;
  while ( ds >= .2e-8*s )
  {
    d += 2;
    s += ds;
    ds *= (x*x) / (d*d);
  }
  return s;
}



void
find_ratio(new_samp_rate, samp_rate, dev, upi, downi)
     double *new_samp_rate;
     double samp_rate;
     int *upi, *downi;
     float dev;
{
  int Nfactor = 50;
  int done, j, i;
  long up, down;
  long r, s, t;

  up =  *new_samp_rate * 100;
  down = samp_rate * 100;

  /* Factor out the g.c.d. of up and down */

  s = MAX(up, down);
  t = MIN(up, down);
  while (t != 0)
  {
    r = s%t;
    s = t;
    t = r;
  }
  up /= s;
  down /= s;

  dev = up*dev;

  done = (up < 10 && down < 10);
  for (i = 1; i<=Nfactor && up>=i && !done; i++)
  {
    for(j = 1; j<=Nfactor && down>=j && !done; j++)
    {
      if (fabs((double) down * i - (double) up * j) <= (double) dev * j)
      {
	up = i;
	down = j;
	done = 1;
      }
    }
  }
  *upi = up;
  *downi = down;
  *new_samp_rate = (samp_rate * up) / down;
}


void
get_range(srec, erec, rng, pflag, Sflag, hd)
/*
 * This function facilitates ESPS range processing.  It sets srec and erec
 * to their parameter/common values unless a range option has been used, in
 * which case it uses the range specification to set srec and erec.  If
 * there is no range option and if start and nan do not appear in the
 * parameter/common file, then srec and erec are set to 1 and LONG_MAX.
 * Get_range assumes that read_params has been called; If a command-line
 * range option (e.g., -r range) has been used, get_range should be called
 * with positive pflag and with rng equal to the range specification.
 */
long *srec;                     /* starting record */
long *erec;                     /* end record */
char *rng;                      /* range string from range option */
int pflag;                      /* flag for whether -r or -p used */
int Sflag;                      /* flag for whether -S used */
struct header *hd;              /* input file header */
{
    long common_nan;

    *srec = 1;
    *erec = LONG_MAX;
    if (pflag)
        lrange_switch (rng, srec, erec, 1);
    else if (Sflag)
        trange_switch (rng, hd, srec, erec);
    else {
        if(symtype("start") != ST_UNDEF) {
            *srec = getsym_i("start");
        }
        if(symtype("nan") != ST_UNDEF) {
            common_nan = getsym_i("nan");
            if (common_nan != 0)
                *erec = *srec + common_nan - 1;
        }
    }
}