/*
 * This material contains proprietary software of Entropic Speech, Inc.
 * Any reproduction, distribution, or publication without the prior
 * written permission of Entropic Speech, Inc. is strictly prohibited.
 * Any public distribution of copies of this work authorized in writing by
 * Entropic Speech, Inc. must bear the notice
 *
 *     "Copyright (c) 1987 Entropic Speech, Inc. All rights reserved."
 *
 *
 * non-stat - process non_stat_output for fea_stats(1-ESPS)
 *
 * Author: Ajaipal S. Virdy, Entropic Speech, Inc.
 *
 */

#ifndef lint
    static char *sccsid = "@(#)non_stat.c	1.11	7/8/96	ESI";
#endif

#include <stdio.h>
#include <esps/esps.h>
#include <esps/unix.h>
#include <esps/fea.h>
#include <esps/feastat.h>
#include "global.h"

/*
 *  G L O B A L
 *   V A R I A B L E S
 *    R E F E R E N C E D:
 *
 *	long	n_rec;
 *	int	debug_level;
 *	char	*ProgName;
 *	long	s_rec;
 *	struct
 *	fea_data	*fea_rec;
 *	struct
 *	header		*esps_hdr;
 *	FILE		*instrm;
 *	int	nstat;
 *	int	*stat_field;
 *
 *	int	ndouble;
 *	int	nfloat;
 *	int	nlong;
 *	int	nshort;
 *	int	nchar;
 *
 *	int	Rflag;
 *	int	iflag;
 *	long	*irange;
 *	long	nitem;
 *	long	**item_arrays;
 *	int	*n_items;
 *	long	*max_elems;
 *	int	Cflag;
 *	int	Iflag;
 *	float	**Data;
 */

struct d_struct {
	double	min;
	int	min_at;
	double	max;
	int	max_at;
	double	stndev;
	double	mean;
} *d_info;

struct f_struct {
	float	min;
	int	min_at;
	float	max;
	int	max_at;
	double	stndev;
	double	mean;
} *f_info;

struct l_struct {
	long	min;
	int	min_at;
	long	max;
	int	max_at;
	double	stndev;
	double	mean;
} *l_info;

struct s_struct {
	short	min;
	int	min_at;
	short	max;
	int	max_at;
	double	stndev;
	double	mean;
} *s_info;

struct b_struct {
	char	min;
	int	min_at;
	char	max;
	int	max_at;
	double	stndev;
	double	mean;
} *b_info;

char *ecalloc();
/* SDI 26/6/06 added LINUX_OR_MAC to avoid compiler error */
#if !defined(DEC_ALPHA) && !defined(HP700) && !defined(LINUX_OR_MAC)
char  *calloc(), *realloc();
#endif

#define SUCCINCT(type, info, index, value) \
	{ \
	    if (Cflag || Iflag) \
		Data[i_rec][k] = (float) value; \
 \
	    info[index].mean += value; \
	    info[index].stndev += (value * value); \
 \
 	    if (i_rec == 0) { \
		if (!Rflag && (k != 0)) { \
		   if (value < info[index].min) { \
			info[index].min = (type) value; \
			info[index].min_at = s_rec + i_rec; \
		   } \
		   if (value > info[index].max) { \
			info[index].max = (type) value; \
			info[index].max_at = s_rec + i_rec; \
		   } \
		} \
		else { \
		   info[index].min = (type) value; \
		   info[index].max = (type) value; \
		   info[index].min_at = s_rec + i_rec; \
		   info[index].max_at = s_rec + i_rec; \
		} \
	    } else { \
		if (value < info[index].min) { \
		   info[index].min = (type) value; \
		   info[index].min_at = s_rec + i_rec; \
		} \
		if (value > info[index].max) { \
		   info[index].max = (type) value; \
		   info[index].max_at = s_rec + i_rec; \
		} \
	    } \
 \
	}


void Allo_Memory();

