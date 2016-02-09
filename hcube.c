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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>

int hcube_fd[32];
int hcube_id;
int hcube_dim;

static int
fdfork(int fd, int post)
{
	static int seq;
	char path[64];
	char buf[64];
	int newfd;
	int len;
	newfd = -1;
	if(post){
		len = snprintf(path, sizeof path, "hcube.%d.%d", getpid(), seq++);
		mkfifo(path, 0666);
		if(write(fd, path, len) != len){
			fprintf(stderr, "write '%s': %s\n", path, strerror(errno));
			goto err_out;
		}
		newfd = open(path, O_RDWR);
		if(newfd == -1){
			fprintf(stderr, "open '%s': %s\n", path, strerror(errno));
			goto err_out;
		}
		len = read(fd, buf, sizeof buf-1);
		if(len <= 0){
			fprintf(stderr, "read: %s\n", strerror(errno));
			goto err_out;
		}
		buf[len] = '\0';
		if(strcmp(path, buf)){
			fprintf(stderr, "read: got '%s' wanted '%s'\n", buf, path);
			goto err_out;
		}
		unlink(path);
		return newfd;
	} else {
		len = read(fd, path, sizeof path-1);
		if(len <= 0){
			fprintf(stderr, "read: %s\n", strerror(errno));
			goto err_out;
		}
		path[len] = '\0';
		newfd = open(path, O_RDWR);
		if(newfd == -1)
			goto err_out;
		if(write(fd, path, len) != len)
			goto err_out;
		return newfd;
	}
err_out:
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


int
initcube(int dim)
{
	int i, id;

	hcube_id = 0;
	hcube_dim = dim;
	for(i = 0; i < dim; i++)
		hcube_id = hyperfork(hcube_fd, hcube_id, i);
	return hcube_id;
}

int
endcube(void)
{
	int i;
	for(i = hcube_dim-1; i >= 0; i--){
		if((hcube_id & (1<<i)) != 0)
			break;
		waitpid(-1, NULL, 0);
	}
	exit(0);
}

