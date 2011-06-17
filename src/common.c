/*
 * (C) 2010-2011 Adrian Ulrich / ETHZ
 *
 * Licensed under the terms of `The Artistic License 2.0'
 * See: ../artistic-2_0.txt or http://www.perlfoundation.org/attachment/legal/artistic-2_0.txt
 *
 */

#include "common.h"

/*
 * Printout build informations
 */
void __fdust_spam() {
	printf("%s starting up - %s\n", __FILE__, RELINFO);
	printf("%s release git-%d, compiled with gcc-%s at %d. mode=%s\n",        __FILE__, 100+_GIT_LOG_COUNT, __VERSION__, _COMPILED_AT, __fdust_mode());
}

/*
 * Read our ENV or contact fairyd
 */
void lock_fdust_devices(int *rdevs, int opmode) {
	char *env;                     /* FDUST_ALLOCATE env-pointer */
	char *scratch;                 /* allocation-info to parse   */
	int i, x, sock;                /* scratch stuff              */
	struct sockaddr_in f_addr;     /* fairyd addr                */
	char inchar[1];                /* response buffer from fairyd*/
	int ngpu = 0;                  /* number of detected GPUs    */
	DPRINT("allocating devices for pid %d\n", getpid());
	
	switch(opmode) {
		case FDUST_MODE_NVIDIA:
			ngpu = _get_number_of_nvidia_devices();
			break;
		case FDUST_MODE_ATI:
			ngpu = _get_number_of_ati_devices();
			break;
		default:
			break; // will panic
	}
	
	/* Fill rdevs with END-Magic (= mark as inited) */
	assert(rdevs[0] == FDUST_RSV_NINIT);
	memset(rdevs, FDUST_RSV_END, sizeof(int)*MAX_GPUCOUNT);
	assert(ngpu > 0);
	
	
	if( (env = getenv(FDUST_ALLOCATE)) != NULL) {
		/* env was set but we cannot modify it: copy into scratch */
		if( (scratch = malloc(strlen(env)+1)) != NULL ) {
			memcpy(scratch,env,strlen(env)+1);
		}
		else {
			abort();
		}
	}
	else {
		/* must ask fairyd */
		sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		assert(sock >= 0);
		memset(&f_addr, 0, sizeof(f_addr));
		f_addr.sin_family = AF_INET;
		f_addr.sin_port   = htons(NVD_PORT);
		
		x = inet_pton(AF_INET, NVD_HOST, &f_addr.sin_addr);
		assert(x == 1);
		
		if(connect(sock, (const struct sockaddr *)&f_addr, sizeof(f_addr)) < 0) {
			fprintf(stderr, "connection to fairyd failed: aborting\n");
			exit(1);
		}
		
		/* send allocate request */
		asprintf(&scratch, "allocate %d\r\n", getpid());
		x = send(sock, scratch, strlen(scratch), 0);
		assert(x >= 0);
		free(scratch);
		
		/* create an empty string */
		if( (scratch = malloc(1)) == NULL )
			abort();
		scratch[0] = '\0';
		
		while( recv(sock, &inchar, 1, 0) == 1 ) {
			if(*inchar < 32) {
				break;
			}
			else {
				if(realloc(scratch,1) == NULL)
					abort();
				
				scratch[strlen(scratch)+1] = '\0';
				scratch[strlen(scratch)]   = *inchar;
			}
		}
		close(sock);
		DPRINT("fairyd reported: *%s*\n", scratch);
	}
	
	/* scratch should now be a string such as '0 3 6\0' */
	assert(scratch != NULL);
	
	i = 0; /* first virtual device number */
	x = 1; /* do atoi() on next run       */
	
	/* special case -> allocate everything */
	if(scratch[0] == '@') {
		scratch[0] = '\0'; /* skip while loop */
		for(i=0;i<ngpu;i++)
			rdevs[i] = i;
		DPRINT("allocated all (%d) devices as requested by environment settings\n",ngpu);
	}
	
	while( scratch[0] != '\0' ) {
		if(scratch[0] >= '0' && scratch[0] <= '9') {
			if(x) {
				if(i < ngpu && atoi(scratch) < ngpu) {
					rdevs[i++] = atoi(scratch);
				}
				x=0;
			}
		}
		else if(x==0) {
			x=1;
		}
		
		memmove(scratch, scratch+1, strlen(scratch)); 
		memset(scratch+strlen(scratch),0,1);          
	}
	
	
	/* cleanup */
	free(scratch);
	assert(rdevs[0] != FDUST_RSV_END); /* die if we ended up with 0 allocated devices */
	for(i=0;i<MAX_GPUCOUNT;i++) {
		if(rdevs[i] == FDUST_RSV_END)
			break;
		DPRINT("rdevs[fake_id=%d] = physical_id=%d\n", i, rdevs[i]);
	}
}


int _get_number_of_nvidia_devices() {
	return _get_number_of_sysfs_devices_whoa_this_function_name_is_too_long("/sys/bus/pci/drivers/nvidia");
}

int _get_number_of_ati_devices() {
	return _get_number_of_sysfs_devices_whoa_this_function_name_is_too_long("/sys/bus/pci/drivers/fglrx_pci");
}

int _get_number_of_sysfs_devices_whoa_this_function_name_is_too_long(char *path) {
	
	DIR           *dfh;
	struct dirent *dent;
	int           ngpu = 0;
	
	dfh = opendir(path);
	if(dfh != NULL) {
		for(;;) {
			dent = readdir(dfh);
			if(dent == NULL)
				break;
			
			if(dent->d_type == DT_LNK && strlen(dent->d_name) == 12 && strncmp("0000:", dent->d_name, 5)==0)
				ngpu++;
		}
		closedir(dfh);
	}
	DPRINT("> %s: returning %d\n", __func__, ngpu);
	return ngpu;
}
