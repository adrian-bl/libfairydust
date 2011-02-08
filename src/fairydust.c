/*
 *
 *  Hijack some Cuda and OpenCL API-Calls
 *
 * (C) 2010-2011 Adrian Ulrich / ETHZ
 *
 * Licensed under the terms of `The Artistic License 2.0'
 * See: ../artistic-2_0.txt or http://www.perlfoundation.org/attachment/legal/artistic-2_0.txt
 *
 */

#include "fairydust.h"



/* java doesn't play well with RTLD_NEXT -> needs dlopen() */
static void *cuda_lib = NULL;
int cuda_set_device   = -1;



/**************************************************************************************
*  OpenCL part
***************************************************************************************/


/*
 * Emulate clGetDeviceIDs
*/
extern CL_API_ENTRY cl_int CL_API_CALL clGetDeviceIDs(cl_platform_id platform    , cl_device_type device_type,
                                                      cl_uint        num_entries , cl_device_id *devices,
                                                      cl_uint        *num_devices) {
	
	static void * (*nv_gdi) ();            /* libOpenCL's version of clGetDeviceIDs    */
	cl_uint internal_num_devices;          /* number of (all) hw-devices               */
	cl_device_id *internal_devices;        /* will hold the IDs of ALL devices         */
	cl_int nv_return, lock_cnt, i, foo;
	
	if(platform == NULL)
		RMSG(HINT_OCL_PLATFORM_NULL);
	
	/* init call to libOpenCL */
	if(!nv_gdi)
		nv_gdi = (void *(*) ()) dlsym(RTLD_NEXT, __func__);
	
	/* init libfairydust and get number of useable devices */
	__fdust_init();
	lock_cnt = _xxGetNumberOfLockedDevices();
	
	/* Get the number of physical devices */
	nv_return = nv_gdi(platform, device_type, NULL, NULL, &internal_num_devices);
	
	assert( nv_return == CL_SUCCESS );          /* this shouldn't fail in any case */
	assert( lock_cnt > 0 );                     /* could we lock anything ? */
	assert( internal_num_devices >= lock_cnt ); /* Did we lock too many devices (??!) */
	
	
	/* caller wants to know how many devices matched -> fixup the result */
	if(num_devices != NULL)
		*num_devices = lock_cnt;
	
	DPRINT("Hardware has %d physical devices, returning %d (num_devices=%p)\n", internal_num_devices, lock_cnt, num_devices);
	
	/* caller (also) want's to actually get a list of devices */
	if(num_entries > 0 && devices != NULL) {
		
		if( num_entries > lock_cnt) { /* Caller requested more devices than available, well: we don't care */
			num_entries = lock_cnt;
		}
		
		internal_devices = (cl_device_id *)malloc(internal_num_devices*sizeof(cl_device_id));
		if(!internal_devices)
			return CL_OUT_OF_HOST_MEMORY;
		
		nv_return = nv_gdi(platform, device_type, internal_num_devices, internal_devices, NULL);
		if(nv_return != CL_SUCCESS)
			return nv_return;
		
		DPRINT("We got %d devices, caller requested %d of them\n", internal_num_devices, num_entries);
		
		for(i=0;i<num_entries;i++) {
			foo = _xxGetPhysicalDevice(i);
			assert( foo >= 0 );
			ocl_ptrcache[i] = devices[i] = internal_devices[foo]; // fake info for caller
			DPRINT("physical_device=%d, fake_device=%d, pointer=%p, want=%d\n", foo, i, internal_devices[foo], num_entries);
		}
	}
	
	return nv_return;
}

