/*********************************************************************
 * decimate.c
 *
 * Time-series downsampling: decimation with anti-alias filtering.
 *
 * The anti-alias FIR filters are all symmetrical (and linear phase)
 * and are the default filters used in SAC.
 *
 * Three versions: for double, float and 32-bit integer samples.
 *
 * Modified: 2011.031
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "decimate.h"

/* A 2-factor decimation AA FIR filter */
static int dec2FIRnc = 48;
static double dec2FIR[48]= {                                                          \
  0.47312629E+00,  0.31697387E+00,  0.26678406E-01, -0.10212776E+00, -0.26100356E-01, \
  0.57145566E-01,  0.25159817E-01, -0.36581896E-01, -0.23883324E-01,  0.24321463E-01, \
  0.22318326E-01, -0.16004425E-01, -0.20517055E-01,  0.99688675E-02,  0.18537275E-01, \
 -0.54459628E-02, -0.16433518E-01,  0.20369552E-02,  0.14277564E-01,  0.50962088E-03, \
 -0.12123583E-01, -0.23524212E-02,  0.10030696E-01,  0.36127516E-02, -0.80487542E-02, \
 -0.43881042E-02,  0.62203603E-02,  0.47640391E-02, -0.45795627E-02, -0.48192143E-02, \
  0.31499979E-02,  0.46267062E-02, -0.19449759E-02, -0.42532040E-02,  0.96769247E-03, \
  0.37597087E-02, -0.21371132E-03, -0.32004546E-02, -0.32992219E-03,  0.26274207E-02, \
  0.67918003E-03, -0.20938036E-02, -0.85202418E-03,  0.16962707E-02,  0.87003352E-03, \
 -0.19099547E-02, -0.33559431E-02, -0.33131917E-03                                    \
};

/* A 3-factor decimation AA FIR filter */
static int dec3FIRnc = 71;
static double dec3FIR[71]= {                                                          \
  0.31535816E+00,  0.26616585E+00,  0.14575759E+00,  0.17844718E-01, -0.57989098E-01, \
 -0.61381415E-01, -0.17455403E-01,  0.27205415E-01,  0.38973324E-01,  0.16825307E-01, \
 -0.14348116E-01, -0.27822647E-01, -0.15969098E-01,  0.71774079E-02,  0.20735700E-01, \
  0.14920589E-01, -0.26327996E-02, -0.15616039E-01, -0.13714714E-01, -0.41046151E-03, \
  0.11650560E-01,  0.12385793E-01,  0.24693119E-02, -0.84637869E-02, -0.10977356E-01, \
 -0.38227537E-02,  0.58662295E-02,  0.95316246E-02,  0.46434188E-02, -0.37460821E-02, \
 -0.80886949E-02, -0.50521158E-02,  0.20347144E-02,  0.66878777E-02,  0.51394254E-02, \
 -0.68287796E-03, -0.53611984E-02, -0.49797446E-02, -0.35139307E-03,  0.41367821E-02, \
  0.46373457E-02,  0.11029486E-02, -0.30388678E-02, -0.41676573E-02, -0.16067328E-02, \
  0.20830208E-02,  0.36205212E-02,  0.18995546E-02, -0.12775299E-02, -0.30384576E-02, \
 -0.20179669E-02,  0.62483293E-03,  0.24575219E-02,  0.19959896E-02, -0.12281968E-03, \
 -0.19090844E-02, -0.18706999E-02, -0.23879687E-03,  0.14155055E-02,  0.16748102E-02, \
  0.47103246E-03, -0.10020733E-02, -0.14473109E-02, -0.58572053E-03,  0.71034848E-03, \
  0.12577202E-02,  0.60853921E-03, -0.79453795E-03, -0.15070150E-02, -0.27735722E-02, \
 -0.57726190E-03                                                                      \
};

