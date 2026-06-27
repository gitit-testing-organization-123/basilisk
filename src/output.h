#include "timestep.h"

/**
# Output functions

## *output_field()*: Multiple fields interpolated on a regular grid (text format)

This function interpolates a *list* of fields on a *n+1 x n+1* regular
grid. The resulting data are written in text format in the file
pointed to by *fp*. The correspondance between column numbers and
variables is summarised in the first line of the file. The data are
written row-by-row and each row is separated from the next by a blank
line. This format is compatible with the *splot* command of *gnuplot*
i.e. one could use something like

~~~bash
gnuplot> set pm3d map
gnuplot> splot 'fields' u 1:2:4
~~~

The arguments and their default values are:

*list*
: list of fields to output. Default is *all*.

*fp*
: file pointer. Default is *stdout*.

*n*
: number of points along each dimension. Default is *N*.

*linear*
: use first-order (default) or bilinear interpolation. 

*box*
: the lower-left and upper-right coordinates of the domain to consider.
 Default is the entire domain. */

trace
void output_field (scalar * list = all,
		   FILE * fp = stdout,
		   int n = N,
		   bool linear = false,
		   coord box[2] = {{X0, Y0}, {X0 + L0, Y0 + L0*Dimensions.y/Dimensions.x}})
{
  n++;
  int len = list_len (list);
  double Delta = 0.999999*(box[1].x - box[0].x)/(n - 1);
  int ny = (box[1].y - box[0].y)/Delta + 1;
  double ** field = (double **) matrix_new (n, ny, len*sizeof(double)), * v = field[0];
  for (int i = 0; i < n*ny*len; i++, v++)
    *v = nodata;
  coord box1[2] = {{box[0].x - Delta/2., box[0].y - Delta/2.},
		   {box[0].x + (n - 0.5)*Delta, box[0].y + (ny - 0.5)*Delta}};
  coord cn = {n, ny}, p;
#if _MPI
  v = field[0];
  foreach_region (p, box1, cn, reduction(min:v[:n*ny*len]))
#else
  foreach_region (p, box1, cn, cpu)
#endif
  {
    double ** alias = field; // so that qcc considers 'field' a local variable
    int i = (p.x - box1[0].x)/(box1[1].x - box1[0].x)*cn.x;
    int j = (p.y - box1[0].y)/(box1[1].y - box1[0].y)*cn.y;
    int k = 0;
    for (scalar s in list)
      alias[i][len*j + k++] = linear ? interpolate_linear (point, s, p.x, p.y, p.z) : s[];
  }
  
  if (pid() == 0) {
    fprintf (fp, "# 1:x 2:y");
    int i = 3;
    for (scalar s in list)
      fprintf (fp, " %d:%s", i++, s.name);
    fputc('\n', fp);
    for (int i = 0; i < n; i++) {
      double x = Delta*i + box[0].x;
      for (int j = 0; j < ny; j++) {
	double y = Delta*j + box[0].y;
	//	map (x, y);
	fprintf (fp, "%g %g", x, y);
	int k = 0;
	for (scalar s in list)
	  fprintf (fp, " %g", field[i][len*j + k++]);
	fputc ('\n', fp);
      }
      fputc ('\n', fp);
    }
    fflush (fp);
  }

  matrix_free (field);
}

/**
## *output_matrix()*: Single field interpolated on a regular grid (binary format)

This function writes a binary representation of a single field
interpolated on a regular *n x n* grid. The format is compatible with
the binary matrix format of gnuplot i.e. one could use

~~~bash
gnuplot> set pm3d map
gnuplot> splot 'matrix' binary u 2:1:3
~~~

The arguments and their default values are:

*f*
: a scalar field (compulsory).

*fp*
: file pointer. Default is *stdout*.

*n*
: number of points along each dimension. Default is *N*.

*linear*
: use first-order (default) or bilinear interpolation. 

*box*
: the lower-left and upper-right coordinates of the domain to consider.
 Default is the entire domain.
*/

trace
void output_matrix (scalar f,
		    FILE * fp = stdout,
		    int n = N,
		    bool linear = false,
		    const char * file = NULL,
		    coord box[2] = {{X0, Y0}, {X0 + L0, Y0 + L0*Dimensions.y/Dimensions.x}})
{
  coord cn = {n}, p;
  double delta = (box[1].x - box[0].x)/n;
  cn.y = (int)((box[1].y - box[0].y)/delta);
    
  double ** ppm = (double **) matrix_new (cn.x, cn.y, sizeof(double));
  double * ppm0 = &ppm[0][0];
  unsigned int len = cn.x*cn.y;
  for (int i = 0; i < len; i++)
    ppm0[i] = - HUGE;

#if _MPI
  foreach_region (p, box, cn, reduction(max:ppm0[:len]))
#else
  foreach_region (p, box, cn, cpu)
#endif
  {
    int i = (p.x - box[0].x)/(box[1].x - box[0].x)*cn.x;
    int j = (p.y - box[0].y)/(box[1].y - box[0].y)*cn.y;
    double ** alias = ppm; // so that qcc considers ppm a local variable
    alias[i][j] = linear ? interpolate_linear (point, f, p.x, p.y, p.z) : f[];
  }
  
  if (pid() == 0) {
    if (file) {
      fp = fopen (file, "wb");
      if (!fp) {
	perror (file);
	exit (1);
      }
    }
    float fn = cn.y;
    fwrite (&fn, sizeof(float), 1, fp);
    coord delta = {(box[1].x - box[0].x)/cn.x, (box[1].y - box[0].y)/cn.y};
    for (int j = 0; j < cn.y; j++) {
      float yp = box[0].y + delta.y*(j + 0.5);
      fwrite (&yp, sizeof(float), 1, fp);
    }
    for (int i = 0; i < cn.x; i++) {
      float xp = box[0].x + delta.x*(i + 0.5);
      fwrite (&xp, sizeof(float), 1, fp);
      for (int j = 0; j < cn.y; j++) {
	float z = ppm[i][j];
	fwrite (&z, sizeof(float), 1, fp);
      }
    }
    if (file)
      fclose (fp);
    else
      fflush (fp);
  }
    
  matrix_free (ppm);
}

/**
## Colormaps

Colormaps are arrays of (127) red, green, blue triplets. */

#define NCMAP 127

typedef void (* Colormap) (double cmap[NCMAP][3]);