/*
 * Emulate clGetDeviceInfo -> just add the fdust: tag to the name
*/
extern CL_API_ENTRY cl_int CL_API_CALL clGetDeviceInfo(cl_device_id device_id     , cl_device_info param_name,
                                                      size_t param_value_size    , void *param_value,
                                                      size_t *param_value_size_ret) {
	
	static void * (*nv_gdx) ();            /* libOpenCL's version of clGetDeviceInfo    */
	cl_int gdx_return;
	
	if(!nv_gdx)
		nv_gdx = (void *(*) ()) dlsym(RTLD_NEXT, __func__);
	
	__fdust_init();
	
	gdx_return = nv_gdx(device_id, param_name, param_value_size, param_value, param_value_size_ret);
	
	if(gdx_return == CL_SUCCESS && param_name == CL_DEVICE_NAME) {
		param_value_size_ret = NULL; // requests caller to ignore and was set to NULL in Cuda 3.2RC anyway...
		_xxAddDeviceMapping(param_value, param_value_size, _xxGetPhysicalDevice(_xxGetFakedevFromClPtr(device_id)), _xxGetFakedevFromClPtr(device_id));
	}
	return gdx_return;
}

/**************************************************************************************
*  CUDA part
***************************************************************************************/

/*
 *   Cuda provides a Runtime-API and a 'driver' API with almost the same syntax.
 *   ..but for some reason, we cannot use the RT-Api in while executing a Driver
 *   function (and vice versa). This is the reason why we have the __cuda_cruft*
 *   functions.
 *
 */


/************************************ CUDA RUNTIME API ************************************/

/*
 * Implements GetDeviceCount stuff:
 */
CUresult __cuda_cruft_GetDeviceCount(CUresult errcode, int *count) {
	int lock_cnt = -1;
	
	__fdust_init();
	
	if(errcode == CUDA_SUCCESS) {
		lock_cnt = _xxGetNumberOfLockedDevices();
		assert( lock_cnt > 0 );
		assert( *count >= lock_cnt );
		*count = lock_cnt;
	}
	
	return errcode;
}

/*
 * CURT version of cuGetDeviceCount
 */
extern __host__ cudaError_t CUDARTAPI cudaGetDeviceCount(int *count) {
	void * (*CURT_cgdc) ();
	
	__fdust_init();
	
	CURT_cgdc = (void *(*) ()) dlsym(RTLD_NEXT, __func__);
	return __cuda_cruft_GetDeviceCount( (CUresult)CURT_cgdc(count), count );
}


/*
 * Implements GetDeviceProperties
 */
extern __host__ cudaError_t CUDARTAPI cudaGetDeviceProperties(struct cudaDeviceProp *prop, int device) {
	void * (*CURT_cgdp) ();
	cudaError_t rval;
	
	__fdust_init();
	
	CURT_cgdp = (void *(*) ()) dlsym(RTLD_NEXT, __func__);
	
	rval = (cudaError_t)CURT_cgdp(prop, _xxGetPhysicalDevice(device));
	
	/* add fairydust description to devname if everything worked out */
	if(rval == cudaSuccess) {
		_xxAddDeviceMapping(&prop->name, CUDA_PROP_NAME_LEN, _xxGetPhysicalDevice(device), device);
	}
	
	return rval;
}

/*
 * Sets context to a specific device
 */
extern __host__ cudaError_t CUDARTAPI cudaSetDevice(int device) {
	void * (*CURT_csd) ();
	cudaError_t nvreturn;
	__fdust_init();
	
	CURT_csd = (void *(*) ()) dlsym(RTLD_NEXT, __func__);
	
	nvreturn = (cudaError_t)CURT_csd(_xxGetPhysicalDevice(device));
	
	if(nvreturn == CUDA_SUCCESS)
		cuda_set_device = device;
	
	return nvreturn;
}

/*
 * ??
 */
extern __host__ cudaError_t CUDARTAPI cudaGLSetGLDevice(int device) {
	void * (*CURT_cglsgld) ();
	cudaError_t nvreturn;
	__fdust_init();
	
	CURT_cglsgld = (void *(*) ()) dlsym(RTLD_NEXT, __func__);
	
	nvreturn = (cudaError_t)CURT_cglsgld(_xxGetPhysicalDevice(device));
	
	if(nvreturn == CUDA_SUCCESS)
		cuda_set_device = device;
	
	return nvreturn;
}

/*
 * Chooses a device based on properties -> does nothing in libfairydust (always sets device to fakedev#0)
 */
