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

#include "amdati.h"



const char *__fdust_mode() {
	return "atidust";
}



/**************************************************************************************
*  OpenCL part
***************************************************************************************/


/*
 * Emulate clGetDeviceIDs
*/

extern CL_API_ENTRY cl_int CL_API_CALL clGetDeviceIDs(cl_platform_id platform    , cl_device_type device_type,
                                                      cl_uint        num_entries , cl_device_id *devices,
                                                      cl_uint        *num_devices) {
	
	static void     *(*ati_gdi) ();                      /* libOpenCL's version of clGetDeviceIDs */
	cl_uint         locked_gpus;                         /* number of GPUs we can use             */
	cl_uint         rval                 = 0;            /* return value of dlsym() calls         */
	cl_uint         i                    = 0;            /* generic loop counter                  */
	cl_uint         scratch_matched_devs = 0;
	cl_device_id    scratch_handout_devs[MAX_GPUCOUNT];
	cl_uint         total_matched_devs   = 0;
	cl_device_id    total_handout_devs[MAX_GPUCOUNT];
	
	/* init call to libOpenCL */
	if(!ati_gdi)
		ati_gdi = (void *(*) ()) dlsym(RTLD_NEXT, __func__);
	
	_atidust_init();
	locked_gpus = _get_locked_gpu_count();
	
	DPRINT("caller set device_type to %08X\n", device_type);
	
	if(platform == NULL && (rval = CL_OUT_OF_HOST_MEMORY)) /* platform shall NOT be null! */
		goto CLEANUP;
	
	
	if(device_type & CL_DEVICE_TYPE_GPU) {
		rval = ati_gdi(platform, CL_DEVICE_TYPE_GPU, MAX_GPUCOUNT, scratch_handout_devs, &scratch_matched_devs);
		if(rval != CL_SUCCESS)
			goto CLEANUP;
		for(i=0;i<locked_gpus;i++) {
			rval = _get_phys_from_virtual(i);
			assert(rval < scratch_matched_devs);
			total_handout_devs[total_matched_devs++] = scratch_handout_devs[rval];
		}
	}
	
	/* cpu should be after GPUs - stream does it like this */
	if(device_type & CL_DEVICE_TYPE_CPU) {
		rval = ati_gdi(platform, CL_DEVICE_TYPE_CPU, MAX_GPUCOUNT, scratch_handout_devs, &scratch_matched_devs);
		if(rval != CL_SUCCESS)
			goto CLEANUP;
		
		for(i=0;i<scratch_matched_devs;i++) {
			total_handout_devs[total_matched_devs++] = scratch_handout_devs[i];
		}
		
	}
	
	
	if(num_devices != NULL)
		*num_devices = total_matched_devs;
	
	if(devices != NULL && num_entries > 0) {
		for(i=0;i<num_entries;i++) {
			if(i>=total_matched_devs)
				break;
			devices[i] = total_handout_devs[i];
		}
	}
	
	
	CLEANUP:
		return rval;
}


void _atidust_init() {
	if(reserved_devices[0] == FDUST_RSV_NINIT) {
		__fdust_spam();
		lock_fdust_devices(reserved_devices, FDUST_MODE_ATI); /* FIXME: _ATI */
		printf("%s allocated gpu-count: %d device(s)\n", __FILE__, _get_locked_gpu_count());
	}
}



/*
 * Returns the PHYSICAL device id of given fakedev
 */
cl_uint _get_phys_from_virtual(cl_uint virtual_i) {
	cl_int result;
	
	assert(virtual_i < MAX_GPUCOUNT);
	result = reserved_devices[virtual_i];
	
	DPRINT("virtual device=%d, physical device=%d\n", virtual_i, result);
	
	assert( result >= 0 );
	return (cl_uint)result;
}



/*
 * Returns the number of locked devices
 */
cl_uint _get_locked_gpu_count() {
	cl_uint i;
	for(i=0;i<MAX_GPUCOUNT;i++) {
		if(reserved_devices[i] == FDUST_RSV_END)
			return i;
	}
	return -1;
}