void jet (double cmap[NCMAP][3])
{
  for (int i = 0; i < NCMAP; i++) {
    cmap[i][0] = 
      i <= 46 ? 0. : 
      i >= 111 ? -0.03125*(i - 111) + 1. :
      i >= 78 ? 1. : 
      0.03125*(i - 46);
    cmap[i][1] = 
      i <= 14 || i >= 111 ? 0. : 
      i >= 79 ? -0.03125*(i - 111) : 
      i <= 46 ? 0.03125*(i - 14) : 
      1.;
    cmap[i][2] =
      i >= 79 ? 0. :
      i >= 47 ? -0.03125*(i - 79) :
      i <= 14 ? 0.03125*(i - 14) + 1.:
      1.;
  }
}

void cool_warm (double cmap[NCMAP][3])
{
  /* diverging cool-warm from:
   *  http://www.sandia.gov/~kmorel/documents/ColorMaps/CoolWarmFloat33.csv
   * see also:
   *  Diverging Color Maps for Scientific Visualization (Expanded)
   *  Kenneth Moreland
   */
  static double basemap[33][3] = {
    {0.2298057,   0.298717966, 0.753683153},
    {0.26623388,  0.353094838, 0.801466763},
    {0.30386891,  0.406535296, 0.84495867},
    {0.342804478, 0.458757618, 0.883725899},
    {0.38301334,  0.50941904,  0.917387822},
    {0.424369608, 0.558148092, 0.945619588},
    {0.46666708,  0.604562568, 0.968154911},
    {0.509635204, 0.648280772, 0.98478814},
    {0.552953156, 0.688929332, 0.995375608},
    {0.596262162, 0.726149107, 0.999836203},
    {0.639176211, 0.759599947, 0.998151185},
    {0.681291281, 0.788964712, 0.990363227},
    {0.722193294, 0.813952739, 0.976574709},
    {0.761464949, 0.834302879, 0.956945269},
    {0.798691636, 0.849786142, 0.931688648},
    {0.833466556, 0.860207984, 0.901068838},
    {0.865395197, 0.86541021,  0.865395561},
    {0.897787179, 0.848937047, 0.820880546},
    {0.924127593, 0.827384882, 0.774508472},
    {0.944468518, 0.800927443, 0.726736146},
    {0.958852946, 0.769767752, 0.678007945},
    {0.96732803,  0.734132809, 0.628751763},
    {0.969954137, 0.694266682, 0.579375448},
    {0.966811177, 0.650421156, 0.530263762},
    {0.958003065, 0.602842431, 0.481775914},
    {0.943660866, 0.551750968, 0.434243684},
    {0.923944917, 0.49730856,  0.387970225},
    {0.89904617,  0.439559467, 0.343229596},
    {0.869186849, 0.378313092, 0.300267182},
    {0.834620542, 0.312874446, 0.259301199},
    {0.795631745, 0.24128379,  0.220525627},
    {0.752534934, 0.157246067, 0.184115123},
    {0.705673158, 0.01555616,  0.150232812}	
  };
  
  for (int i = 0; i < NCMAP; i++) {
    double x = i*(32 - 1e-10)/(NCMAP - 1);
    int j = x; x -= j;
    for (int k = 0; k < 3; k++)
      cmap[i][k] = (1. - x)*basemap[j][k] + x*basemap[j+1][k];
  }
}

void gray (double cmap[NCMAP][3])
{
  for (int i = 0; i < NCMAP; i++)
    for (int k = 0; k < 3; k++)
      cmap[i][k] = i/(NCMAP - 1.);
}

void randomap (double cmap[NCMAP][3])
{
  srand(0);
  for (int i = 0; i < NCMAP; i++)
    for (int k = 0; k < 3; k++)
      cmap[i][k] = (noise() + 1.)/2.;
}

void blue_white_red (double cmap[NCMAP][3])
{
  for (int i = 0; i < (NCMAP + 1)/2; i++) {
    cmap[i][0] = i/((NCMAP - 1)/2.);
    cmap[i][1] = i/((NCMAP - 1)/2.);
    cmap[i][2] = 1.;
  }
  for (int i = 0; i < (NCMAP - 1)/2; i++) {
    cmap[i + (NCMAP + 1)/2][0] = 1.;
    cmap[i + (NCMAP + 1)/2][1] = cmap[(NCMAP - 3)/2 - i][1];
    cmap[i + (NCMAP + 1)/2][2] = cmap[(NCMAP - 3)/2 - i][1];
  }
}

/**
Given a colormap and a minimum and maximum value, this function
returns the red/green/blue triplet corresponding to *val*. */

typedef struct {
  unsigned char r, g, b;
} Color;

Color colormap_color (double cmap[NCMAP][3], 
		      double val, double min, double max)
{
  Color c;
  if (val == nodata) {
    c.r = c.g = c.b = 0; // nodata is black
    return c;
  }
  int i;
  double coef;
  if (max != min)
    val = (val - min)/(max - min);
  else
    val = 0.;
  if (val <= 0.) i = 0, coef = 0.;
  else if (val >= 1.) i = NCMAP - 2, coef = 1.;
  else {
    i = val*(NCMAP - 1);
    coef = val*(NCMAP - 1) - i;
  }
  if (i < 0 || i >= NCMAP - 1)
    return (Color){99,55,43}; // brown is an error
  unsigned char * c1 = (unsigned char *) &c;
  for (int j = 0; j < 3; j++)
    c1[j] = 255*(cmap[i][j]*(1. - coef) + cmap[i + 1][j]*coef);
  return c;
}

/**
## Image/animation conversion

The open_image()/close_image() functions use pipes to convert PPM
images to other formats, including `.mp4`, `.ogv` and `.gif`
animations.

The functions check whether the 'ffmpeg' or 'convert' executables are
accessible, if they are not the conversion is disabled and the raw PPM
images are saved. An extra ".ppm" extension is added to the file name
to indicate that this happened. */

static const char * extension (const char * file, const char * ext) {
  int len = strlen(file);
  return len > 4 && !strcmp (file + len - 4, ext) ? file + len - 4 : NULL;
}

static const char * is_animation (const char * file) {
  const char * ext;
  if ((ext = extension (file, ".mp4")) ||
      (ext = extension (file, ".ogv")) ||
      (ext = extension (file, ".gif")))
    return ext;
  return NULL;
}

static struct {
  FILE ** fp;
  char ** names;
  int n;
} open_image_data = {NULL, NULL, 0};

static void open_image_cleanup()
{
  for (int i = 0; i < open_image_data.n; i++) {
    pclose (open_image_data.fp[i]);
    free (open_image_data.names[i]);
  }
  free (open_image_data.fp);
  free (open_image_data.names);
  open_image_data.fp = NULL;
  open_image_data.names = NULL;
  open_image_data.n = 0;
}

