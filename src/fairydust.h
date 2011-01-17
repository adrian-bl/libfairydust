/*
 * This file is part of libfairydust - (C) 2010-2011 Adrian Ulrich
 *
 * Licensed under the terms of `The Artistic License 2.0'
 * See: ../artistic-2_0.txt or http://www.perlfoundation.org/attachment/legal/artistic-2_0.txt
 *
 */


#define _GNU_SOURCE

#ifndef __LP64__
#error "Untested environment (libfairydust expects 64bit pointers)"
#endif

#include <sys/utsname.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/file.h>
#include <assert.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <CL/cl.h>

#include <cuda_runtime_api.h>
#include <cuda.h>

/* max. number of supported GPUs per system */
#define MAX_GPUCOUNT 256

/* library to load */
#define LIBCUDA_SONAME "libcuda.so"

/* internal vars to for reserved_devices[] */
#define FDUST_RSV_END   -1
#define FDUST_RSV_NINIT -2

#define RELINFO "(C) 2010-2011 Adrian Ulrich <adrian@blinkenlights.ch>"

#define FDUST_ALLOCATE        "FDUST_ALLOCATE"
#define FDUST_FORCE_DEBUG     "FDUST_FORCE_DEBUG"
#define FDUST_BE_QUIET        "FDUST_BE_QUIET"

#define FDUST_TESTBUILD

#define DMODE_INIT         if(getenv(FDUST_FORCE_DEBUG)!=NULL) { debug_mode = 1; }
#define DPRINT2(...)       if(debug_mode) { fprintf(stderr, __VA_ARGS__); }
#define DPRINT(_fmt, ...)  DPRINT2("\x1b[1;33m[libfdust] %8s:%4d (%-30s) | " _fmt "\x1b[0m", __FILE__,__LINE__, __func__, __VA_ARGS__)
#define RMSG(...)          fprintf(stderr, "\x1b[1;31m[libfdust] %s\x1b[0m\n", __VA_ARGS__);

#define CUDA_PROP_NAME_LEN 255 // cuda header uses 255 for device naming



/* user hints for common errors */
#define HINT_OCL_PLATFORM_NULL "Hint: First argument of clGetDeviceIDs() [`platform'] must not be `NULL' while running on nvidia! You should call `clGetPlatformIDs()' or `oclGetPlatformID()'. Your application will crash soon."


/* host and port to connect */
#define NVD_HOST "127.0.0.1"
#define NVD_PORT 6680


int reserved_devices[MAX_GPUCOUNT]       = { FDUST_RSV_NINIT };  /* map of reserved devices */
cl_device_id ocl_ptrcache[MAX_GPUCOUNT]  = { -1 };               /* map between fakedev <-> ocl_pointer device */
int debug_mode = 0;


struct fdust_performance {
	uint64_t cu_kernel_launch;
	uint64_t cu_thread_exit;
	uint64_t cu_thread_synch;
};



cl_uint _xxGetNumberOfLockedDevices();
cl_uint _xxGetPhysicalDevice(cl_uint i);
cl_uint _xxGetVirtualDevice(cl_uint i);
cl_uint _xxGetFakedevFromClPtr(cl_device_id clptr);

void *__cu_dlsym    (const char *func);                        /* dlsym helper                */
void _xxAddDeviceMapping(void *param_value, size_t malloced_bytes, cl_int real_dev, cl_int fake_dev);

void __fdust_lock_devices();
void __fdust_init();


extern CL_API_ENTRY cl_int CL_API_CALL clGetDeviceIDs(cl_platform_id platform    , cl_device_type device_type,
                                                      cl_uint        num_entries , cl_device_id *devices,
                                                      cl_uint        *num_devices);
extern CL_API_ENTRY cl_int CL_API_CALL clGetDeviceInfo(cl_device_id device_id     , cl_device_info param_name,
                                                      size_t param_value_size    , void *param_value,
                                                      size_t *param_value_size_ret);

