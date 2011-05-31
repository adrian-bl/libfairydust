/*
 * This file is part of libfairydust - (C) 2010-2011 Adrian Ulrich
 *
 * Licensed under the terms of `The Artistic License 2.0'
 * See: ../artistic-2_0.txt or http://www.perlfoundation.org/attachment/legal/artistic-2_0.txt
 *
 */

#include "common.h"
#include <CL/cl.h>

#define STREAM_DEVNAME_LEN 255 /* fixme */

int reserved_devices[MAX_GPUCOUNT]       = { FDUST_RSV_NINIT };  /* map of reserved devices */
cl_device_id ocl_ptrcache[MAX_GPUCOUNT]  = { -1 };               /* map between fakedev <-> ocl_pointer device */




cl_uint _get_phys_from_virtual(cl_uint virtual_i);
cl_uint _get_locked_gpu_count();
void _atidust_init();


extern CL_API_ENTRY cl_int CL_API_CALL clGetDeviceIDs(cl_platform_id platform    , cl_device_type device_type,
                                                      cl_uint        num_entries , cl_device_id *devices,
                                                      cl_uint        *num_devices);
extern CL_API_ENTRY cl_int CL_API_CALL clGetDeviceInfo(cl_device_id device_id     , cl_device_info param_name,
                                                      size_t param_value_size    , void *param_value,
                                                      size_t *param_value_size_ret);