static FILE * open_image_lookup (const char * file)
{
  for (int i = 0; i < open_image_data.n; i++)
    if (!strcmp (file, open_image_data.names[i]))
      return open_image_data.fp[i];
  return NULL;
}

static bool which (const char * command)
{
  char * s = getenv ("PATH");
  if (!s)
    return false;
  char path[strlen(s) + 1];
  strcpy (path, s);
  s = strtok (path, ":");
  while (s) {
    char f[strlen(s) + strlen(command) + 2];
    strcpy (f, s);
    strcat (f, "/");
    strcat (f, command);
    FILE * fp = fopen (f, "r");
    if (fp) {
      fclose (fp);
      return true;
    }
    s = strtok (NULL, ":");
  }
  return false;
}

static FILE * ppm_fallback (const char * file, const char * mode)
{
  char filename[strlen(file) + 5];
  strcpy (filename, file);
  strcat (filename, ".ppm");
  FILE * fp = fopen (filename, mode);
  if (!fp) {
    perror (file);
#if _MPI
    MPI_Abort (MPI_COMM_WORLD, 1);
#endif
    exit (1);
  }
  return fp;
}

FILE * open_image (const char * file, const char * options)
{
@if __EMSCRIPTEN__
  return ppm_fallback (file, "w");
@else // !__EMSCRIPTEN__
  assert (pid() == 0);
  const char * ext;
  if ((ext = is_animation (file))) {
    FILE * fp = open_image_lookup (file);
    if (fp)
      return fp;

    int len = strlen ("ppm2???    ") + strlen (file) +
      (options ? strlen (options) : 0);
    char command[len + 1];
    strcpy (command, "ppm2"); strcat (command, ext + 1);

    static int has_ffmpeg = -1;
    if (has_ffmpeg < 0) {
      if (which (command) && (which ("ffmpeg") || which ("avconv")))
	has_ffmpeg = true;
      else {
	fprintf (ferr,
		 "src/output.h:%d: warning: cannot find '%s' or 'ffmpeg'/'avconv'\n"
		 "src/output.h:%d: warning: falling back to raw PPM outputs\n",
		 LINENO, command, LINENO);
	has_ffmpeg = false;
      }
    }
    if (!has_ffmpeg)
      return ppm_fallback (file, "a");

    static bool added = false;
    if (!added) {
      free_solver_func_add (open_image_cleanup);
      added = true;
    }      
    open_image_data.n++;
    qrealloc (open_image_data.names, open_image_data.n, char *);
    open_image_data.names[open_image_data.n - 1] = strdup (file);

    if (options) {
      strcat (command, " ");
      strcat (command, options);
    }
    strcat (command, !strcmp (ext, ".mp4") ? " " : " > ");
    strcat (command, file);
    qrealloc (open_image_data.fp, open_image_data.n, FILE *);
    return open_image_data.fp[open_image_data.n - 1] = popen (command, "w");
  }
  else { // !animation
    static int has_convert = -1;
    if (has_convert < 0) {
      if (which ("convert"))
	has_convert = true;
      else {
	fprintf (ferr,
		 "src/output.h:%d: warning: cannot find 'convert'\n"
		 "src/output.h:%d: warning: falling back to raw PPM outputs\n",
		 LINENO, LINENO);
	has_convert = false;
      }
    }
    if (!has_convert)
      return ppm_fallback (file, "w");
    
    int len = strlen ("convert ppm:-   ") + strlen (file) +
      (options ? strlen (options) : 0);
    char command[len];
    strcpy (command, "convert ppm:- ");
    if (options) {
      strcat (command, options);
      strcat (command, " ");
    }
    strcat (command, file);
    return popen (command, "w");
  }
@endif // !__EMSCRIPTEN__
}

void close_image (const char * file, FILE * fp)
{
  assert (pid() == 0);
  if (is_animation (file)) {
    if (!open_image_lookup (file))
      fclose (fp);
  }
@if !__EMSCRIPTEN__
  else if (which ("convert"))
    pclose (fp);
@endif // !__EMSCRIPTEN__
  else
    fclose (fp);
}

/**
## *output_ppm()*: Portable PixMap (PPM) image output

Given a field, this function outputs a colormaped representation as a
[Portable PixMap](http://en.wikipedia.org/wiki/Netpbm_format) image.

If [ImageMagick](http://www.imagemagick.org/) is installed on the
system, this image can optionally be converted to any image format
supported by ImageMagick.

The arguments and their default values are:

*f*
: a scalar field (compulsory).

*fp*
: a file pointer. Default is stdout.

*n*
: number of pixels. Default is *N*.

*file*
: sets the name of the file used as output for
ImageMagick. This allows outputs in all formats supported by
ImageMagick. For example, one could use

~~~c
output_ppm (f, file = "f.png");
~~~

to get a [PNG](http://en.wikipedia.org/wiki/Portable_Network_Graphics)
image.

*min, max*
: minimum and maximum values used to define the
colorscale. By default these are set automatically using the *spread*
parameter. 

*spread*
: if not specified explicitly, *min* and *max* are set to the average
of the field minus (resp. plus) *spread* times the standard deviation.
By default *spread* is five. If negative, the minimum and maximum
values of the field are used.

*z*
: the z-coordinate (in 3D) of the plane being represented.

*linear*
: whether to use bilinear or first-order interpolation. Default is 
first-order.

*box*
: the lower-left and upper-right coordinates of the domain to consider.
 Default is the entire domain.

*mask*
: if set, this field will be used to mask out (in black), the regions 
of the domain for which *mask* is negative. 

*map*
: the colormap: *jet*, *cool_warm* or *gray*. Default is *jet*.

*opt*
: options to pass to 'convert' or to the 'ppm2???' scripts (used
with *file*).

*fps*
: used only for [online output](grid/gpu/output.h) on GPUs.

*checksum*
: write a checksum of the generated image in the file pointed.
*/

