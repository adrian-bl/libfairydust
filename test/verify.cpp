#include <stdio.h>
#include <cuda.h>
#include <cuda_runtime.h>
#include <driver_types.h>
#include <assert.h>
#include <dlfcn.h>
void test_cudaRT() {
	int result = 0;
	int devcount=0;
	int i=0;
	cudaDeviceProp deviceProp;
	printf("===== cudaRT API test starts =====\n");
	
	result = cudaGetDeviceCount(&devcount);
	assert(result==0);
	
	printf("DeviceCount       : %d\n", devcount);
	
	for(i=0;i<devcount;i++) {
		printf("setDevice         : %d\n", i);
		result = cudaSetDevice(i);
		assert(result==0);
	}
	
	for(i=0;i<devcount;i++) {
		result = cudaGetDeviceProperties(&deviceProp,i);
		assert(result==0);
		printf("properties of #%2d : ",i);
		printf("name=%s\n", deviceProp.name);
	}
	
	printf("===== cudaRT API test finished =====\n");
}


void test_cuAPI() {
	int result   = 0;
	int devcount = 0;
	char devname[256];
	CUdevice cudev;
	printf("===== cuAPI test starts =====\n");
	
	result = cuInit(0);
	assert(result==0);
	
	
	result = cuDeviceGetCount(&devcount);
	assert(result==0);
	printf("DeviceCount      : %d\n", devcount);
	
	result = cuDeviceGet(&cudev,devcount-1);
	assert(result==0);
	printf("Last Devhandle   : %d\n", cudev);
	
	result = cuDeviceGetName(devname,255,cudev);
	assert(result==0);
	printf("Name of last dev : %s\n", devname);
	
	
	printf("get unallocated  : ");
	result = cuDeviceGetName(devname,255,(cudev==0?1:0));
	printf("%d (should be non-zero in enforce mode!)\n",result);
	
	printf("====== cuAPI test finished =====\n\n");
}

int main() {
	
	void * (*nve_dl) ();
	nve_dl = (void *(*) ()) dlsym(RTLD_NEXT, "nve_version");
	
	if(nve_dl == NULL) {
		printf(">>> RUNNING WITHOUT ENFORCE\n");
	}
	else {
		printf(">>> RUNNING WITH ENFORCE %d\n", nve_dl());
	}
	
	
	test_cudaRT();
	printf("\n\n\n");
	test_cuAPI();
}

