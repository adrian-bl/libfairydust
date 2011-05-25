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

#define TO_ALLOC 2048


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
	int i;
	
	newenv = malloc(TO_ALLOC);
	if(newenv == NULL)
		abort();
	
	newenv[0] = '\0'; /* make sure we start with a null-byte for strncat() */
	strncat(newenv, "CUDA_VISIBLE_DEVICES=", TO_ALLOC);
	
	lock_fdust_devices(rdevs, FDUST_MODE_NVIDIA); /* ask fairyd for devices. This will have AT LEAST one dev */
	for(i=0;i<MAX_GPUCOUNT;i++) {
		if(rdevs[i] == FDUST_RSV_END)
			break; /* -> got all */
		
		snprintf(newenv+strlen(newenv), (TO_ALLOC-strlen(newenv)), "%d,",rdevs[i]);
	}
	
	if(newenv[strlen(newenv)-1] == ',') {
		newenv[strlen(newenv)-1] = '\0';
	}
	else {
		RMSG("Shouldn't be here: panic!");
		abort();
	}
	
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