trace
void output_ppm (scalar f,
		 FILE * fp = stdout,
		 int n = N,
		 char * file = NULL,
		 double min = 0, double max = 0, double spread = 5,
		 double z = 0,
		 bool linear = false,
		 coord box[2] = {{X0, Y0}, {X0 + L0, Y0 + L0*Dimensions.y/Dimensions.x}},
		 scalar mask = {-1},
		 Colormap map = jet,
		 char * opt = NULL,
		 int fps = 0,
		 FILE * checksum = NULL)
{
  // default values
  if (!min && !max) {
    stats s = statsf (f);
    if (spread < 0.)
      min = s.min, max = s.max;
    else {
      double avg = s.sum/s.volume;
      min = avg - spread*s.stddev; max = avg + spread*s.stddev;
    }
  }
  box[0].z = z, box[1].z = z;
  
  coord cn = {n}, p;
  double delta = (box[1].x - box[0].x)/n;
  cn.y = (int)((box[1].y - box[0].y)/delta);
  if (((int)cn.y) % 2) cn.y++;
    
  Color ** ppm = (Color **) matrix_new (cn.y, cn.x, sizeof(Color));
  unsigned char * ppm0 = &ppm[0][0].r;
  int len = 3*cn.x*cn.y;
  memset (ppm0, 0, len*sizeof (unsigned char));
  double cmap[NCMAP][3];
  (* map) (cmap);

#if _MPI
  foreach_region (p, box, cn, reduction(max:ppm0[:len]))
#else
  foreach_region (p, box, cn, cpu)
#endif
  {
    double v;
    if (mask.i >= 0) { // masking
      if (linear) {
	double m = interpolate_linear (point, mask, p.x, p.y, p.z);
	if (m < 0.)
	  v = nodata;
	else
	  v = interpolate_linear (point, f, p.x, p.y, p.z);
      }
      else {
	if (mask[] < 0.)
	  v = nodata;
	else
	  v = f[];
      }
    }
    else if (linear)
      v = interpolate_linear (point, f, p.x, p.y, p.z);
    else
      v = f[];
    int i = (p.x - box[0].x)/(box[1].x - box[0].x)*cn.x;
    int j = (p.y - box[0].y)/(box[1].y - box[0].y)*cn.y;
    Color ** alias = ppm; // so that qcc considers ppm a local variable
    alias[(int)cn.y - 1 - j][i] = colormap_color (cmap, v, min, max);	    
  }
  
  if (pid() == 0) {
    if (file)
      fp = open_image (file, opt);
    
    fprintf (fp, "P6\n%g %g 255\n", cn.x, cn.y);
    fwrite (ppm0, sizeof(unsigned char), 3*cn.x*cn.y, fp);
    
    if (file)
      close_image (file, fp);
    else
      fflush (fp);

    if (checksum) {
      Adler32Hash hash;
      a32_hash_init (&hash);
      a32_hash_add (&hash, ppm0, sizeof(unsigned char)*3*cn.x*cn.y);
      fputs ("# ", checksum);
      if (file)
	fprintf (checksum, "%s: ", file);
      fprintf (checksum, "checksum: %08lx\n", (unsigned long) a32_hash (&hash));
    }
  }
    
  matrix_free (ppm);
}

/**
## *output_grd()*: ESRI ASCII Grid format

The [ESRI GRD format](http://en.wikipedia.org/wiki/Esri_grid) is a
standard format for importing raster data into [GIS
systems](http://en.wikipedia.org/wiki/Geographic_information_system).

The arguments and their default values are:

*f*
: a scalar field (compulsory).

*fp*
: a file pointer. Default is stdout.

$\Delta$
: size of a grid element. Default is L0/N.

*linear*
: whether to use bilinear or first-order interpolation. Default is 
first-order.

*box*
: the lower-left and upper-right coordinates of the domain to consider.
 Default is the entire domain.

*mask*
: if set, this field will be used to mask out, the regions 
of the domain for which *mask* is negative. */

trace
void output_grd (scalar f,
		 FILE * fp = stdout,
		 double Delta = L0/N,
		 bool linear = false,
		 coord box[2] = {{X0, Y0}, {X0 + L0, Y0 + L0*Dimensions.y/Dimensions.x}},
		 scalar mask = {-1})
{
  int nx = (box[1].x - box[0].x)/Delta;
  int ny = (box[1].y - box[0].y)/Delta;

  // header
  fprintf (fp, "ncols          %d\n", nx);
  fprintf (fp, "nrows          %d\n", ny);
  fprintf (fp, "xllcorner      %g\n", box[0].x);
  fprintf (fp, "yllcorner      %g\n", box[0].y);
  fprintf (fp, "cellsize       %g\n", Delta);
  fprintf (fp, "nodata_value   -9999\n");
  
  // data
  for (int j = ny-1; j >= 0; j--) {
    double yp = Delta*j + box[0].y + Delta/2.;
    for (int i = 0; i < nx; i++) {
      double xp = Delta*i + box[0].x + Delta/2., v;
      if (mask.i >= 0) { // masking
	double m = interpolate (mask, xp, yp, linear = linear);
	if (m < 0.)
	  v = nodata;
	else
	  v = interpolate (f, xp, yp, linear = linear);
      }
      else
	v = interpolate (f, xp, yp, linear = linear);
      if (v == nodata)
	fprintf (fp, "-9999 ");
      else
	fprintf (fp, "%f ", v);
    }
    fprintf (fp, "\n");
  }

  fflush (fp);
}

#if MULTIGRID

/**
## *output_gfs()*: Gerris simulation format

The function writes simulation data in the format used in
[Gerris](https://gerris.dalembert.upmc.fr) simulation files. These
files can be read with GfsView.

The arguments and their default values are:

*fp*
: a file pointer. Default is stdout or *file*.

*list*
: a list of scalar fields to write. Default is *all*. 

*file*
: the name of the file to write to (mutually exclusive with *fp*).

*translate*
: whether to replace "well-known" Basilisk variables with their Gerris
equivalents.
*/

static char * replace (const char * input, int target, int with,
		       bool translate)
{
  if (translate) {
    if (!strcmp (input, "u.x"))
      return strdup ("U");
    if (!strcmp (input, "u.y"))
      return strdup ("V");
    if (!strcmp (input, "u.z"))
      return strdup ("W");
  }
  char * name = strdup (input), * i = name;
  while (*i != '\0') {
    if (*i == target)
      *i = with;
    i++;
  }
  return name;
}

