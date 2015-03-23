/* Routines for decimation */

#ifndef DECIMATE_H
#define DECIMATE_H 1

#include <libmseed.h>

#ifdef __cplusplus
extern "C" {
#endif

int ddecimate (double *data, int npts, int factor,
	       double *fir, int firc, int firsym);
  
int fdecimate (float *data, int npts, int factor,
	       double *fir, int firc, int firsym);

int idecimate (int32_t *data, int npts, int factor,
	       double *fir, int firc, int firsym);

#ifdef __cplusplus
}
#endif

#endif /* DECIMATE_H */
