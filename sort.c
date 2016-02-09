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

static int
cmp(const void *ap, const void *bp)
{
	int a, b;
	a = *(int *)ap;
	b = *(int *)bp;
	if(a < b)
		return -1;
	else if(a > b)
		return 1;
	return 0;
}

static void
swap(int *arr, int64 i, int64 j)
{
	int a, b;
	a = arr[i];
	b = arr[j];
	arr[i] = b;
	arr[j] = a;
}

static void
cmpswap(int *arr, int64 i, int64 j)
{
	int a, b;
	a = arr[i];
	b = arr[j];
	arr[i] = a < b ? a : b;
	arr[j] = a < b ? b : a;
}

static void
isort(int *arr, int64 n)
{
}

static void
merge(int *arr, int64 n, int up)
{
	int64 s = n/2, i, j;
	for(s = n / 2; s > 0; s /= 2)
		for(i = 0; i < n; i += s*2)
			for(j = 0; j < s; j++)
				cmpswap(arr, up ? i+j : i+j+s, up ? i+j+s : i+j);
}

int
main(int argc, char **argv)
{
	int *arr;
	int64 i, j, s, n;
	long seed;
	int64 start, end;

	seed = nsec() % 10000;

	n = 1024*1024;
	if(argc > 1){
		char *r;
		n = strtol(argv[1], &r, 10);
		if(r != NULL){
			switch(*r){
			case 'k': case 'K':
				n *= 1024;
				break;
			case 'm': case 'M':
				n *= 1024*1024;
				break;
			case 'g': case 'G':
				n *= 1024*1024*1024;
				break;
			}
		}
	}

	arr = malloc(n * sizeof arr[0]);
	memset(arr, 0, n * sizeof arr[0]);
	for(i = 0; i < n; i++)
		arr[i] = i;

	for(j = 0; j < 10; j++){
		srand48(seed);
		for(i = 0; i < n; i++)
			swap(arr, i, lrand48()%n);

		start = nsec();
		qsort(arr, n, sizeof arr[0], cmp);
		end = nsec();

		for(i = 1; i < n; i++)
			if(arr[i-1] != arr[i]-1)
				printf("quicksort %d %d\n",arr[i-1],arr[i]);
		printf("quicksort %.4f s, %.3f kkeys/s\n", ((end-start)*1e-9), (double)n / ((end-start)*1e-6));

		srand48(seed);
		for(i = 0; i < n; i++)
			swap(arr, i, lrand48()%n);

		start = nsec();
		for(s = 2; s <= n; s *= 2)
			for(i = 0; i < n; i += s)
				merge(arr+i, s, (i & s) == 0);
		end = nsec();

		for(i = 1; i < n; i++)
			if(arr[i-1] != arr[i]-1)
				printf("bitonsort %d %d\n",arr[i-1],arr[i]);
		printf("bitonsort %.4f s, %.3f kkeys/s\n", ((end-start)*1e-9), (double)n / ((end-start)*1e-6));
	}
	return 0;
}