/* A 4-factor decimation AA FIR filter */
static int dec4FIRnc = 91;
static double dec4FIR[91]= {                                                          \
  0.23653504E+00,  0.21532360E+00,  0.15848082E+00,  0.83793312E-01,  0.13365105E-01, \
 -0.34419119E-01, -0.51046878E-01, -0.39812922E-01, -0.13070323E-01,  0.13739644E-01, \
  0.28548554E-01,  0.26923735E-01,  0.12592813E-01, -0.55519193E-02, -0.18260375E-01, \
 -0.20213991E-01, -0.11949001E-01,  0.11730746E-02,  0.12122922E-01,  0.15773855E-01, \
  0.11154728E-01,  0.14588218E-02, -0.79624243E-02, -0.12446117E-01, -0.10243686E-01, \
 -0.30995123E-02,  0.49446635E-02,  0.97706541E-02,  0.92410743E-02,  0.40912377E-02, \
 -0.26885252E-02, -0.75423168E-02, -0.81806388E-02, -0.46254322E-02,  0.98837493E-03, \
  0.56574643E-02,  0.70931828E-02,  0.48204092E-02,  0.27697056E-03, -0.40607406E-02, \
 -0.60094716E-02, -0.47603725E-02, -0.11885283E-02,  0.27206815E-02,  0.49573286E-02, \
  0.45121713E-02,  0.18078703E-02, -0.16159373E-02, -0.39624022E-02, -0.41275406E-02, \
 -0.21834560E-02,  0.72517502E-03,  0.30510179E-02,  0.36549442E-02,  0.23592040E-02, \
 -0.36519799E-04, -0.22335923E-02, -0.31325626E-02, -0.23753699E-02, -0.47197170E-03, \
  0.15254335E-02,  0.25965930E-02,  0.22699400E-02,  0.81899471E-03, -0.93285192E-03, \
 -0.20744186E-02, -0.20755990E-02, -0.10270840E-02,  0.45596727E-03,  0.15875453E-02, \
  0.18238572E-02,  0.11186297E-02, -0.93354625E-04, -0.11566642E-02, -0.15448434E-02, \
 -0.11173668E-02, -0.15881853E-03,  0.79965324E-03,  0.12694919E-02,  0.10489000E-02, \
  0.30336727E-03, -0.54487761E-03, -0.10457698E-02, -0.94576413E-03, -0.29340666E-03, \
  0.60871453E-03,  0.13765346E-02,  0.17459453E-02,  0.17342302E-02,  0.17703171E-02, \
 -0.78610331E-03                                                                      \
};

