/*
 * This file is part of libfairydust - (C) 2010-2011 Adrian Ulrich
 *
 * Licensed under the terms of `The Artistic License 2.0'
 * See: ../artistic-2_0.txt or http://www.perlfoundation.org/attachment/legal/artistic-2_0.txt
 *
 */

#include "common.h"
#include <CL/cl.h>
#include <cuda_runtime_api.h>
#include <cuda.h>

/* library to load */
#define LIBCUDA_SONAME "libcuda.so"
/* user hints for common errors */
#define HINT_OCL_PLATFORM_NULL "Hint: First argument of clGetDeviceIDs() [`platform'] must not be `NULL' while running on nvidia! You should call `clGetPlatformIDs()' or `oclGetPlatformID()'. Your application will crash soon."
/* cuda headers use 255 for device naming */
#define CUDA_PROP_NAME_LEN 255


int reserved_devices[MAX_GPUCOUNT]       = { FDUST_RSV_NINIT };  /* map of reserved devices */
cl_device_id ocl_ptrcache[MAX_GPUCOUNT]  = { -1 };               /* map between fakedev <-> ocl_pointer device */




cl_uint _xxGetNumberOfLockedDevices();
cl_uint _xxGetPhysicalDevice(cl_uint i);
cl_uint _xxGetVirtualDevice(cl_uint i);
cl_uint _xxGetFakedevFromClPtr(cl_device_id clptr);

void *__cu_dlsym    (const char *func);                        /* dlsym helper                */
void _xxAddDeviceMapping(void *param_value, size_t malloced_bytes, cl_int real_dev, cl_int fake_dev);
void __fdust_init();


extern CL_API_ENTRY cl_int CL_API_CALL clGetDeviceIDs(cl_platform_id platform    , cl_device_type device_type,
                                                      cl_uint        num_entries , cl_device_id *devices,
                                                      cl_uint        *num_devices);
extern CL_API_ENTRY cl_int CL_API_CALL clGetDeviceInfo(cl_device_id device_id     , cl_device_info param_name,
                                                      size_t param_value_size    , void *param_value,
                                                      size_t *param_value_size_ret);