trace
void output_gfs (FILE * fp = NULL,
		 scalar * list = NULL,
		 char * file = NULL,
		 bool translate = false)
{
  char * fname = file;
  
#if _MPI
#if MULTIGRID_MPI
  not_mpi_compatible();
#endif // !MULTIGRID_MPI
  FILE * sfp = fp;
  if (file == NULL) {
    long pid = getpid();
    MPI_Bcast (&pid, 1, MPI_LONG, 0, MPI_COMM_WORLD);
    fname = qmalloc (80, char);
    snprintf (fname, 80, ".output-%ld", pid);
    fp = NULL;
  }
#endif // _MPI
  
  bool opened = false;
  if (fp == NULL) {
    if (fname == NULL)
      fp = stdout;
    else if (!(fp = fopen (fname, "w"))) {
      perror (fname);
      exit (1);
    }
    else
      opened = true;
  }
  
  scalar * slist = list ? list : list_copy (all);

  restriction (slist);
  fprintf (fp, 
	   "1 0 GfsSimulation GfsBox GfsGEdge { binary = 1"
	   " x = %g y = %g ",
	   0.5 + X0/L0, 0.5 + Y0/L0);
#if dimension == 3
  fprintf (fp, "z = %g ", 0.5 + Z0/L0);
#endif

  if (slist != NULL && slist[0].i != -1) {
    scalar s = slist[0];
    char * name = replace (s.name, '.', '_', translate);
    fprintf (fp, "variables = %s", name);
    free (name);
    for (int i = 1; i < list_len(slist); i++) {
      scalar s = slist[i];
      if (s.name) {
	char * name = replace (s.name, '.', '_', translate);
	fprintf (fp, ",%s", name);
	free (name);
      }
    }
    fprintf (fp, " ");
  }
  fprintf (fp, "} {\n");
  fprintf (fp, "  Time { t = %g }\n", t);
  if (L0 != 1.)
    fprintf (fp, "  PhysicalParams { L = %g }\n", L0);
  fprintf (fp, "  VariableTracerVOF f\n");
  fprintf (fp, "}\nGfsBox { x = 0 y = 0 z = 0 } {\n");

#if _MPI
  long header;
  if ((header = ftell (fp)) < 0) {
    perror ("output_gfs(): error in header");
    exit (1);
  }
  int cell_size = sizeof(unsigned) + sizeof(double);
  for (scalar s in slist)
    if (s.name)
      cell_size += sizeof(double);
  scalar index = new scalar;
  size_t total_size = header + (z_indexing (index, false) + 1)*cell_size;
#endif
  
  // see gerris/ftt.c:ftt_cell_write()
  //     gerris/domain.c:gfs_cell_write()
  foreach_cell() {
#if _MPI // fixme: this won't work when combining MPI and mask()
    if (is_local(cell))
#endif
    {
#if _MPI
      if (fseek (fp, header + index[]*cell_size, SEEK_SET) < 0) {
	perror ("output_gfs(): error while seeking");
	exit (1);
      }
#endif
      unsigned flags = 
	level == 0 ? 0 :
#if dimension == 1
	child.x == 1;
#elif dimension == 2
      child.x == -1 && child.y == -1 ? 0 :
	child.x == -1 && child.y ==  1 ? 1 :
	child.x ==  1 && child.y == -1 ? 2 : 
	3;
#else // dimension == 3
      child.x == -1 && child.y == -1 && child.z == -1  ? 0 :
	child.x == -1 && child.y == -1 && child.z ==  1  ? 1 :
	child.x == -1 && child.y ==  1 && child.z == -1  ? 2 : 
	child.x == -1 && child.y ==  1 && child.z ==  1  ? 3 : 
	child.x ==  1 && child.y == -1 && child.z == -1 ? 4 :
	child.x ==  1 && child.y == -1 && child.z ==  1 ? 5 :
	child.x ==  1 && child.y ==  1 && child.z == -1 ? 6 : 
	7;
#endif
      if (is_leaf(cell))
	flags |= (1 << 4);
      fwrite (&flags, sizeof (unsigned), 1, fp);
      double a = -1;
      fwrite (&a, sizeof (double), 1, fp);
      for (scalar s in slist)
	if (s.name) {
	  if (s.v.x.i >= 0) {
	    // this is a vector component, we need to rotate from
	    // N-ordering (Basilisk) to Z-ordering (Gerris)
	    // fixme: this does not work for tensors
#if dimension >= 2
	    if (s.v.x.i == s.i) {
	      s = s.v.y;
	      a = is_local(cell) && s[] != nodata ? s[] : (double) DBL_MAX;
	    }
	    else if (s.v.y.i == s.i) {
	      s = s.v.x;
	      a = is_local(cell) && s[] != nodata ? - s[] : (double) DBL_MAX;
	    }
#endif
#if dimension >= 3
	    else
	      a = is_local(cell) && s[] != nodata ? s[] : (double) DBL_MAX;
#endif
	  }
	  else
	    a = is_local(cell) && s[] != nodata ? s[] : (double) DBL_MAX;
	  fwrite (&a, sizeof (double), 1, fp);
	}
    }
    if (is_leaf(cell))
      continue;
  }
  
#if _MPI
  delete ({index});
  if (!pid() && fseek (fp, total_size, SEEK_SET) < 0) {
    perror ("output_gfs(): error while finishing");
    exit (1);
  }
  if (!pid())
#endif  
    fputs ("}\n", fp);
  fflush (fp);

  if (!list)
    free (slist);
  if (opened)
    fclose (fp);

#if _MPI
  if (file == NULL) {
    MPI_Barrier (MPI_COMM_WORLD);
    if (pid() == 0) {
      if (sfp == NULL)
	sfp = stdout;
      fp = fopen (fname, "r");
      size_t l;
      unsigned char buffer[8192];
      while ((l = fread (buffer, 1, 8192, fp)) > 0)
	fwrite (buffer, 1, l, sfp);
      fflush (sfp);
      remove (fname);
    }
    free (fname);
  }
#endif // _MPI
}

/**
## *dump()*: Basilisk snapshots

This function (together with *restore()*) can be used to dump/restore
entire simulations.

The arguments and their default values are:

*file*
: the name of the file to write to (mutually exclusive with *fp*). The
default is "dump".

*list*
: a list of scalar fields to write. Default is *all*. 

*fp*
: a file pointer. Default is stdout.

*unbuffered*
: whether to use a file buffer. Default is false.

*zero*
: whether to dump fields which are zero. Default is true.
*/

struct DumpHeader {
  double t, dt, dt_previous;
  long len;
  int i, depth, npe, version;
  coord n;
};

static const int dump_version =
  260628;

static int dump_field_axis (scalar s)
{
  int axis = 0;
  foreach_dimension() {
    if (s.v.x.i == s.i)
      return axis;
    axis++;
  }
  return -1;
}

static int dump_field_kind (scalar s)
{
  return s.face ? dump_field_face :
    is_vertex_scalar (s) ? dump_field_vertex : dump_field_cell;
}

static int dump_field_count (int kind)
{
  return kind == dump_field_face ? 2 :
    kind == dump_field_vertex ? (1 << dimension) : 1;
}