/* A 5-factor decimation AA FIR filter */
static int dec5FIRnc = 111;
static double dec5FIR[111]= {                                                         \
  0.18922436E+00,  0.17825589E+00,  0.14762825E+00,  0.10361496E+00,  0.54943368E-01, \
  0.10695269E-01, -0.21801252E-01, -0.38539425E-01, -0.39482988E-01, -0.28172743E-01, \
 -0.10461548E-01,  0.72293030E-02,  0.19629899E-01,  0.23857031E-01,  0.19867271E-01, \
  0.10076012E-01, -0.16730814E-02, -0.11430031E-01, -0.16343743E-01, -0.15406003E-01, \
 -0.95601920E-02, -0.12011472E-02,  0.67212107E-02,  0.11686970E-01,  0.12363423E-01, \
  0.89212917E-02,  0.28455369E-02, -0.36542960E-02, -0.84163100E-02, -0.10022298E-01, \
 -0.81948247E-02, -0.38015433E-02,  0.15175294E-02,  0.59415763E-02,  0.80774855E-02, \
  0.73840916E-02,  0.42904904E-02, -0.14044337E-04, -0.40227529E-02, -0.64289104E-02, \
 -0.65423511E-02, -0.44770911E-02, -0.10602982E-02,  0.24869707E-02,  0.49791960E-02, \
  0.56554032E-02,  0.44051381E-02,  0.17638088E-02, -0.13042900E-02, -0.37535410E-02, \
 -0.48074462E-02, -0.42014252E-02, -0.22375220E-02,  0.34752686E-03,  0.26567061E-02, \
  0.39369017E-02,  0.38236496E-02,  0.24387422E-02,  0.31702407E-03, -0.17899896E-02, \
 -0.31829374E-02, -0.34412947E-02, -0.25477768E-02, -0.86848100E-03,  0.98881521E-03, \
  0.23946902E-02,  0.29073050E-02,  0.24096994E-02,  0.11268370E-02, -0.46738447E-03, \
 -0.18248872E-02, -0.25087805E-02, -0.23346422E-02, -0.14139037E-02, -0.10080911E-03, \
  0.11430616E-02,  0.19060248E-02,  0.19636080E-02,  0.13379015E-02,  0.27951988E-03, \
 -0.83641545E-03, -0.16343265E-02, -0.18772145E-02, -0.15157261E-02, -0.71785948E-03, \
  0.22151141E-03,  0.98129292E-03,  0.13131760E-02,  0.11324680E-02,  0.53038984E-03, \
 -0.26810885E-03, -0.99059893E-03, -0.14061579E-02, -0.14018975E-02, -0.10119241E-02, \
 -0.39880717E-03,  0.21137166E-03,  0.60708891E-03,  0.66196767E-03,  0.36935217E-03, \
 -0.16124418E-03, -0.74757589E-03, -0.11960072E-02, -0.13651208E-02, -0.12058818E-02, \
 -0.77180623E-03, -0.19336015E-03,  0.36705163E-03,  0.76842657E-03,  0.92948624E-03, \
  0.22739838E-02                                                                      \
};

/* A 6-factor decimation AA FIR filter */
static int dec6FIRnc = 91;
static double dec6FIR[91]= {                                                          \
  0.16570362E+00,  0.15829441E+00,  0.13725868E+00,  0.10594265E+00,  0.69211230E-01, \
  0.32528229E-01,  0.95806678E-03, -0.21721859E-01, -0.33611827E-01, -0.34888856E-01, \
 -0.27580921E-01, -0.14997372E-01, -0.94359554E-03,  0.11083508E-01,  0.18581118E-01, \
  0.20429686E-01,  0.16964789E-01,  0.97183716E-02,  0.92194974E-03, -0.70848679E-02, \
 -0.12422763E-01, -0.14064534E-01, -0.11991728E-01, -0.71014166E-02, -0.89137035E-03, \
  0.49562268E-02,  0.90078600E-02,  0.10408280E-01,  0.90432446E-02,  0.54955026E-02, \
  0.85260952E-03, -0.36210883E-02, -0.68052970E-02, -0.79971515E-02, -0.70541361E-02, \
 -0.43872511E-02, -0.80762105E-03,  0.27008946E-02,  0.52497657E-02,  0.62641921E-02, \
  0.56016045E-02,  0.35561360E-02,  0.75419003E-03, -0.20313612E-02, -0.40918030E-02, \
 -0.49545085E-02, -0.44863801E-02, -0.29051360E-02, -0.69879252E-03,  0.15243914E-02, \
  0.31950874E-02,  0.39263805E-02,  0.35993042E-02,  0.23769522E-02,  0.63838286E-03, \
 -0.11335427E-02, -0.24846792E-02, -0.30999891E-02, -0.28772806E-02, -0.19367724E-02, \
 -0.57383557E-03,  0.83092984E-03,  0.19156958E-02,  0.24286588E-02,  0.22825976E-02, \
  0.15665065E-02,  0.50730573E-03, -0.59655984E-03, -0.14599159E-02, -0.18816034E-02, \
 -0.17901543E-02, -0.12505304E-02, -0.43752801E-03,  0.41982584E-03,  0.10982298E-02, \
  0.14395756E-02,  0.13833372E-02,  0.97796321E-03,  0.35366282E-03, -0.30664937E-03, \
 -0.83971326E-03, -0.10991644E-02, -0.10712864E-02, -0.71174919E-03, -0.25581248E-03, \
  0.43810415E-03,  0.71178772E-03,  0.14833882E-02,  0.72977401E-03,  0.22810167E-02, \
  0.11542854E-02                                                                      \
};