void
Process_non_stat_output()
{

    long    i_rec;

    int	    d_index;
    int	    f_index;
    int	    l_index;
    int	    s_index;
    int	    b_index;

    int	    i;
    int	    k;
    int	    ptr;

    double  *d_ptr;
    float   *f_ptr;
    long    *l_ptr;
    short   *s_ptr;
    char    *b_ptr;

    double  value;
    float   f_value;
    long    l_value;
    short   s_value;
    char    b_value;

    char    *Module = "non_stat_out";


    Allo_Memory ();

    for (i_rec = 0; i_rec < n_rec; i_rec++) {

	if (debug_level > 8)
	    Fprintf (stderr,
		"\t%s: getting record number %d.\n", Module, s_rec + i_rec);

	if (get_fea_rec(fea_rec, esps_hdr, instrm) == EOF) {
	    Fprintf (stderr,
		"%s: ran out of data after record number %d.\n",
		Module, s_rec + i_rec);
	    exit (1);
	}

	/* Initialize indices */

	d_index = 0;
	f_index = 0;
	l_index = 0;
	s_index = 0;
	b_index = 0;

	for (i = 0; i < nstat; i++) {

	    ptr = stat_field[i];

	    if (debug_level > 8)
		Fprintf (stderr,
		    "\t%s: size of field %s is %d.\n",
		    Module, fea_hdr->names[ptr], fea_hdr->sizes[ptr]);

	    /* Position the pointers to the correct places. */

	    switch (fea_hdr->types[ptr])
	    {
	    case DOUBLE:
		d_ptr = (double *) get_fea_ptr(fea_rec,
			    fea_hdr->names[ptr], esps_hdr);
		break;
	    case FLOAT:
		f_ptr = (float *) get_fea_ptr(fea_rec,
			    fea_hdr->names[ptr], esps_hdr);
		break;
	    case LONG:
		l_ptr = (long *) get_fea_ptr(fea_rec,
			    fea_hdr->names[ptr], esps_hdr);
		break;
	    case SHORT:
		s_ptr = (short *) get_fea_ptr(fea_rec,
			    fea_hdr->names[ptr], esps_hdr);
		break;
	    case BYTE:
		b_ptr = (char *) get_fea_ptr(fea_rec,
			    fea_hdr->names[ptr], esps_hdr);
		break;
	    default:
		Fprintf (stderr, "%s: incorrect data type.\n", Module);
	    }

	    if ((fea_hdr->types[ptr] == DOUBLE) && ndouble)
	    {
		for (k = 0; k < n_items[i]; k++) {
		    value = d_ptr[item_arrays[i][k]];

		    SUCCINCT(double, d_info, d_index, value);

		    if (debug_level > 12) {
			Fprintf (stderr,
			"\n\trecord #%d, k = %d, d_index = %d, value = %g\n",
			s_rec + i_rec, k, d_index, value);
			Fprintf (stderr,
			"\td_info.min = %g, d_info.max = %g\n",
			d_info[d_index].min, d_info[d_index].max);
			Fprintf (stderr,
			"\td_info.mean = %g, d_info.stndev = %g\n",
			d_info[d_index].mean, d_info[d_index].stndev);
		    }
		    if (Rflag) d_index++;
		}
		if (!Rflag) d_index++;
	    }

	    if ((fea_hdr->types[ptr] == FLOAT) && nfloat)
	    {
		for (k = 0; k < n_items[i]; k++) {
		    f_value = f_ptr[item_arrays[i][k]];

		    SUCCINCT (float, f_info, f_index, f_value);

		    if (debug_level > 12) {
			Fprintf (stderr,
			"\n\trecord #%d, k = %d, f_index = %d, f_value = %g\n",
			s_rec + i_rec, k, f_index, f_value);
			Fprintf (stderr,
			"\tf_info.min = %g, f_info.max = %g\n",
			f_info[f_index].min, f_info[f_index].max);
			Fprintf (stderr,
			"\tf_info.mean = %g, f_info.stndev = %g\n",
			f_info[f_index].mean, f_info[f_index].stndev);
		    }
		    if (Rflag) f_index++;
		}
		if (!Rflag) f_index++;
	    }

	    if ((fea_hdr->types[ptr] == LONG) && nlong)
	    {
		for (k = 0; k < n_items[i]; k++) {
		    l_value = l_ptr[item_arrays[i][k]];
		    value = (double) l_value;

		    SUCCINCT (long, l_info, l_index, value);

		    if (debug_level > 12) {
			Fprintf (stderr,
			"\n\trecord #%d, k = %d, l_index = %d, l_value = %ld\n",
			s_rec + i_rec, k, l_index, l_value);
			Fprintf (stderr,
			"\tl_info.min = %ld, l_info.max = %ld\n",
			l_info[l_index].min, l_info[l_index].max);
			Fprintf (stderr,
			"\tl_info.mean = %g, l_info.stndev = %g\n",
			l_info[l_index].mean, l_info[l_index].stndev);
		    }
		    if (Rflag) l_index++;
		}
		if (!Rflag) l_index++;
	    }

	    if ((fea_hdr->types[ptr] == SHORT) && nshort)
	    {
		for (k = 0; k < n_items[i]; k++) {
		    s_value = s_ptr[item_arrays[i][k]];
		    value = (double) s_value;

		    SUCCINCT (short, s_info, s_index, value);

		    if (debug_level > 12) {
			Fprintf (stderr,
			"\n\trecord #%d, k = %d, s_index = %d, s_value = %d\n",
			s_rec + i_rec, k, s_index, s_value);
			Fprintf (stderr,
			"\ts_info.min = %d, s_info.max = %d\n",
			s_info[s_index].min, s_info[s_index].max);
			Fprintf (stderr,
			"\ts_info.mean = %g, s_info.stndev = %g\n",
			s_info[s_index].mean, s_info[s_index].stndev);
		    }
		    if (Rflag) s_index++;
		}
		if (!Rflag) s_index++;
	    }

	    if ((fea_hdr->types[ptr] == BYTE) && nchar)
	    {
		for (k = 0; k < n_items[i]; k++) {
		    b_value = b_ptr[item_arrays[i][k]];
		    value = (double) b_value;

		    SUCCINCT (char, b_info, b_index, value);

		    if (debug_level > 12) {
			Fprintf (stderr,
			"\n\trecord #%d, k = %d, b_index = %d, b_value = %d\n",
			s_rec + i_rec, k, b_index, b_value);
			Fprintf (stderr,
			"\tb_info.min = %d, b_info.max = %d\n",
			b_info[b_index].min, b_info[b_index].max);
			Fprintf (stderr,
			"\tb_info.mean = %g, b_info.stndev = %g\n",
			b_info[b_index].mean, b_info[b_index].stndev);
		    }
		    if (Rflag) b_index++;
		}
		if (!Rflag) b_index++;
	   }

	}  /* end for (i = 0; i < nstat; i++) */

    }  /* end for (i_rec = 0; i_rec < n_rec; i_rec++) */

}