static bool dump_field_nonzero (scalar s)
{
  if (s.face) {
    int nonzero = 0;
    foreach_dimension()
      if (s.v.x.i == s.i) {
	scalar sf = s.v.x;
	foreach_face (x, reduction(max:nonzero))
	  if (sf[] != 0.)
	    nonzero = 1;
      }
    return nonzero;
  }
  if (is_vertex_scalar (s)) {
    int nonzero = 0;
    foreach_vertex (reduction(max:nonzero))
      if (s[] != 0.)
	nonzero = 1;
    return nonzero;
  }
  stats ss = statsf (s);
  return ss.min != 0. || ss.max != 0.;
}

static scalar * dump_list (scalar * lista, bool zero)
{
  scalar * list = is_constant(cm) ? NULL : list_concat ({cm}, NULL);
  // fixme: on GPUs statsf() can change the `all` list, because it
  // allocates new fields to store reductions, which causes a nasty
  // crash...
#if 1
  scalar * listb = list_copy (lista);
#endif
  for (scalar s in listb)
    if (!s.nodump && s.i != cm.i) {
      if (zero)
	list = list_add (list, s);
      else if (dump_field_nonzero (s))
	list = list_add (list, s);
    }
  free (listb);
  return list;
}

static DumpField * dump_fields (scalar * list, int * nfields, long * record_len)
{
  *nfields = list_len (list);
  DumpField * fields = *nfields ? (DumpField *) malloc (*nfields*sizeof(DumpField)) : NULL;
  *record_len = 1; // subtree size
  int i = 0;
  for (scalar s in list) {
    fields[i].name = strdup (s.name);
    fields[i].index = s.i;
    fields[i].kind = dump_field_kind (s);
    fields[i].axis = fields[i].kind == dump_field_face ?
      dump_field_axis (s) : -1;
    fields[i].count = dump_field_count (fields[i].kind);
    *record_len += fields[i].count;
    i++;
  }
  return fields;
}

static void dump_fields_free (DumpField * fields, int nfields)
{
  for (int i = 0; i < nfields; i++)
    free (fields[i].name);
  free (fields);
}

static long dump_header_size (DumpField * fields, int nfields)
{
  long size = sizeof(struct DumpHeader) + sizeof(int) + 4*sizeof(double);
  for (int i = 0; i < nfields; i++)
    size += sizeof(unsigned) + strlen(fields[i].name)*sizeof(char) +
      3*sizeof(int);
  return size;
}

static void dump_header (FILE * fp, struct DumpHeader * header,
			 DumpField * fields, int nfields)
{
  if (fwrite (header, sizeof(struct DumpHeader), 1, fp) < 1) {
    perror ("dump(): error while writing header");
    exit (1);
  }
  if (fwrite (&nfields, sizeof(int), 1, fp) < 1) {
    perror ("dump(): error while writing nfields");
    exit (1);
  }
  for (int i = 0; i < nfields; i++) {
    unsigned len = strlen(fields[i].name);
    if (fwrite (&len, sizeof(unsigned), 1, fp) < 1) {
      perror ("dump(): error while writing len");
      exit (1);
    }
    if (fwrite (fields[i].name, sizeof(char), len, fp) < len) {
      perror ("dump(): error while writing s.name");
      exit (1);
    }
    if (fwrite (&fields[i].kind, sizeof(int), 1, fp) < 1 ||
	fwrite (&fields[i].axis, sizeof(int), 1, fp) < 1 ||
	fwrite (&fields[i].count, sizeof(int), 1, fp) < 1) {
      perror ("dump(): error while writing field descriptor");
      exit (1);
    }
  }
  double o[4] = {X0,Y0,Z0,L0};
  if (fwrite (o, sizeof(double), 4, fp) < 4) {
    perror ("dump(): error while writing coordinates");
    exit (1);
  }
}

static void dump_write_double (FILE * fp, double val)
{
  if (fwrite (&val, sizeof(double), 1, fp) < 1) {
    perror ("dump(): error while writing field value");
    exit (1);
  }
}

static void dump_write_item (Point point, FILE * fp,
			     int index, int kind, int axis)
{
  if (kind == dump_field_cell)
    dump_write_double (fp, val(((scalar){index}),0,0,0));
  else if (kind == dump_field_face) {
    int d = 0;
    foreach_dimension() {
      if (d == axis) {
	dump_write_double (fp, val(((scalar){index}),0,0,0));
	dump_write_double (fp, val(((scalar){index}),1,0,0));
      }
      d++;
    }
  }
  else if (kind == dump_field_vertex) {
    for (int i = 0; i <= 1; i++)
#if dimension >= 2
      for (int j = 0; j <= 1; j++)
#endif
#if dimension >= 3
	for (int k = 0; k <= 1; k++)
#endif
	{
#if dimension == 1
	  dump_write_double (fp, val(((scalar){index}),i,0,0));
#elif dimension == 2
	  dump_write_double (fp, val(((scalar){index}),i,j,0));
#else
	  dump_write_double (fp, val(((scalar){index}),i,j,k));
#endif
	}
  }
}

static double dump_read_double (FILE * fp)
{
  double val;
  if (fread (&val, sizeof(double), 1, fp) != 1) {
    fprintf (ferr, "restore(): error: expecting a field value\n");
    exit (1);
  }
  return isfinite(val) ? val : nodata;
}

static void dump_read_item (Point point, FILE * fp,
			    int index, int kind, int axis, int count)
{
  if (index == INT_MAX) {
    for (int i = 0; i < count; i++)
      dump_read_double (fp);
    return;
  }
  if (kind == dump_field_cell)
    val(((scalar){index}),0,0,0) = dump_read_double (fp);
  else if (kind == dump_field_face) {
    int d = 0;
    foreach_dimension() {
      if (d == axis) {
	val(((scalar){index}),0,0,0) = dump_read_double (fp);
	val(((scalar){index}),1,0,0) = dump_read_double (fp);
      }
      d++;
    }
  }
  else if (kind == dump_field_vertex) {
    for (int i = 0; i <= 1; i++)
#if dimension >= 2
      for (int j = 0; j <= 1; j++)
#endif
#if dimension >= 3
	for (int k = 0; k <= 1; k++)
#endif
	{
#if dimension == 1
	  val(((scalar){index}),i,0,0) = dump_read_double (fp);
#elif dimension == 2
	  val(((scalar){index}),i,j,0) = dump_read_double (fp);
#else
	  val(((scalar){index}),i,j,k) = dump_read_double (fp);
#endif
	}
  }
}

static scalar dump_find_scalar (const char * name)
{
  for (scalar s in all)
    if (s.name && !strcmp (s.name, name))
      return s;
  return (scalar){INT_MAX};
}

static void dump_rename_scalar (scalar s, const char * name)
{
  free (s.name);
  s.name = strdup (name);
}