/* A 7-factor decimation AA FIR filter */
static int dec7FIRnc = 113;
static double dec7FIR[113]= {                                                         \
  0.14208198E+00,  0.13739452E+00,  0.12389062E+00,  0.10316056E+00,  0.77608004E-01, \
  0.50108083E-01,  0.23616169E-01,  0.77113276E-03, -0.16440101E-01, -0.26909120E-01, \
 -0.30497886E-01, -0.27981848E-01, -0.20865075E-01, -0.11100596E-01, -0.76022197E-03, \
  0.82820896E-02,  0.14618799E-01,  0.17472565E-01,  0.16758278E-01,  0.13016257E-01, \
  0.72571430E-02,  0.74292615E-03, -0.52608857E-02, -0.97016394E-02, -0.11911346E-01, \
 -0.11686133E-01, -0.92841610E-02, -0.53405212E-02, -0.71840314E-03,  0.36636740E-02, \
  0.70070671E-02,  0.87675247E-02,  0.87357759E-02,  0.70546297E-02,  0.41608354E-02, \
  0.68841781E-03, -0.26673293E-02, -0.52824821E-02, -0.67139948E-02, -0.67736702E-02, \
 -0.55438336E-02, -0.33427100E-02, -0.65325515E-03,  0.19840142E-02,  0.40728515E-02, \
  0.52518910E-02,  0.53568939E-02,  0.44368859E-02,  0.27307249E-02,  0.61371550E-03, \
 -0.14870598E-02, -0.31737122E-02, -0.41507659E-02, -0.42769266E-02, -0.35821130E-02, \
 -0.22478071E-02, -0.56846417E-03,  0.11144583E-02,  0.24819272E-02,  0.32929941E-02, \
  0.34265392E-02,  0.29004803E-02,  0.18556728E-02,  0.52273611E-03, -0.82722993E-03, \
 -0.19346501E-02, -0.26047612E-02, -0.27381419E-02, -0.23428579E-02, -0.15266940E-02, \
 -0.47217472E-03,  0.60416642E-03,  0.14966615E-02,  0.20483169E-02,  0.21752114E-02, \
  0.18809054E-02,  0.12483622E-02,  0.42107934E-03, -0.43112988E-03, -0.11449105E-02, \
 -0.15943006E-02, -0.17113318E-02, -0.14960549E-02, -0.10118610E-02, -0.36934030E-03, \
  0.29832160E-03,  0.86243439E-03,  0.12237863E-02,  0.13277864E-02,  0.11733547E-02, \
  0.80783467E-03,  0.31598710E-03, -0.19982469E-03, -0.63952431E-03, -0.92532497E-03, \
 -0.10140305E-02, -0.90339431E-03, -0.62933657E-03, -0.25537529E-03,  0.13938319E-03, \
  0.47917303E-03,  0.70110161E-03,  0.77338412E-03,  0.68326876E-03,  0.47101121E-03, \
  0.14812217E-03, -0.15542324E-03, -0.53318450E-03, -0.62111754E-03, -0.10503677E-02, \
 -0.43352076E-03, -0.16600490E-02, -0.79061883E-03                                    \
};


/*********************************************************************
 * ddecimate:
 *
 * Decimate and low-pass filter a time-series.
 *
 * This routine is a modified version of SAC 2000's decim() routine.
 *
 * Arguments:
 *   data       : array of data samples
 *   npts       : number of samples
 *   factor     : decimation factor
 *   fir        : FIR coefficients, 1/2 of symmetric filter
 *   firnc      : number of FIR coefficients in array
 *   firsym     : FIR symmetry: 0=odd symmetry, 1=even symmetry
 *
 * If firnc is a negative number the default FIR filters will be used.
 * For the default FIR filters the decimation factor must be between
 * 2-7 (inclusive).
 *
 * No testing has been done with oddly symmetric FIR filters (firsym=0).
 *
 * Returns the number of samples in the output time series on success
 * and -1 on error.
 *********************************************************************/
