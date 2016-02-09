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


typedef struct Cubehdr Cubehdr;
typedef struct Cubeconn Cubeconn;

struct Cubeconn {
	uint32 una;
	uint32 seq;
};

struct Cubehdr {
	uint32 dst;
	uint32 src;
	uint32 fragsiz;
	uint32 seq;
	uint32 ack;
	uint32 flags;
};

enum {
	Flast = 1,
};

static const struct timeval cube_tick = { 0, 100*1000 };
static int cube_fd[32];
static Cubeconn *cube_conn;
uint32 cube_id;
uint32 cube_mask;
int cube_dim;

static void
cubetick(void)
{
	printf("%d: cubetick\n", cube_id);
	// retrasnmit una packets
}

static void
send_ack(uint32 dst)
{
	// generate and send an ack
	// what if we are on flow-controlled link and write would block?
	// what if the recipient is trying to write to us?
}

static void
process_ack(Cubehdr *hdr)
{
	// release una packet(s) up to, but not including, hdr->ack.
}

static int
fdfork(int fd, int post)
{
	static int seq;
	struct sockaddr_un sa;
	char buf[64];
	int newfd, lfd;
	int len;

	newfd = -1;
	lfd = -1;
	if(post){
		lfd = socket(PF_UNIX, SOCK_STREAM, 0);
		if(lfd == -1){
			fprintf(stderr, "socket: %s\n", strerror(errno));
			goto err_out;
		}
		memset(&sa, 0, sizeof sa);
		sa.sun_family = AF_UNIX;
		len = snprintf(sa.sun_path, sizeof sa.sun_path, "cubesock.%d.%d", getpid(), seq++);
		if(bind(lfd, (struct sockaddr *)&sa, sizeof sa) == -1){
			fprintf(stderr, "bind: %s\n", strerror(errno));
			goto err_out;
		}
		if(listen(lfd, 1) == -1){
			fprintf(stderr, "listen: %s\n", strerror(errno));
			goto err_out;
		}
		if(write(fd, sa.sun_path, len) != len){
			fprintf(stderr, "write '%s': %s\n", sa.sun_path, strerror(errno));
			goto err_out;
		}
		newfd = accept(lfd, NULL, NULL);

		if(newfd == -1){
			fprintf(stderr, "accept '%s': %s\n", sa.sun_path, strerror(errno));
			goto err_out;
		}

		len = read(fd, buf, len);
		if(len <= 0){
			fprintf(stderr, "read: %s\n", strerror(errno));
			goto err_out;
		}
		buf[len] = '\0';
		if(strcmp(sa.sun_path, buf)){
			fprintf(stderr, "read: got '%s' wanted '%s'\n", buf, sa.sun_path);
			goto err_out;
		}
		close(lfd);
		unlink(sa.sun_path);
	} else {

		newfd = socket(PF_UNIX, SOCK_STREAM, 0);
		memset(&sa, 0, sizeof sa);
		sa.sun_family = AF_UNIX;
		len = read(fd, sa.sun_path, sizeof sa.sun_path-1);
		if(len <= 0){
			fprintf(stderr, "read: %s\n", strerror(errno));
			goto err_out;
		}
		if(connect(newfd, (struct sockaddr*)&sa, sizeof sa) == -1){
			fprintf(stderr, "connect '%s': %s\n", sa.sun_path, strerror(errno));
			goto err_out;
		}
		if(write(fd, sa.sun_path, len) != len)
			goto err_out;
	}
#if 1
	if(setsockopt(newfd, SOL_SOCKET, SO_SNDTIMEO, &cube_tick, sizeof cube_tick) == -1)
		goto err_out;
	if(setsockopt(newfd, SOL_SOCKET, SO_RCVTIMEO, &cube_tick, sizeof cube_tick) == -1)
		goto err_out;
#endif
	return newfd;

err_out:
	if(lfd != -1)
		close(lfd);
	if(newfd != -1)
		close(newfd);
	fprintf(stderr, "fdfork: fail\n");
	return -1;
}

static int
hyperfork(int *fd, int id, int dim)
{
	int chfd[dim];
	int ch[2];
	int i;

	for(i = 0; i < dim; i++){
		chfd[i] = fdfork(fd[i], (id & (1<<i)) == 0);
		if(chfd[i] == -1){
			fprintf(stderr, "fdfork %d/%d bad\n", i, dim);
			return -1;
		}
	}
	socketpair(PF_LOCAL, SOCK_STREAM, 0, ch);
	switch(fork()){
	case -1:
		fprintf(stderr, "fork() %s:", strerror(errno));
		return -1;
	case 0:
		for(i = 0; i < dim; i++){
			close(fd[i]);
			fd[i] = chfd[i];
		}
		id |= 1 << dim;
		close(ch[0]);
		fd[dim] = ch[1];
		break;
	default:
		for(i = 0; i < dim; i++)
			close(chfd[i]);
		close(ch[1]);
		fd[dim] = ch[0];
		break;
	}
	return id;
}