static scalar dump_new_face_component (const char * name, int axis)
{
  const char * suffix = axis == 0 ? ".x" : axis == 1 ? ".y" : ".z";
  int len = strlen (name), slen = strlen (suffix);
  char * base = strdup (name);
  if (len > slen && !strcmp (name + len - slen, suffix))
    base[len - slen] = '\0';
  
  vector v = new face vector;
  int d = 0;
  scalar out = {INT_MAX};
  foreach_dimension() {
    char cname[strlen(base) + 3];
    sprintf (cname, "%s.%c", base, "xyz"[d]);
    dump_rename_scalar (v.x, cname);
    if (d == axis)
      out = v.x;
    d++;
  }
  free (base);
  return out;
}

static scalar dump_create_field (DumpField * f)
{
  if (f->kind == dump_field_cell) {
    scalar s = new scalar;
    dump_rename_scalar (s, f->name);
    return s;
  }
  if (f->kind == dump_field_vertex) {
    scalar s = new vertex scalar;
    dump_rename_scalar (s, f->name);
    return s;
  }
  return dump_new_face_component (f->name, f->axis);
}

static bool dump_compatible_field (DumpField * f, scalar s)
{
  if (s.i == INT_MAX)
    return true;
  int kind = dump_field_kind (s);
  return kind == f->kind &&
    (kind != dump_field_face || dump_field_axis (s) == f->axis);
}

static scalar dump_restore_scalar (DumpField * f, scalar * slist, bool restore_all)
{
  scalar found = {INT_MAX};
  for (scalar s in slist)
    if (s.name && !strcmp (s.name, f->name)) {
      found = s; break;
    }
  if (found.i == INT_MAX && restore_all)
    found = dump_find_scalar (f->name);
  if (found.i == INT_MAX && restore_all)
    found = dump_create_field (f);
  if (!dump_compatible_field (f, found)) {
    fprintf (ferr, "restore(): error: field '%s' has incompatible location\n",
	     f->name);
    exit (1);
  }
  return found;
}

static DumpField * dump_read_header (FILE * fp, int * nfields)
{
  if (fread (nfields, sizeof(int), 1, fp) < 1) {
    fprintf (ferr, "restore(): error: expecting nfields\n");
    exit (1);
  }
  DumpField * fields = *nfields ?
    (DumpField *) calloc (*nfields, sizeof(DumpField)) : NULL;
  for (int i = 0; i < *nfields; i++) {
    unsigned len;
    if (fread (&len, sizeof(unsigned), 1, fp) < 1) {
      fprintf (ferr, "restore(): error: expecting len\n");
      exit (1);
    }
    fields[i].name = (char *) malloc (len + 1);
    if (fread (fields[i].name, sizeof(char), len, fp) < len) {
      fprintf (ferr, "restore(): error: expecting field name\n");
      exit (1);
    }
    fields[i].name[len] = '\0';
    if (fread (&fields[i].kind, sizeof(int), 1, fp) < 1 ||
	fread (&fields[i].axis, sizeof(int), 1, fp) < 1 ||
	fread (&fields[i].count, sizeof(int), 1, fp) < 1) {
      fprintf (ferr, "restore(): error: expecting field descriptor\n");
      exit (1);
    }
    if (fields[i].kind < dump_field_cell ||
	fields[i].kind > dump_field_vertex ||
	fields[i].count != dump_field_count (fields[i].kind)) {
      fprintf (ferr, "restore(): error: invalid count for field '%s'\n",
	       fields[i].name);
      exit (1);
    }
  }
  return fields;
}

#if !_MPI
trace
void dump (const char * file = "dump",
	   scalar * list = all,
	   FILE * fp = NULL,
	   bool unbuffered = false,
	   bool zero = true)
{
  char * name = NULL;
  if (!fp) {
    name = (char *) malloc (strlen(file) + 2);
    strcpy (name, file);
    if (!unbuffered)
      strcat (name, "~");
    if ((fp = fopen (name, "w")) == NULL) {
      perror (name);
      exit (1);
    }
  }
  assert (fp);
  
  scalar * dlist = dump_list (list, zero);
  scalar size[];
  int nfields;
  long record_len;
  DumpField * fields = dump_fields (dlist, &nfields, &record_len);
  scalar * slist = list_concat ({size}, dlist); free (dlist);
  struct DumpHeader header = { t, dt, dt_previous, record_len, iter, depth(), npe(),
			       dump_version };
  int npe = 1;
  foreach_dimension() {
    header.n.x = Dimensions.x;
    npe *= header.n.x;
  }
  header.npe = npe;
  dump_header (fp, &header, fields, nfields);
  
  subtree_size (size, false);
#if _GPU
  for (scalar s in slist)
    s.stencil.io |= s_input;
  gpu_cpu_sync (slist, GPU_READ, __FILE__, LINENO);
#endif // _GPU
  foreach_cell() {
    unsigned flags = is_leaf(cell) ? leaf : 0;
    if (fwrite (&flags, sizeof(unsigned), 1, fp) < 1) {
      perror ("dump(): error while writing flags");
      exit (1);
    }
    dump_write_double (fp, size[]);
    for (int i = 0; i < nfields; i++)
      dump_write_item (point, fp, fields[i].index,
			fields[i].kind, fields[i].axis);
    if (is_leaf(cell))
      continue;
  }
  
  free (slist);
  dump_fields_free (fields, nfields);
  if (file) {
    fclose (fp);
    if (!unbuffered)
      rename (name, file);
    free (name);
  }
}
#else // _MPI
trace
void dump (const char * file = "dump",
	   scalar * list = all,
	   FILE * fp = NULL,
	   bool unbuffered = false,
	   bool zero = true)
{
  if (fp != NULL || file == NULL) {
    fprintf (ferr, "dump(): must specify a file name when using MPI\n");
    exit(1);
  }

  char name[strlen(file) + 2];
  strcpy (name, file);
  if (!unbuffered)
    strcat (name, "~");
  FILE * fh = NULL;
  if (pid() == 0) {
    fh = fopen (name, "w");
    if (fh == NULL) {
      perror (name);
      exit (1);
    }
  }

  scalar * dlist = dump_list (list, zero);
  scalar size[];
  int nfields;
  long record_len;
  DumpField * fields = dump_fields (dlist, &nfields, &record_len);
  scalar * slist = list_concat ({size}, dlist); free (dlist);
  struct DumpHeader header = { t, dt, dt_previous, record_len, iter, depth(), npe(),
			       dump_version };
  foreach_dimension()
    header.n.x = Dimensions.x;
  
#if MULTIGRID_MPI
  MPI_Barrier (MPI_COMM_WORLD);
#endif

  if (pid() == 0) {
    dump_header (fh, &header, fields, nfields);
    fflush (fh);
  }
  
  MPI_Barrier (MPI_COMM_WORLD);

  if (pid() != 0) {
    fh = fopen (name, "r+");
    if (fh == NULL) {
      perror (name);
      exit (1);
    }
  }

  scalar index = {-1};
  
  index = new scalar;
  z_indexing (index, false);
  long cell_size = sizeof(unsigned) + header.len*sizeof(double);
  long sizeofheader = dump_header_size (fields, nfields);
  long pos = pid() ? 0 : sizeofheader;
  
  subtree_size (size, false);
  
  foreach_cell() {
    // fixme: this won't work when combining MPI and mask()
    if (is_local(cell)) {
      long offset = sizeofheader + index[]*cell_size;
      if (pos != offset) {
	fseek (fh, offset, SEEK_SET);
	pos = offset;
      }
      unsigned flags = is_leaf(cell) ? leaf : 0;
      fwrite (&flags, 1, sizeof(unsigned), fh);
      double val = size[];
      fwrite (&val, 1, sizeof(double), fh);
      for (int i = 0; i < nfields; i++)
	dump_write_item (point, fh, fields[i].index,
			  fields[i].kind, fields[i].axis);
      pos += cell_size;
    }
    if (is_leaf(cell))
      continue;
  }

  delete ({index});
  
  free (slist);
  dump_fields_free (fields, nfields);
  fclose (fh);
  if (!unbuffered && pid() == 0)
    rename (name, file);
}
#endif // _MPI

