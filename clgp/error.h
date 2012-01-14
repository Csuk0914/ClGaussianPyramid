#ifndef _CLGP_ERROR_H_
#define _CLGP_ERROR_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __APPLE__
# include <CL/opencl.h>
#else
# include <OpenCL/opencl.h>
#endif

/* clgp errors, returned by all clgp_ functions */
enum {
	CLGP_CL_ERROR         = -1, /* Unspecified OpenCL failure */
	CLGP_ENOMEM           =  1, /* Not enough host memory */
	CLGP_NO_IMAGE_SUPPORT =  2, /* The device do not support image processing */
};

/* Retrieve the status of the last OpenCL operation */
cl_int
clgpClError();

#ifdef __cplusplus
}
#endif

#endif /* ndef _CLGP_ERROR_H_ */

