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






/**************************************************************************************
*  OpenCL part
***************************************************************************************/


/*
 * Emulate clGetDeviceIDs
*/
extern CL_API_ENTRY cl_int CL_API_CALL clGetDeviceIDs(cl_platform_id platform    , cl_device_type device_type,
                                                      cl_uint        num_entries , cl_device_id *devices,
                                                      cl_uint        *num_devices) {
	
	static void * (*ati_gdi) ();           /* libOpenCL's version of clGetDeviceIDs    */
	cl_uint internal_num_devices;          /* number of (all) hw-devices               */
	cl_device_id *internal_devices;        /* will hold the IDs of ALL devices         */
	cl_int nv_return, lock_cnt, i, foo;
	
	/* init call to libOpenCL */
	if(!ati_gdi)
		ati_gdi = (void *(*) ()) dlsym(RTLD_NEXT, __func__);
	
	/* init libfairydust and get number of useable devices */
	__atidust_init(); /* fixme: thread safe! */
	lock_cnt = _xxGetNumberOfLockedDevices();
	
	/* Get the number of physical devices */
	nv_return = ati_gdi(platform, device_type, NULL, NULL, &internal_num_devices);
	
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
		
		nv_return = ati_gdi(platform, device_type, internal_num_devices, internal_devices, NULL);
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
	
	__atidust_init();
	
	gdx_return = nv_gdx(device_id, param_name, param_value_size, param_value, param_value_size_ret);
	
	if(gdx_return == CL_SUCCESS && param_name == CL_DEVICE_NAME) {
		param_value_size_ret = NULL; // requests caller to ignore and was set to NULL in Cuda 3.2RC anyway...
		_xxAddDeviceMapping(param_value, param_value_size, _xxGetPhysicalDevice(_xxGetFakedevFromClPtr(device_id)), _xxGetFakedevFromClPtr(device_id));
	}
	return gdx_return;
}


void __atidust_init() {
	if(reserved_devices[0] == FDUST_RSV_NINIT) {
		__fdust_spam();
		__fdust_lock_devices(reserved_devices, num_ati_devices);
		printf("%s allocated gpu-count: %d device(s)\n", __FILE__, _xxGetNumberOfLockedDevices());
	}
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
	char fdust_devname[STREAM_DEVNAME_LEN] = {0};
	
	sprintf(fdust_devname, " - fdust{v:h}={%u:%u}", fake_dev, real_dev);
	
	if( (strlen(param_value)+strlen(fdust_devname)) < malloced_bytes ) {
		memcpy(param_value+strlen(param_value), fdust_devname, strlen(fdust_devname)+1); // +1 = include \0 of sprintf()
	}

}