extern __host__ cudaError_t CUDARTAPI cudaChooseDevice(int *device, const struct cudaDeviceProp *prop) {
	RMSG("oh noes! call to cudaChooseDevice ignored, retuning first device");
	*device = 0;
	return CUDA_SUCCESS;
}

/*
 * noop - should be implemented some day
 */
extern __host__ cudaError_t CUDARTAPI cudaGetDevice(int *device) {
	
	if(cuda_set_device >= 0) {
		*device = cuda_set_device;
	}
	else {
		*device = 0;
		DPRINT("*WARNING* cudaGetDevice() called -> device was never initialized, returning %d",*device);
	}
	
	return CL_SUCCESS;
}

/*
 * imlements cudaSetValidDevices
 */
extern __host__ cudaError_t CUDARTAPI cudaSetValidDevices(int *device_arr, int len) {
	void * (*CURT_csvd) ();
	int i;
	cudaError_t nvreturn;
	
	__fdust_init();
	
	CURT_csvd = (void *(*) ()) dlsym(RTLD_NEXT, __func__);
	
	/* convert the virtual device id's into physical ids */
	for(i=0;i<len;i++) {
		device_arr[i] = _xxGetPhysicalDevice(device_arr[i]);
	}
	
	nvreturn = (cudaError_t)CURT_csvd(device_arr,len);
	
	/* ..and back (we do not care about the return code */
	for(i=0;i<len;i++) {
		device_arr[i] = _xxGetVirtualDevice(device_arr[i]);
	}
	
	return nvreturn;
}



/************************************ CUDA DRIVER API ************************************/

/*
 * Creates a new Ctx
 */
CUresult CUDAAPI cuCtxCreate(CUcontext *pctx, unsigned int flags, CUdevice dev ) {
	void * (*cu_ctxcreate) ();
	CUdevice hwdev;
	
	__fdust_init();
	
	cu_ctxcreate = __cu_dlsym(__func__);
	hwdev        = _xxGetPhysicalDevice(dev);
	DPRINT(" ***** routing virtual device %d to hardware device %d\n", dev, hwdev);
	return (CUresult)cu_ctxcreate(pctx, flags, hwdev);
}



/*
 * The obsoleted cuCtxCreate call. I have NO idea why nvidia renamed this to _v2 ...
 */
#ifndef cuCtxCreate
	#error "cuCtxCreate is not re-defined, but it should be on cuda >= 3.2"
#else
	#undef cuCtxCreate
	
	CUresult CUDAAPI cuCtxCreate(CUcontext *pctx, unsigned int flags, CUdevice dev ) {
		void * (*cu_ctxcreate) ();
		CUdevice hwdev;
		
		__fdust_init();
		
		cu_ctxcreate = __cu_dlsym("cuCtxCreate_v2");
		hwdev        = _xxGetPhysicalDevice(dev);
		RMSG("OBSOLETED API call!");
		return (CUresult)cu_ctxcreate(pctx, flags, hwdev);
	}
	
	// fixme: should check if this was true in the first place:
	#define cuCtxCreate cuCtxCreate_v2
	
#endif


/*
 * Returns the current device in Ctx
 */
CUresult CUDAAPI cuCtxGetDevice(CUdevice *device) {
	void * (*cu_ctxgetdev) ();
	CUresult nvreturn;
	CUdevice ctxdev;
	
	__fdust_init();
	
	cu_ctxgetdev = __cu_dlsym(__func__);
	
	nvreturn = (CUresult)cu_ctxgetdev(&ctxdev);
	
	if(nvreturn == CUDA_SUCCESS) {
		*device = _xxGetVirtualDevice(ctxdev);
	}
	
	return nvreturn;
}

/*
 * Returns the number of available devices
 */
CUresult CUDAAPI cuDeviceGetCount(int *count) {
	void * (*cu_px) ();
	
	__fdust_init();
	
	cu_px = __cu_dlsym(__func__);
	return __cuda_cruft_GetDeviceCount( (CUresult)cu_px(count), count );
}

/*
 * Returns a device handle for fakedev specified in 'ordinal'
 */