int
ddecimate (double *data, int npts, int factor,
	   double *fir, int firnc, int firsym)
{
  int i;
  int in;
  int ioff;
  int nbstop;
  int nc;
  int nch;
  int nsave;
  int nptsstop;
  int out;
  int ptr;
  int nptsout;
  
  double temp;
  double *buf;
  
  double *Data;
  double *Buf;
  double *Fir;
  
  /* Determine AA filter to use if using internal filters */
  if ( firnc < 0 )
    {
      switch (factor)
	{
	case 2:
	  fir = dec2FIR; firnc = dec2FIRnc; firsym = 1;
	  break;
	case 3:
	  fir = dec3FIR; firnc = dec3FIRnc; firsym = 1;
	  break;
	case 4:
	  fir = dec4FIR; firnc = dec4FIRnc; firsym = 1;
	  break;
	case 5:
	  fir = dec5FIR; firnc = dec5FIRnc; firsym = 1;
	  break;
	case 6:
	  fir = dec6FIR; firnc = dec6FIRnc; firsym = 1;
	  break;
	case 7:
	  fir = dec7FIR; firnc = dec7FIRnc; firsym = 1;
	  break;
	default:
	  fprintf (stderr, "decimate(): decimation factor must be between 2 and 7 (not %d) for internal filters\n",
		   factor);
	  return -1;
	  break;
	}
    }
  
  /* Initializations */
  nch = firnc - 1;
  nc = 2*nch + 1;
  nptsout = (npts - 1)/factor + 1;
  nptsstop = npts - nch - factor;
  nbstop = firnc;
  ptr = firnc - factor;
  in = 1 - factor;
  out = 0;
  
  /* Allocate working buffer */
  if ( ! (buf = (double *) calloc(nc, sizeof(double))) )
    {
      fprintf (stderr, "decimate(): Cannot allocate memory\n");
      return -1;
    }
  
  /* Set crazy historical pointers, vestiges of fortran indexing, blarg. */
  /* Detangling the indexing logic below proved to be time not well spent */
  Data = data -1;
  Buf = buf - 1;
  Fir = fir - 1;
  
  /* Initialize buffer, the remaining initial samples are added below */
  for ( i = 1; i <= nch; i++ )
    Buf[i] = 0.0;
  
  for ( i = firnc; i <= (nc - factor); i++ )
    Buf[i] = Data[i - nch];
  
  /* Main loop - filter until end of data reached. */
  while ( in <= nptsstop )
    {
      /* Shift buffer, if end of buffer reached. */
      if ( ptr == nbstop )
	{
	  ptr = firnc - factor;
	  nsave = nc - factor;
	  for ( i = 1; i <= nsave; i++ )
	    Buf[i] = Buf[i + factor];
	}
      
      /* Update pointers */
      out = out + 1;
      in = in + factor;
      ptr = ptr + factor;
      
      /* Read in new data */
      for ( i = 1; i <= factor; i++ )
	{
	  ioff = nch + i - factor;
	  Buf[ptr + ioff] = Data[in + ioff];
	}
      
      /* Compute output point */
      temp = Fir[1]*Buf[ptr];
      for ( i = 1; i <= nch; i++ )
	temp = temp + Fir[i+1]*(Buf[ptr + i] + firsym*Buf[ptr - i]);
      
      Data[out] = temp;
    } /* end while */
  
  /* Loop to handle case where operator runs off of the data */
  while ( in <= npts - factor )
    {
      /* Shift buffer, if end of buffer reached. */
      if ( ptr == nbstop )
	{
	  ptr = firnc - factor;
	  nsave = nc - factor;
	  for ( i = 1; i <= nsave; i++ )
	    Buf[i] = Buf[i + factor];
	}
      
      /* Update pointers */
      out = out + 1;
      in = in + factor;
      ptr = ptr + factor;
      
      /* Read in new data */
      for ( i = 1; i <= factor; i++ )
	{
	  ioff = nch + i - factor;
	  if ( in + ioff > npts )
	    Buf[ptr + ioff] = 0.0;
	  
	  else
	    Buf[ptr + ioff] = Data[in + ioff];
	}
      
      /* Compute output point */
      temp = Fir[1]*Buf[ptr];
      for( i = 1; i <= nch; i++ )
	temp = temp + Fir[i+1]*(Buf[ptr + i] + firsym*Buf[ptr - i]);
      
      Data[out] = temp;
    } /* end while */
  
  /* Free working buffer */
  if ( buf )
    free (buf);
  
  return nptsout;
}  /* End of ddecimate() */


