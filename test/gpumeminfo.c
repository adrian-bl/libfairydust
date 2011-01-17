/*
 * (C) 2010 Adrian Ulrich
 *
 * Display memory information via cuda-driver api
 *
*/
#include <stdio.h>
#include <cuda.h>

void xAbort (char *txt) {
	printf("FATAL ERROR: %s\n", txt);
	abort();
}

int main() {
	#define DEVNAME_SIZE 255
	CUdevice dev;
	CUdevice fakedev;
	CUcontext ctx;
	char device_name[DEVNAME_SIZE] = {0};
	unsigned int mem_free, mem_total;
	int devcount;
	int i;
	
	if( cuInit(0) != CUDA_SUCCESS )
		xAbort("cuInit failed!\n");
	
	cuDeviceGetCount(&devcount);
	printf(">> found %d cuda device(s)\n", devcount);
	
	for(i=0;i<devcount;i++) {
		printf("# device %2d: ",i);
		cuDeviceGet(&dev,i);
		cuDeviceGetName(device_name, DEVNAME_SIZE, dev);
		cuCtxCreate(&ctx, 0, dev);
		
		cuCtxGetDevice(&fakedev);
		
		printf(">> %d == %d\n", dev, fakedev);
		
		if( cuMemGetInfo(&mem_free, &mem_total) != CUDA_SUCCESS ) {
			printf("!! failed to get memory information\n");
		}
		else {
			printf("cudev=%d, name=%s, total=%4dMiB, free=%4dMiB\n", dev, device_name, mem_total/1024/1024, mem_free/1024/1024);
		}
		cuCtxDetach(ctx);
	}
	
}
