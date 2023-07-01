
/* ======================================================================
                 3D test generator, David Pisinger and Jens Egeblad, 2006
   ====================================================================== 
*/

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#define MAXITEMS       1000

typedef int           boolean; /* logical variable      */
typedef int           ntype;   /* number of states,bins */
typedef int           itype;   /* can hold up to H,W,D  */
typedef long          stype;   /* can hold up to H*W*D  */
typedef double        ptype;   /* profit type (float)   */

/* item record */
typedef struct irec {
  ntype    no;           /* item number */
  itype    w;            /* item x-size */
  itype    h;            /* item y-size */
  itype    d; 		/* item z-size */
  double    p;            /* profit of item */
  itype    x;            /* optimal x-position (returned) */
  itype    y;            /* optimal y-position (returned) */
  boolean  k;            /* is the item chosen (returned) */
  ntype    bin;          /* bin number (returned) */
  ntype    btype;        /* bin type (returned) */
  stype    volume;         /* volume of item (used internally) */
  struct irec *ref;      /* pointer (used internally) */
} item;

/* ======================================================================
                                   macros
   ====================================================================== */

#define srand(x)     srand48x(x)
#define randm(x)     (lrand48x() % (long) (x))
#define rint(a,b)    (randm((b)-(a)+1) + (a))
#define TRUE  1           /* logical variables */
#define FALSE 0

unsigned long _h48, _l48;

void srand48x(long s)
{
  _h48 = s;
  _l48 = 0x330E;
}

long lrand48x(void)
{
  _h48 = (_h48 * 0xDEECE66D) + (_l48 * 0x5DEEC);
  _l48 = _l48 * 0xE66D + 0xB;
  _h48 = _h48 + (_l48 >> 16);
  _l48 = _l48 & 0xFFFF;
  return (_h48 >> 1);
}



/* ======================================================================
				 type declarations
   ====================================================================== */

typedef int (*funcptr) (const void *, const void *);


/* **********************************************************************
   **********************************************************************
			     Small procedures
   **********************************************************************
   ********************************************************************** */

void cap(int *toCap, int min, int max)
{
	if (*toCap < min) { *toCap = min; }
	if (*toCap > max) { *toCap = max; }
}

/* ======================================================================
				randomtype
   ====================================================================== */

void randomtype(item *i, int W, int H, int D, int type)
{
  itype w, h, d;

  switch (type) {
	// Flat
        case  0: w = rint(W/2,W);     h = rint(H/2,H); d = rint(D/4,3*D/5); break;
	// Long
	case  1: w = rint(1, 2*W/3);   h = rint(1,2*H/3);   d = rint(D/2,D); break;
	// Cubes
        case  2: w = rint(1, W);      h = w; d = w; break;
	// Diverse
	case  3: w = rint(1, W/2);    h = rint(1, H/2); d = rint(1, D/2); break;
	// Uniform
	case  4: w = rint(W/2,W);   h = rint(H/2, H); d= rint(D/2, D); break;
  }

  cap(&w, 1, W);
  cap(&h, 1, H);
  cap(&d, 1, D);
  
  i->w = w; i->h = h; i->d = d; i->volume = i->w * i->h * i->d;
  i->x = 0; i->y = 0; i->k = 0; 

  i->p = i->volume + 200;     
}


/* ======================================================================
				maketest
   ====================================================================== */


void makebenchmark(item *items, int n, int type, int bintype, int clustered)
{
	// Modulus will be used to index in cluster or random. 
	// in clustered we reuse the 5 first items.
	int modulus;
	double totalvolume;
	double binvolume;
	double W, H, D;
	int i;
	int w, h, d;
	char filename[200];
	FILE *f;
	unsigned char typestring[][80] = {"F", "L", "C", "U", "D"};

	totalvolume = 0.0;
	modulus = clustered ? 5 : n;
	// Determine the area of the n items.
	for (i = 0; i < n; ++i) {
		totalvolume += (double)items[i % modulus].volume;
	}
	// Ok, depending on the bintype the bin area should be
	// 0. 50 % of total area. 1: 90 % of total bin area.
	binvolume = (bintype ? 0.90 : 0.50) * totalvolume;
	
	// Ok. Difficult bit. Determine height and width.
	// W * H = binarea, H = 2 * W => 2 * W^2 = area => W = sqrt(area/2)
	W = pow(binvolume / 2.0, 0.33333333333333333);
	H = W;
	D = 2*W;
	// filename ep_[n]_[type]_[c/r]_[025/075]

	// Make sure the rectangles can actually fit in the bin.
	for (i = 0; i < n; ++i) {
		w = items[i % modulus].w;
		h = items[i % modulus].h;
		d = items[i % modulus].h;
		if (W < w ) { W = w; }
		if (H < h ) { H = h; }
		if (D < d ) { D = d; }
	}

	
	sprintf(filename, "ep3d-%i-%s-%s-%s.3kp", n, typestring[type], clustered ? "C" : "R", bintype ? "90" : "50");
	f = fopen(filename,"w+");
	fprintf(f, "dim, %i, %i, %i\n", (int)W, (int)H, (int)D);
	for (i = 0; i < n; ++i) {	
		// every rectangle only has a count of 1... 
		fprintf(f, "box, %i, %i, %i, %i, %i, 1\n", i, (int)items[i % modulus].w, (int)items[i % modulus].h, (int)items[i % modulus].d, (int)(items[i  % modulus].p));
	}
	fclose(f);
}

int main(int argc, char **argv)
{
	int j, type;
	// Number of items in the instance -1 is sentinel
	int const maxn = 200;
	int const BW = 100;
	int const BH = 100;
	int const BD = 100;

	int const n[] = {20, 40, 60, -1}; 
	
	// initialize random generator
	srand48x(0);
	
	item  * items;
	
	for (type = 0; type < 5; ++type) {
		// Generate the instance base values
	        items = (item*)malloc( maxn * sizeof(item) );
		
		for (j = 0; j < maxn; ++j ) {
			randomtype(&items[j], BW, BH, BD, type);
		}
		
		// Now we need to output for each for each n
		for (j = 0; n[j] != -1; ++j) {
			// Cluster and random 
			// And two different bin sizes
			// random and small bin
			makebenchmark(items, n[j], type, 0, 0);
			// random and big bin
			makebenchmark(items, n[j], type, 1, 0);
			// clustered and small bin
			makebenchmark(items, n[j], type, 0, 1);
			// clustered and big bin
			makebenchmark(items, n[j], type, 1, 1);
		}
		free(items);
	}
	
	return 0;
}