void
Allo_Memory ()
{

    /*
     *   G L O B A L    V A R I A B L E S
     *
     *         R E F E R E N C E D
     *
     *   char	*ProgName;
     *   int	debug_level;
     *   int	ndouble, nfloat, nlong, nshort, nchar;
     */

    int	    i;

    if ((d_info =
	    (struct d_struct *) ecalloc ((unsigned) ndouble, sizeof(struct d_struct)))
	 == NULL) {
       Fprintf (stderr,
       "%s: calloc: could not allocate memory for d_info.\n", ProgName);
       exit (1);
    }

    for (i = 0; i < ndouble; i++)
    {
	d_info[i].mean = 0.0;
	d_info[i].stndev = 0.0;
    }

    if ((f_info =
	    (struct f_struct *) ecalloc ((unsigned) nfloat, sizeof(struct f_struct)))
	 == NULL) {
       Fprintf (stderr,
       "%s: calloc: could not allocate memory for f_info.\n", ProgName);
       exit (1);
    }

    for (i = 0; i < nfloat; i++)
    {
	f_info[i].mean = 0.0;
	f_info[i].stndev = 0.0;
    }

    if ((l_info =
	    (struct l_struct *) ecalloc ((unsigned) nlong, sizeof(struct l_struct)))
	 == NULL) {
       Fprintf (stderr,
       "%s: calloc: could not allocate memory for l_info.\n", ProgName);
       exit (1);
    }

    for (i = 0; i < nlong; i++)
    {
	l_info[i].mean = 0.0;
	l_info[i].stndev = 0.0;
    }

    if ((s_info =
	    (struct s_struct *) ecalloc ((unsigned) nshort, sizeof(struct s_struct)))
	 == NULL) {
       Fprintf (stderr,
       "%s: calloc: could not allocate memory for s_info.\n", ProgName);
       exit (1);
    }

    for (i = 0; i < nshort; i++)
    {
	s_info[i].mean = 0.0;
	s_info[i].stndev = 0.0;
    }

    if ((b_info =
	    (struct b_struct *) ecalloc ((unsigned) nchar, sizeof(struct b_struct)))
	 == NULL) {
       Fprintf (stderr,
       "%s: calloc: could not allocate memory for b_info.\n", ProgName);
       exit (1);
    }

    for (i = 0; i < nchar; i++)
    {
	b_info[i].mean = 0.0;
	b_info[i].stndev = 0.0;
    }
}
