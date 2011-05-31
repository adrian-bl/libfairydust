Welcome to libfairydust!

libfairydust is a wrapper around NVIDIAs cuda and OpenCL implementation.
AMDs `stream' (APP) OpenCL implementation is also supported

Report bugs and problems to <adrian@blinkenlights.ch>


~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ 
FAQ Index:

1.0 Why should i use libfairydust?
1.1 Great! How can i compile libfairydust?

2.0 How to use libfairydust
2.1 Playing with the enviroment (local testing)
2.2 Using fairyd (cluster installation)


~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ 



1.0 Why should i use libfairydust?
--------------------------------------------------
libfairydust was written to be used in GPU-Clusters where multiple GPUs
are attached to one physical host and each GPU can be used by a different
user.

Most people do stuff like this:

 ...
 nvret = cudaSetDevice(0)
 if(nvret != CUDA_SUCCESS) {
    die_in_some_fata_way("oh noes!");
 }
 ...

This works great on your local workstation with one GPU but fails in 
spectacular ways on a GPU cluster. In the best case your code will simply crash.
In the worst case *all* your processes will just run on GPU#0 and you won't
even know about it.

Libfairydust solves this problem by simulating `fake-devices': If an application
requests GPU#0, libfairydust can re-route all requests to some other GPU!
This requires *NO* changes to existing applications: The application won't even
know about this: libfairydust works 100% transparent.



1.1 Great! How can i compile libfairydust?
--------------------------------------------------
Libfairydust has been tested with Cuda 3.2 and gcc 4.1.2 + 4.4.4 on x86_64 linux. Support for 32bit installations is currently not implemented.

Compiling it is as easy as doing:

 $ cd libfairydust/src
 $ make

 This should produce a file called ./obj/libfairydust.so.0

(Note: The makefile expects to find all cuda headers at
 /usr/local/cuda/include OR/AND at $(CUDA_INSTALL_PATH)/include)


To compile an AMD-Compatible version run:
 $ cd libfairydust/src
 $ make atidust

 This should produce a file called ./obj/libatidust.so.0
 (Note: The rest of this documentation will talk about
  `libfairydust.so.0' - the ati version works in exactly
  the same way - just replace the library name when needed)


2.0 How to use libfairyudst
--------------------------------------------------
libfairydust.so.0 needs to get pre-loaded by ld-linux.
To do this, run something like this in your shell:

 $ export LD_PRELOAD=/path/to/obj/libfairydust.so.0

That's it: Libfairydust will now hijack all cuda/openCL calls!




2.1 Playing with the environment (local testing)
--------------------------------------------------
Running any cuda/openCL application with libfairydust preloaded
should now show such an error message:

$ ./deviceQuery
 ./deviceQuery Starting...
 
  CUDA Device Query (Runtime API) version (CUDART static linking)
 
 fairydust.c starting up - (C) 2010-2011 Adrian Ulrich <adrian@blinkenlights.ch>
 fairydust.c release git-108, compiled with gcc-4.4.4 and cuda-3020 at 1295351458
 connection to fairyd failed: aborting


Booh! What happened? Libfairyd asked 'fairyd' for some new GPUs but fairyd isn't
running yet.

Don't worry: We can use libfairydust without a running fairyd instance:

 $ export FDUST_FORCE_DEBUG=1   # get some debug infos
 $ FDUST_ALLOCATE=0 ./deviceQuery
 
`deviceQuery' will now show exactly ONE GPU (even if you have 10 GPUs)

If you have multiple GPUs you can try to run

 $ FDUST_ALLOCATE=1 ./deviceQuery

It will still show only one GPU, but this time libfairydust tricked libcuda into
beliving that GPU#1 is actually GPU#0

You might also have noticed that libfairydust changed the device names into something
like this.

   Device = GeForce 9500 GT - fdust{v:h}={0:1}

In this case libfairydust simulated a VIRTUAL (v) device 0.
The REAL (h (hardware)) device for this virtual device is set to device 1


`FDUST_ALLOCATE' also supports multiple GPUs: Setting it to
 "0 3 2"
 
 Will map:
  The REAL GPU0 to the VIRTUAL GPU0
  The REAL GPU3 to the VIRTUAL GPU1
  The REAL GPU2 to the VIRTUAL GPU2

Setting FDUST_ALLOCATE="@" will cause libfairydust to return all found GPUs



2.2 Using fairyd (cluster installation)
--------------------------------------------------
In a 'real-world' installation you will have to adapt fairyd.pl to your batchsystem.
The included version of fairyd.pl works with LSF 7.x and was written for use on the
Brutus-Cluster of the ETH Zurich.


Blabla: Fixme: Write more text :-)

 - add startup workflow (connect to -> reply)
 - protocol description
 - testing implementation (use xterm as launcher?)