CUresult  CUDAAPI cuDeviceGet(CUdevice *device, int ordinal) {
	void * (*cu_px) ();
	CUresult nvreturn;
	
	__fdust_init();
	
	cu_px = __cu_dlsym(__func__);
	
	nvreturn = (CUresult)cu_px(device, _xxGetPhysicalDevice(ordinal));
	if(nvreturn == CL_SUCCESS)
		*device = _xxGetVirtualDevice(*device);
	
	return nvreturn;
}

/*
 * Return compute capability 
 */
CUresult CUDAAPI cuDeviceComputeCapability(int *major, int *minor, CUdevice dev) {
	void * (*cu_px) ();
	
	__fdust_init();
	
	cu_px = __cu_dlsym(__func__);
	return (CUresult)cu_px(major, minor, _xxGetPhysicalDevice(dev));
}


/*
 * Return total memory for given device
 */
#if CUDA_VERSION >= 3020
   CUresult CUDAAPI cuDeviceTotalMem(size_t *bytes, CUdevice dev) {
#else
   CUresult CUDAAPI cuDeviceTotalMem(unsigned int *bytes, CUdevice dev) {
#endif
	void * (*cu_px) ();
	
	__fdust_init();
	
	cu_px = __cu_dlsym(__func__);
	return (CUresult)cu_px(bytes, _xxGetPhysicalDevice(dev));
}

/*
 * Fixes up the devname to include fdust stuff :)
 */
CUresult CUDAAPI cuDeviceGetName(char *name, int len, CUdevice dev) {
	void * (*cu_px) ();
	CUresult nvreturn;
	
	__fdust_init();
	
	cu_px = __cu_dlsym(__func__);
	
	nvreturn = (CUresult)cu_px(name, len, _xxGetPhysicalDevice(dev));
	
	if(nvreturn == CUDA_SUCCESS) {
		_xxAddDeviceMapping(name, len, _xxGetPhysicalDevice(dev), dev);
	}
	return nvreturn;
}

/*
 * Returns device properties of given device
 */
CUresult CUDAAPI cuDeviceGetProperties(CUdevprop *prop, CUdevice dev) {
	void * (*cu_px) ();
	
	__fdust_init();
	
	cu_px = __cu_dlsym(__func__);
	return (CUresult)cu_px(prop, _xxGetPhysicalDevice(dev));
}

/*
 * Returns attributes for given device
 */
CUresult CUDAAPI cuDeviceGetAttribute(int *pi, CUdevice_attribute attrib, CUdevice dev) {
	void * (*cu_px) ();
	
	__fdust_init();
	
	cu_px = __cu_dlsym(__func__);
	return (CUresult)cu_px(pi, attrib, _xxGetPhysicalDevice(dev));
}

/*
 * This MIGHT be a performance hog: __fdust_init could actually patch-this-out if it gets called
 * but i didn't figure out how to do this (yet)
*/
extern __host__ cudaError_t cudaMalloc (void ** devptr, size_t size) {
	void * (*cu_malloc) ();
	
	if(cuda_set_device < 0) {
		RMSG("OUCH! Your application just called cudaMalloc(), but no device was set!");
		RMSG("Simulating driver bug: setting first device as in-use");
		cudaSetDevice(0);
	}
	
	cu_malloc =  dlsym(RTLD_NEXT,__func__);
	return (cudaError_t)cu_malloc(devptr, size);
}

#ifdef _FDUST_PROFILE
/* Disabled per default: This can have a negative impact on performance */

extern __host__ cudaError_t CUDARTAPI cudaThreadSynchronize(void) {
	void * (*cu_perf_thread_sync)();
	
	__fdust_init();
	
	cu_perf_thread_sync = dlsym(RTLD_NEXT,__func__);
	return (cudaError_t)cu_perf_thread_sync();
}
extern __host__ cudaError_t CUDARTAPI cudaThreadExit(void) {
	void * (*cu_perf_thread_exit)();
	
	__fdust_init();
	
	cu_perf_thread_exit = dlsym(RTLD_NEXT,__func__);
	return (cudaError_t)cu_perf_thread_exit();
}

cudaError_t cudaLaunch (const char *entry) {
	void * (*cu_perf_launch)();
	
	__fdust_init();
	
	cu_perf_launch = dlsym(RTLD_NEXT,__func__);
	
	fdust_perfc.cu_kernel_launch++;
	return (cudaError_t)cu_perf_launch(entry);
}

#endif

void cublasInit() {
	void * (*cublas)();
	
	if(cuda_set_device < 0) {
		RMSG("OUCH! Called cublasInit() with no active device! calling cudaSetDevice(0) right now\n");
		cudaSetDevice(0);
	}
	cublas = dlsym(RTLD_NEXT,__func__);
	cublas();
}


/**************************************************************************************
*  Misc stuff
***************************************************************************************/

/*
 * Initializes the library. This should get called by ALL functions
 * __fdust_init does..
 *  -> set the debug level
 *  -> contact fairyd / lock devices
 */
void __fdust_init() {
	
	if(reserved_devices[0] == FDUST_RSV_NINIT) {
		__fdust_spam();
		__fdust_lock_devices(reserved_devices);
		
		printf("%s allocated gpu-count: %d device(s)\n", __FILE__, _xxGetNumberOfLockedDevices());
		
	}
}

/*
 * Return 'modeid'
 */
const char *__fdust_mode() {
	return "cuda.so";
}

/*
 * dlsym wrapper
 */
void *__cu_dlsym(const char *func) {
	/* We do not know when (and how) we get called for the first time, so we cannot
	 * 'pre-init' cuda */
	if(cuda_lib == NULL)
		cuda_lib = dlopen(LIBCUDA_SONAME, RTLD_NOW);
	assert(cuda_lib != NULL);
	
	return dlsym(cuda_lib, func);
}


/*
 * Returns the PHYSICAL device id of given fakedev
 */
cl_uint _xxGetPhysicalDevice(cl_uint virtual_i) {
	cl_int result;
	
	assert(virtual_i < MAX_GPUCOUNT);
	result = reserved_devices[virtual_i];
	
	DPRINT("virtual device=%d, physical device=%d\n", virtual_i, result);
	
	assert( result >= 0 );
	return (cl_uint)result;
}


/*
 * Returns the FAKEID for given physical device
 */
cl_uint _xxGetVirtualDevice(cl_uint physical_i) {
	cl_uint i;
	
	DPRINT("physical_i=%d\n",physical_i);
	
	assert(physical_i < MAX_GPUCOUNT);
	for(i=0;i<MAX_GPUCOUNT;i++) {
		if(reserved_devices[i] == physical_i)
			return i;
		if(reserved_devices[i] < 0)
			break;
	}
	
	/* reached on error */
	assert(0);
}

/*
 * Returns the number of locked devices
 */
cl_uint _xxGetNumberOfLockedDevices() {
	cl_uint i;
	for(i=0;i<MAX_GPUCOUNT;i++) {
		if(reserved_devices[i] == FDUST_RSV_END)
			return i;
	}
	return -1;
}

/*
 * Takes an OpenCL device pointer and returns the FAKEDEV for it
 */
cl_uint _xxGetFakedevFromClPtr(cl_device_id clptr) {
	cl_int i=0;
	
	for(i=0;i<MAX_GPUCOUNT;i++) {
		if(ocl_ptrcache[i] == clptr)
			return i;
	}
	
	RMSG("Ooops! shouldn't be here -> somehow ocl_ptrcache got messed up?!");
	abort();
	return 0; // make gcc happy!
}

/*
 * Includes some debug infos in the device description
 */
void _xxAddDeviceMapping(void *param_value, size_t malloced_bytes, cl_int real_dev, cl_int fake_dev) {
	char fdust_devname[CUDA_PROP_NAME_LEN] = {0};
	
	sprintf(fdust_devname, " - fdust{v:h}={%u:%u}", fake_dev, real_dev);
	
	if( (strlen(param_value)+strlen(fdust_devname)) < malloced_bytes ) {
		memcpy(param_value+strlen(param_value), fdust_devname, strlen(fdust_devname)+1); // +1 = include \0 of sprintf()
	}

}



