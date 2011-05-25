#define _GNU_SOURCE

#ifndef __LP64__
#error "Untested environment"
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

/* max. number of supported GPUs per system */
#define MAX_GPUCOUNT 256


/* internal vars to for __fdust_lock_devices arg[] */
#define FDUST_RSV_END   -1
#define FDUST_RSV_NINIT -2



#define RELINFO               "(C) 2010-2011 <adrian@blinkenlights.ch> // ETHZ"
#define FDUST_ALLOCATE        "FDUST_ALLOCATE"                                    /* Used by getenv()        */
#define FDUST_FORCE_DEBUG     "FDUST_FORCE_DEBUG"                                 /* Printout debug messages */
#define FDUST_BE_QUIET        "FDUST_BE_QUIET"                                    /* Be less verbose         */

#define FDUST_MODE_NVIDIA 1
#define FDUST_MODE_ATI    2

/* Debug stuff */
#define DPRINT2(...)       if(getenv(FDUST_FORCE_DEBUG)!=NULL) { fprintf(stderr, __VA_ARGS__); }
#define DPRINT(_fmt, ...)  DPRINT2("\x1b[1;33m[libfdust] %8s:%4d (%-30s) | " _fmt "\x1b[0m", __FILE__,__LINE__, __func__, __VA_ARGS__)
#define RMSG(...)          fprintf(stderr, "\x1b[1;31m[libfdust] %s\x1b[0m\n", __VA_ARGS__);



/* host and port to connect */
#define NVD_HOST "127.0.0.1"
#define NVD_PORT 6680


void __fdust_spam();
const char *__fdust_mode();

void lock_fdust_devices(int *rdevs, int opmode);
int _get_number_of_nvidia_devices();
int _get_number_of_ati_devices();
