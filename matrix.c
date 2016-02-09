/*
 *	Copyright (c) 2015 Aki Nyrhinen
 *
 *	Permission is hereby granted, free of charge, to any person obtaining a copy
 *	of this software and associated documentation files (the "Software"), to deal
 *	in the Software without restriction, including without limitation the rights
 *	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *	copies of the Software, and to permit persons to whom the Software is
 *	furnished to do so, subject to the following conditions:
 *
 *	The above copyright notice and this permission notice shall be included in
 *	all copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 *	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *	THE SOFTWARE.
 */
#include "os.h"
#include "cube.h"

enum {
	Ndim = 0,
	N = 1024,
};

double *m;


static void
swap(double *p, double *q, int n)
{
	double t;
	int i;
	for(i = 0; i < n; i++){
		t = p[i];
		p[i] = q[i];
		q[i] = t;
	}
}

void
dumpmatrix(double *m, int ncols, int nrows)
{
	int i, j;
	for(i = 0; i < nrows; i++){
		printf("%2d:", i);
		for(j = 0; j < ncols; j++){
			printf(" %5.2f", m[i*ncols+j]);
		}
		printf("\n");
	}
	printf("\n");
}

/*
 *	this gauss-jordan elimination works on the principle that
 *	the matrix has been striped across processors by columns.
 *	full striping to utilize all processors
 *	elimination proceeds row by row.
 */
void
gaussjordan(double *m, int ncols, int nrows)
{
	double mults[nrows];
	int i, col, row;
	int pivrow;

	memset(mults, 0, sizeof mults);
	for(row = 0; row < nrows; row++){
		col = row >> cube_dim;
		if((row & cube_mask) != 0 && (row & cube_mask) > cube_id)
			col++;
		//if((row & cube_mask) > cube_id)
		//	col++;
		if((row & cube_mask) == cube_id){
			/* we own this diagonal element */
			double piv, maxval;
			/* .. so find the next pivot */
			piv = m[row*ncols+col];
			maxval = fabs(piv);
			pivrow = row;
			if(1 || maxval < 1e-3){
				for(i = row+1; i < nrows; i++){
					if(fabs(m[i*ncols+col]) > maxval){
						piv = m[i*ncols+col];
						maxval = fabs(piv);
						pivrow = i;
					}
				}
			}
			/* compute row multipliers */
			if(maxval < 1e-9)
				fprintf(stderr, "%d: row %d col %d tiny maxval %.20f\n", cube_id, row, col, maxval);
			for(i = 0; i < nrows; i++)
				mults[i] = -m[i*ncols+col]/piv;
		}

		cubebroadcast(
			row & cube_mask,
			(struct iovec[]){	
				{&pivrow, sizeof pivrow},
				{mults, sizeof mults}
			}, 2
		);

		swap(mults+pivrow, mults+row, 1);
		swap(m+pivrow*ncols, m+row*ncols, ncols);

		for(i = 0; i < nrows; i++){
			if(i != row){
				int j;
				for(j = col; j < ncols; j++)
					m[i*ncols+j] += mults[i]*m[row*ncols+j];
			}
		}
	}
}

int
main(int argc, char *argv[])
{
	int i, j;
	int nz, nnz;
	int dim = Ndim;
	int ncols, nrows;

	nrows = N;
	if(argc > 1)
		dim = strtol(argv[1], NULL, 10);
	if(argc > 2)
		nrows = strtol(argv[2], NULL, 10);

	if(dim < 0 || dim > 20){
		printf("crazy dim %d (want 0 <= dim <= 20)\n", dim);
		exit(1);
	}

	long seed;
	seed = getpid();
	initcube(dim);
	seed = (seed << dim) | cube_id;

	ncols = nrows >> cube_dim;
	if((nrows & cube_mask) != 0 && (nrows & cube_mask) > cube_id)
		ncols++;
	if(ncols <= 0){
		fprintf(stderr, "matrix %d is too small for cube dim %d\n", nrows, cube_dim);
		exit(1);
	}

	m = malloc(ncols * nrows * sizeof m[0]);

	srand48(seed | cube_id);
	for(i = 0; i < nrows*ncols; i++){
		do {
			m[i] = 1.0 - 2.0*drand48();
		} while(fabs(m[i]) < 1e-6);
	}

	gaussjordan(m, ncols, nrows);

	nz = 0;
	nnz = 0;
	for(i = 0; i < nrows; i++){
		for(j = 0; j < ncols; j++){
			if(fabs(m[i*ncols+j]) < 1e-10){
				nz++;
			} else {
				nnz++;
			}
		}
	}

	printf("%3d: %dx%d matrix, nnz %d nz %d\n", cube_id, ncols, nrows, nnz, nz);

	endcube();

	return 0;
}