/*********************************************************************
 * fdecimate:
 *
 * Decimate and low-pass filter a time-series.
 *
 * This routine is a modified version of SAC 2000's decim() routine.
 *
 * Arguments:
 *   data       : array of data samples
 *   npts       : number of samples
 *   factor     : decimation factor
 *   fir        : FIR coefficients, 1/2 of symmetric filter
 *   firnc      : number of FIR coefficients in array
 *   firsym     : FIR symmetry: 0=odd symmetry, 1=even symmetry
 *
 * If firnc is a negative number the default FIR filters will be used.
 * For the default FIR filters the decimation factor must be between
 * 2-7 (inclusive).
 *
 * No testing has been done with oddly symmetric FIR filters (firsym=0).
 *
 * Returns the number of samples in the output time series on success
 * and -1 on error.
 *********************************************************************/
int
fdecimate (float *data, int npts, int factor,
	   double *fir, int firnc, int firsym)
{
  int i;
  int in;
  int ioff;
  int nbstop;
  int nc;
  int nch;
  int nsave;
  int nptsstop;
  int out;
  int ptr;
  int nptsout;
  
  double temp;
  double *buf;
  
  float *Data;
  double *Buf;
  double *Fir;
  
  /* Determine AA filter to use if using internal filters */
  if ( firnc < 0 )
    {
      switch (factor)
	{
	case 2:
	  fir = dec2FIR; firnc = dec2FIRnc; firsym = 1;
	  break;
	case 3:
	  fir = dec3FIR; firnc = dec3FIRnc; firsym = 1;
	  break;
	case 4:
	  fir = dec4FIR; firnc = dec4FIRnc; firsym = 1;
	  break;
	case 5:
	  fir = dec5FIR; firnc = dec5FIRnc; firsym = 1;
	  break;
	case 6:
	  fir = dec6FIR; firnc = dec6FIRnc; firsym = 1;
	  break;
	case 7:
	  fir = dec7FIR; firnc = dec7FIRnc; firsym = 1;
	  break;
	default:
	  fprintf (stderr, "decimate(): decimation factor must be between 2 and 7 (not %d) for internal filters\n",
		   factor);
	  return -1;
	  break;
	}
    }
  
  /* Initializations */
  nch = firnc - 1;
  nc = 2*nch + 1;
  nptsout = (npts - 1)/factor + 1;
  nptsstop = npts - nch - factor;
  nbstop = firnc;
  ptr = firnc - factor;
  in = 1 - factor;
  out = 0;
  
  /* Allocate working buffer */
  if ( ! (buf = (double *) calloc(nc, sizeof(double))) )
    {
      fprintf (stderr, "decimate(): Cannot allocate memory\n");
      return -1;
    }
  
  /* Set crazy historical pointers, vestiges of fortran indexing, blarg. */
  /* Detangling the indexing logic below proved to be time not well spent */
  Data = data -1;
  Buf = buf - 1;
  Fir = fir - 1;
  
  /* Initialize buffer, the remaining initial samples are added below */
  for ( i = 1; i <= nch; i++ )
    Buf[i] = 0.0;
  
  for ( i = firnc; i <= (nc - factor); i++ )
    Buf[i] = Data[i - nch];
  
  /* Main loop - filter until end of data reached. */
  while ( in <= nptsstop )
    {
      /* Shift buffer, if end of buffer reached. */
      if ( ptr == nbstop )
	{
	  ptr = firnc - factor;
	  nsave = nc - factor;
	  for ( i = 1; i <= nsave; i++ )
	    Buf[i] = Buf[i + factor];
	}
      
      /* Update pointers */
      out = out + 1;
      in = in + factor;
      ptr = ptr + factor;
      
      /* Read in new data */
      for ( i = 1; i <= factor; i++ )
	{
	  ioff = nch + i - factor;
	  Buf[ptr + ioff] = Data[in + ioff];
	}
      
      /* Compute output point */
      temp = Fir[1]*Buf[ptr];
      for ( i = 1; i <= nch; i++ )
	temp = temp + Fir[i+1]*(Buf[ptr + i] + firsym*Buf[ptr - i]);
      
      Data[out] = temp;
    } /* end while */
  
  /* Loop to handle case where operator runs off of the data */
  while ( in <= npts - factor )
    {
      /* Shift buffer, if end of buffer reached. */
      if ( ptr == nbstop )
	{
	  ptr = firnc - factor;
	  nsave = nc - factor;
	  for ( i = 1; i <= nsave; i++ )
	    Buf[i] = Buf[i + factor];
	}
      
      /* Update pointers */
      out = out + 1;
      in = in + factor;
      ptr = ptr + factor;
      
      /* Read in new data */
      for ( i = 1; i <= factor; i++ )
	{
	  ioff = nch + i - factor;
	  if ( in + ioff > npts )
	    Buf[ptr + ioff] = 0.0;
	  
	  else
	    Buf[ptr + ioff] = Data[in + ioff];
	}
      
      /* Compute output point */
      temp = Fir[1]*Buf[ptr];
      for( i = 1; i <= nch; i++ )
	temp = temp + Fir[i+1]*(Buf[ptr + i] + firsym*Buf[ptr - i]);
      
      Data[out] = temp;
    } /* end while */
  
  /* Free working buffer */
  if ( buf )
    free (buf);
  
  return nptsout;
}  /* End of fdecimate() */