trace
bool restore (const char * file = "dump",
	      scalar * list = NULL,
	      FILE * fp = NULL)
{
  if (!fp && (fp = fopen (file, "r")) == NULL)
    return false;
  assert (fp);

  struct DumpHeader header = {0};
  if (fread (&header, sizeof(header), 1, fp) < 1) {
    fprintf (ferr, "restore(): error: expecting header\n");
    exit (1);
  }

#if TREE
  init_grid (1);
  foreach_cell() {
    cell.pid = pid();
    cell.flags |= active;
  }
  tree->dirty = true;
#else // multigrid
#if MULTIGRID_MPI
  if (header.npe != npe()) {
    fprintf (ferr,
	     "restore(): error: the number of processes don't match:"
	     " %d != %d\n",
	     header.npe, npe());
    exit (1);
  }
#endif // MULTIGRID_MPI
  dimensions (header.n.x, header.n.y, header.n.z);
  double n = header.n.x;
  int depth = header.depth;
  while (n > 1)
    depth++, n /= 2;
  init_grid (1 << depth);
#endif // multigrid

  if (header.version != dump_version) {
    fprintf (ferr,
	     "restore(): error: file version mismatch: "
	     "%d (file) != %d (code)\n",
	     header.version, dump_version);
    exit (1);
  }
  dt = header.dt;
  dt_previous = header.dt_previous;
  
  bool restore_all = (list == all);
  scalar * slist = dump_list (list ? list : all, true);
  int nfields;
  DumpField * fields = dump_read_header (fp, &nfields);
  long record_len = 1;
  scalar * restored = NULL;
  for (int i = 0; i < nfields; i++) {
    fields[i].index = dump_restore_scalar (&fields[i], slist, restore_all).i;
    record_len += fields[i].count;
    if (fields[i].index != INT_MAX)
      restored = list_add (restored, (scalar){fields[i].index});
  }
  free (slist);
  if (record_len != header.len) {
    fprintf (ferr,
	     "restore(): error: record length mismatch: "
	     "%ld (descriptors) != %ld (header)\n",
	     record_len, header.len);
    exit (1);
  }
  
  double o[4];
  if (fread (o, sizeof(double), 4, fp) < 4) {
    fprintf (ferr, "restore(): error: expecting coordinates\n");
    exit (1);
  }
  origin (o[0], o[1], o[2]);
  size (o[3]);

#if MULTIGRID_MPI
  long cell_size = sizeof(unsigned) + header.len*sizeof(double);
  long offset = pid()*((1L << dimension*(header.depth + 1)) - 1)/
    ((1L << dimension) - 1)*cell_size;
  if (fseek (fp, offset, SEEK_CUR) < 0) {
    perror ("restore(): error while seeking");
    exit (1);
  }
#endif // MULTIGRID_MPI
  
  scalar * listm = is_constant(cm) ? NULL : (scalar *){fm};
#if TREE && _MPI
  restore_mpi (fp, fields, nfields, header.len);
#else // ! (TREE && _MPI)
#if !_MPI
  int rootlevel = 0;
#endif
#if TREE
  foreach_dimension()
    while ((1 << rootlevel) < header.n.x)
      rootlevel++;
  if (rootlevel > 0)
    init_grid (1 << rootlevel);
#endif // TREE
#if _MPI  
  foreach_cell() {
#else
  foreach_cell_restore (header.n, rootlevel) {
#endif
    unsigned flags;
    if (fread (&flags, sizeof(unsigned), 1, fp) != 1) {
      fprintf (ferr, "restore(): error: expecting 'flags'\n");
      exit (1);
    }
    // skip subtree size
    fseek (fp, sizeof(double), SEEK_CUR);
    for (int i = 0; i < nfields; i++)
      dump_read_item (point, fp, fields[i].index, fields[i].kind,
		       fields[i].axis, fields[i].count);
    if (!(flags & leaf) && is_leaf(cell))
      refine_cell (point, listm, 0, NULL);
    if (is_leaf(cell))
      continue;
  }
#if _GPU
  for (scalar s in restored)
    if (s.i != INT_MAX)
      s.gpu.stored = 1; // stored on CPU
#endif // _GPU
  for (scalar s in all)
    set_dirty_stencil (s);
#endif // ! (TREE && _MPI)
  
  scalar * other = NULL;
  for (scalar s in all)
    if (!list_lookup (restored, s) && !list_lookup (listm, s))
      other = list_append (other, s);
  reset (other, 0.);
  free (other);
  
  free (restored);
  dump_fields_free (fields, nfields);
  if (file)
    fclose (fp);

  // the events are advanced to catch up with the time  
  while (iter < header.i && events (false))
    iter = inext;
  events (false);
  while (t < header.t && events (false))
    t = tnext;
  t = header.t;
  events (false);
  
  return true;
}

#endif // MULTIGRID

#if _GPU && !_CUDA
# include "grid/gpu/output.h"
#endif
