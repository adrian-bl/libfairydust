/*
 * (C) 2010-2011 Adrian Ulrich / ETHZ
 *
 * Licensed under the terms of `The Artistic License 2.0'
 * See: ../artistic-2_0.txt or http://www.perlfoundation.org/attachment/legal/artistic-2_0.txt
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <dlfcn.h>

#define BUFFER_OVERFLOWS_ARE_COOL  5                                       /* 255,\0 */
#define TO_ALLOC                   MAX_GPUCOUNT*BUFFER_OVERFLOWS_ARE_COOL

void *(*libc_ioctl)() = NULL;  /* libc's version of ioctl      */
char *newenv = NULL;           /* pointer for new ENV          */

/*
 * Return 'modeid'
 */
const char *__fdust_mode() {
	return "cuvisible";
}


/*
 * Modify the current processes *ENV to include CUDA_VISIBLE_DEVICES
 */
void __fdust_cuvisible_init() {
	int rdevs[MAX_GPUCOUNT] = { FDUST_RSV_NINIT };
	char xxx[BUFFER_OVERFLOWS_ARE_COOL];
	int i;
	
	newenv = malloc(TO_ALLOC); /* This could be BIND8 code ;-) */
	if(newenv == NULL)
		abort();
	
	memset(newenv, 0, TO_ALLOC);             /* cleanup allocated memory...   */
	strcat(newenv, "CUDA_VISIBLE_DEVICES="); /* ..and set the 'magic' keyword */
	
	__fdust_lock_devices(rdevs);             /* ask fairyd for devices        */
	
	for(i=0;i<MAX_GPUCOUNT;i++) {            /* ..and convert the response into a string */
		if(rdevs[i] == FDUST_RSV_END)
			break;
		sprintf(xxx, "%d,",rdevs[i]);
		strcat(newenv, xxx);
	}
	
	newenv[strlen(newenv)-1] = '\0';        /* remove last ','. this code is NOT reached if devs==0 */
	i = putenv(newenv);
	
	if(i)
		abort();
}


/*
 * Hijack ioctl() calls and route first cuda-like call to __fdust_cuvisible_init()
 */
int ioctl(int fd, unsigned long rq, ...) {
	char *argp = NULL;
	
	va_list ap;
	va_start(ap, rq);
	argp = (char *)va_arg(ap, char *);
	va_end(ap);
	
	if(libc_ioctl == NULL) {
		libc_ioctl = dlsym(RTLD_NEXT, __func__);
	}
	
	if(argp) {
		if(newenv == NULL && rq == 0xc04846d2) { /* aah, the sweet sweet smell of cuda... */
			__fdust_cuvisible_init();
		}
		return libc_ioctl(fd,rq,argp);
	}
	else {
		return libc_ioctl(fd,rq);
	}
	
	/* not reached */
	return EINVAL;
}