/*********************************************************************
 * idecimate:
 *
 * Decimate and low-pass filter a 32-bit integer time-series.
 *
 * This routine is a modified version of SAC 2000's decim() routine.
 *
 * Arguments:
 *   data       : array of data samples
 *   npts       : number of samples
 *   factor     : decimation factor
 *   fir        : FIR coefficients, 1/2 of symmetric filter
 *   firnc      : number of FIR coefficients in array
 *   firsym     : FIR symmetry: 0=odd symmetry, 1=even symmetry
 *
 * If firnc is a negative number the default FIR filters will be used.
 * For the default FIR filters the decimation factor must be between
 * 2-7 (inclusive).
 *
 * No testing has been done with oddly symmetric FIR filters (firsym=0).
 *
 * Returns the number of samples in the output time series on success
 * and -1 on error.
 *********************************************************************/
int
idecimate (int32_t *data, int npts, int factor,
	   double *fir, int firnc, int firsym)
{
  int i;
  int in;
  int ioff;
  int nbstop;
  int nc;
  int nch;
  int nsave;
  int nptsstop;
  int out;
  int ptr;
  int nptsout;
  
  double temp;
  double *buf;
  
  int32_t *Data;
  double *Buf;
  double *Fir;
  
  /* Determine AA filter to use if using internal filters */
  if ( firnc < 0 )
    {
      switch (factor)
	{
	case 2:
	  fir = dec2FIR; firnc = dec2FIRnc; firsym = 1;
	  break;
	case 3:
	  fir = dec3FIR; firnc = dec3FIRnc; firsym = 1;
	  break;
	case 4:
	  fir = dec4FIR; firnc = dec4FIRnc; firsym = 1;
	  break;
	case 5:
	  fir = dec5FIR; firnc = dec5FIRnc; firsym = 1;
	  break;
	case 6:
	  fir = dec6FIR; firnc = dec6FIRnc; firsym = 1;
	  break;
	case 7:
	  fir = dec7FIR; firnc = dec7FIRnc; firsym = 1;
	  break;
	default:
	  fprintf (stderr, "decimate(): decimation factor must be between 2 and 7 (not %d) for internal filters\n",
		   factor);
	  return -1;
	  break;
	}
    }
  
  /* Initializations */
  nch = firnc - 1;
  nc = 2*nch + 1;
  nptsout = (npts - 1)/factor + 1;
  nptsstop = npts - nch - factor;
  nbstop = firnc;
  ptr = firnc - factor;
  in = 1 - factor;
  out = 0;
  
  /* Allocate working buffer */
  if ( ! (buf = (double *) calloc(nc, sizeof(double))) )
    {
      fprintf (stderr, "decimate(): Cannot allocate memory\n");
      return -1;
    }
  
  /* Set crazy historical pointers, vestiges of fortran indexing, blarg. */
  /* Detangling the indexing logic below proved to be time not well spent */
  Data = data -1;
  Buf = buf - 1;
  Fir = fir - 1;
  
  /* Initialize buffer, the remaining initial samples are added below */
  for ( i = 1; i <= nch; i++ )
    Buf[i] = 0.0;
  
  for ( i = firnc; i <= (nc - factor); i++ )
    Buf[i] = Data[i - nch];
  
  /* Main loop - filter until end of data reached. */
  while ( in <= nptsstop )
    {
      /* Shift buffer, if end of buffer reached. */
      if ( ptr == nbstop )
	{
	  ptr = firnc - factor;
	  nsave = nc - factor;
	  for ( i = 1; i <= nsave; i++ )
	    Buf[i] = Buf[i + factor];
	}
      
      /* Update pointers */
      out = out + 1;
      in = in + factor;
      ptr = ptr + factor;
      
      /* Read in new data */
      for ( i = 1; i <= factor; i++ )
	{
	  ioff = nch + i - factor;
	  Buf[ptr + ioff] = Data[in + ioff];
	}
      
      /* Compute output point */
      temp = Fir[1]*Buf[ptr];
      for ( i = 1; i <= nch; i++ )
	temp = temp + Fir[i+1]*(Buf[ptr + i] + firsym*Buf[ptr - i]);
      
      /* Round the result using truncation back to integer */
      Data[out] = (int32_t) (temp + 0.5);
    } /* end while */
  
  /* Loop to handle case where operator runs off of the data */
  while ( in <= npts - factor )
    {
      /* Shift buffer, if end of buffer reached. */
      if ( ptr == nbstop )
	{
	  ptr = firnc - factor;
	  nsave = nc - factor;
	  for ( i = 1; i <= nsave; i++ )
	    Buf[i] = Buf[i + factor];
	}
      
      /* Update pointers */
      out = out + 1;
      in = in + factor;
      ptr = ptr + factor;
      
      /* Read in new data */
      for ( i = 1; i <= factor; i++ )
	{
	  ioff = nch + i - factor;
	  if ( in + ioff > npts )
	    Buf[ptr + ioff] = 0.0;
	  
	  else
	    Buf[ptr + ioff] = Data[in + ioff];
	}
      
      /* Compute output point */
      temp = Fir[1]*Buf[ptr];
      for( i = 1; i <= nch; i++ )
	temp = temp + Fir[i+1]*(Buf[ptr + i] + firsym*Buf[ptr - i]);
      
      /* Round the result using truncation back to integer */
      Data[out] = (int32_t) (temp + 0.5);
    } /* end while */
  
  /* Free working buffer */
  if ( buf )
    free (buf);
  
  return nptsout;
}  /* End of idecimate() */