static int
readvn(int dim, struct iovec *xiov, int niov)
{
	Cubehdr hdr;
	struct iovec tiov[niov+1], *iov;
	size_t nrd;
	uint32 totrd;
	uint32 allbut;
	int i;

	tiov[0] = (struct iovec){ &hdr, sizeof hdr };
	memcpy(tiov+1, xiov, niov * sizeof tiov[0]);
	niov += 1;

	allbut = 0;
	for(i = 0; i < niov-1; i++)
		allbut += tiov[i].iov_len;

	iov = tiov;
	totrd = 0;
	for(;;){
		nrd = readv(cube_fd[dim], iov, niov);
		if(nrd == (size_t)-1){
			if((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINPROGRESS)){
				cubetick();
				continue;
			}
		}
		totrd += nrd;
		while(niov > 0 && nrd >= iov->iov_len){
			nrd -= iov->iov_len;
			iov++;
			niov--;
		}
		if(niov == 0)
			break;
		iov->iov_base = (char*)iov->iov_base + nrd;
		iov->iov_len -= nrd;
//fprintf(stderr, "partial readv %d left %d\n", nrd, iov->iov_len);
	}

	/* out of order! */
	if(hdr.seq != cube_conn[hdr.src].una){
		printf("cube: out of order pkt, got %u wanted %u\n", hdr.seq, cube_conn[hdr.src].una);
	}
	if(hdr.fragsiz != totrd){
		printf("cube: wrong length, got %u pkt says %u\n", totrd, hdr.fragsiz);
	}
	if(hdr.dst != cube_id){
		printf("cube: packet has wrong dst, got %u but I am %u\n", hdr.dst, cube_id);
	}
	if(hdr.src != (cube_id ^ (1u<<dim))){
		printf("cube: packet has wrong src, got %u want %u\n", hdr.src, cube_id ^ (1u<<dim));
	}
	if((hdr.flags & Flast) == 0){
		printf("cube: not last packet, seq %u\n", hdr.seq);
	}
	cube_conn[hdr.src].una = hdr.seq+1;

	return totrd;
}

static int
writevn(int dim, struct iovec *xiov, int niov)
{
	Cubehdr hdr;
	struct iovec tiov[niov+1], *iov;
	size_t nwr;
	int i, totwr;

	tiov[0] = (struct iovec){ &hdr, sizeof hdr };
	memcpy(tiov+1, xiov, niov * sizeof tiov[0]);
	niov += 1;

	hdr.fragsiz = 0;
	for(i = 0; i < niov; i++)
		hdr.fragsiz += tiov[i].iov_len;
	hdr.dst = cube_id ^ (1u<<dim);
	hdr.src = cube_id;
	hdr.seq = cube_conn[hdr.dst].seq++;
	hdr.flags = Flast;

	iov = tiov;
	totwr = 0;
	for(;;){
		nwr = writev(cube_fd[dim], iov, niov);
		if(nwr == (size_t)-1){
			if((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINPROGRESS)){
				cubetick();
				continue;
			}
		}
		totwr += nwr;
		while(niov > 0 && nwr >= iov->iov_len){
			nwr -= iov->iov_len;
			iov++;
			niov--;
		}
		if(niov == 0)
			break;
		iov->iov_base = (char*)iov->iov_base + nwr;
		iov->iov_len -= nwr;
fprintf(stderr, "partial writev\n");

	}

	return totwr;
}

int
cubebroadcast(int srcid, struct iovec *iov, int niov)
{
	int virtid, dim, mask;
	int nrd, nwr;

	nrd = nwr = 0;
	virtid = srcid ^ cube_id;
	mask = cube_mask;
	for(dim = cube_dim-1; dim >= 0; dim--){
		mask &= ~(1<<dim);
		if((virtid & mask) == 0){
			if((virtid & (1<<dim)) == 0){
				nwr = writevn(dim, iov, niov);
			} else {
				nrd = readvn(dim, iov, niov);
			}
		}
	}
	return virtid == 0 ? nwr : nrd;
}

int
initcube(int dim)
{
	int i;

	cube_id = 0;
	cube_dim = dim;
	cube_mask = (1<<dim) - 1;
	cube_conn = malloc((1u<<dim) * sizeof cube_conn[0]);
	memset(cube_conn, 0, (1u<<dim) * sizeof cube_conn[0]);
	for(i = 0; i < dim; i++)
		cube_id = hyperfork(cube_fd, cube_id, i);

	cpu_set_t my_set;        /* Define your cpu_set bit mask. */
	CPU_ZERO(&my_set);       /* Initialize it all to 0, i.e. no CPUs selected. */

	int id = (cube_id & ~15) | ((cube_id&15)>>1) | ((cube_id&1)<<3);
	CPU_SET(id, &my_set);     /* set the bit that represents this core */
	sched_setaffinity(0, sizeof(cpu_set_t), &my_set); /* Set affinity of tihs process to */
       
	return cube_id;
}

int
endcube(void)
{
	int i;
	for(i = cube_dim-1; i >= 0; i--){
		if((cube_id & (1<<i)) != 0)
			break;
		waitpid(-1, NULL, 0);
	}
	exit(0);
}

