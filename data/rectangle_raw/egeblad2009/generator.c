
/* ======================================================================
                 2D test generator, David Pisinger and Jens Egeblad, 2006
   ====================================================================== */

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
  double    p;            /* profit of item */
  itype    x;            /* optimal x-position (returned) */
  itype    y;            /* optimal y-position (returned) */
  boolean  k;            /* is the item chosen (returned) */
  ntype    bin;          /* bin number (returned) */
  ntype    btype;        /* bin type (returned) */
  stype    area;         /* area of item (used internally) */
  struct irec *ref;      /* pointer (used internally) */
} item;

/* ======================================================================
                                   macros
   ====================================================================== */

#define srand(x)     srand48x(x)
#define randm(x)     (lrand48x() % (long long) (x))
#define rint(a,b)    (randm((b)-(a)+1) + (a))
#define TRUE  1           /* logical variables */
#define FALSE 0

unsigned long long _h48, _l48;

void srand48x(long long s)
{
  _h48 = s;
  _l48 = 0x330E;
}

long long lrand48x(void)
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

void randomtype(item *i, int W, int H, int type)
{
  itype w, h;

  switch (type) {
    case  0: w = rint(1,W/3);     h = rint(2*H/3,H); break;
	case  1: w = rint(2*W/3,W);   h = rint(1,H/3);   break;
//	case  2: w = rint(1, W);      h = w - 15 + rint(1,30); break;	
        case  2: w = rint(1, W);      h = w; break;
	case  3: w = rint(1, W/3);    h = rint(1, H/3); break;
	case  4: w = rint(2*W/3,W);   h = rint(2*H/3, H); break;
  }

  cap(&w, 1, W);
  cap(&h, 1, H);
  
  i->w = w; i->h = h; i->area = i->w * i->h;
  i->x = 0; i->y = 0; i->k = 0; 

  i->p = i->area + 20;     
}


/* ======================================================================
				maketest
   ====================================================================== */


void makebenchmark(item *items, int n, int type, int bintype, int clustered)
{
	// Modulus will be used to index in cluster or random. 
	// in clustered we reuse the 20 first items.
	int modulus;
	double totalarea;
	double binarea;
	double W, H;
	int i;
	int w, h;
	char filename[200];
	FILE *f;
	unsigned char typestring[][80] = {"T", "W", "S", "U", "D"};

	totalarea = 0.0;
	modulus = clustered ? 20 : n;
	// Determine the area of the n items.
	for (i = 0; i < n; ++i) {
		totalarea += (double)items[i % modulus].area;
	}
	// Ok, depending on the bintype the bin area should be
	// 0. 25 % of total area. 1: 75 % of total bin area.
	binarea = (bintype ? 0.75 : 0.25) * totalarea;
	
	// Ok. Difficult bit. Determine height and width.
	// W * H = binarea, H = 2 * W => 2 * W^2 = area => W = sqrt(area/2)
	W = sqrt(binarea / 2.0);
	H = 2 * W;
	// filename ep_[n]_[type]_[c/r]_[025/075]

	// Make sure the rectangles can actually fit in the bin.
	for (i = 0; i < n; ++i) {
		w = items[i % modulus].w;
		h = items[i % modulus].h;

		if (W < w ) { W = w; }
		if (H < h ) { H = h; }
	}

	
	sprintf(filename, "ep-%i-%s-%s-%s.2kp", n, typestring[type], clustered ? "C" : "R", bintype ? "75" : "25");
	f = fopen(filename,"w+");
	fprintf(f, "dim, %i, %i\n", (int)W, (int)H);
	for (i = 0; i < n; ++i) {	
		// every rectangle only has a count of 1... 
		fprintf(f, "rect, %i, %i, %i, %i, 1\n", i, (int)items[i % modulus].w, (int)items[i % modulus].h, (int)(items[i  % modulus].p));
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
	int const n[] = {30, 50, 100, 200, -1}; 
	
	// initialize random generator
	srand48x(0);
	
	item  * items;
	
	for (type = 0; type < 5; ++type) {
		// Generate the instance base values
	        items = (item*)malloc( maxn * sizeof(item) );
		
		for (j = 0; j < maxn; ++j ) {
			randomtype(&items[j], BW, BH, type);
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


