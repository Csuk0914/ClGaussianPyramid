/* Copyright (c) 2009-2012 Matthieu Volat and Remi Emonet. All rights
 * reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __APPLE__
# include <CL/opencl.h>
#else
# include <OpenCL/opencl.h>
#endif

#include "downsampledgauss5x5.h"
#include "downscaledgauss5x5.h"
#include "error.h"
#include "gauss9x9.h"

#ifndef CLFLAGS
# define CLFLAGS "-cl-mad-enable -cl-fast-relaxed-math"
#endif

extern cl_int clgp_clerr;

/* Global variables for the library */
extern const char clgp_downsampledgauss5x5_src[];
extern const char clgp_downscaledgauss5x5_src[];
extern const char clgp_gauss9x9_src[];

#define DOWNSCALEDGAUSS5X5 0
#define GAUSS9X9_ROWS 1
#define GAUSS9X9_COLS 2
#define DOWNSAMPLEDGAUSS5X5_ROWS 3
#define DOWNSAMPLEDGAUSS5X5_COLS 4

void 
clgpRelease(cl_kernel *kernels);

/* Register clgp in opencl context, must be called before everyelse function */
int
clgpInit(cl_context context, cl_kernel **kernelsptr)
{
    int err = 0;

    cl_device_id *devices = NULL; 
	cl_uint ndevices = 0;

    cl_program program = NULL;
    cl_kernel *kernels = NULL;

    const char *sources[3] = {
        (const char *)clgp_downsampledgauss5x5_src, 
        (const char *)clgp_downscaledgauss5x5_src, 
        (const char *)clgp_gauss9x9_src
    };

#ifdef DEBUG
    char build_log[20000];
#endif

    /* Retrieve the device list associated with the context */
	clgp_clerr = 
		clGetContextInfo(
				context,
				CL_CONTEXT_NUM_DEVICES,
				sizeof(cl_uint),
				&ndevices,
				NULL);
    if (clgp_clerr != CL_SUCCESS) {
#ifdef DEBUG
        fprintf(stderr, "clgp: cannot access context devices number\n");
#endif
        err = CLGP_CL_ERROR;
        goto end;
    }
	if (ndevices == 0) {
		/* That would be strange... */
        goto end;
	}
	devices = malloc(ndevices * sizeof(cl_device_id));
    if (devices == NULL) {
        err = CLGP_ENOMEM;
        goto end;
    }
    clgp_clerr = 
        clGetContextInfo(
                context,
                CL_CONTEXT_DEVICES,
                ndevices * sizeof(cl_device_id),
                devices,
                NULL);
    if (clgp_clerr != CL_SUCCESS) {
#ifdef DEBUG
        fprintf(stderr, "clgp: cannot access context devices list\n");
#endif
        err = CLGP_CL_ERROR;
        goto end;
    }

	/* Check the devices image support */
	for (size_t d = 0; d < ndevices; d++) {
		cl_bool has_image = CL_FALSE;
		clgp_clerr = 
			clGetDeviceInfo(
					devices[d],
					CL_DEVICE_IMAGE_SUPPORT,
					sizeof(cl_bool),
					&has_image,
					NULL);
		if (clgp_clerr != CL_SUCCESS) {
#ifdef DEBUG
			fprintf(stderr, "clgp: cannot check device image support\n");
#endif
			err = CLGP_CL_ERROR;
			goto end;
		}
		if (has_image == CL_FALSE) {
#ifdef DEBUG
			fprintf(stderr, "clgp: no image support\n");
#endif
			err = CLGP_NO_IMAGE_SUPPORT;
			goto end;
		}
	}

    /* Allocate kernels array */
    kernels = malloc(8*sizeof(cl_kernel));
    if (kernels == NULL) {
        err = CLGP_ENOMEM;
        goto end;
    }
    memset(kernels, 0, 8*sizeof(cl_kernel));

    /* Build the programs... */
    program =
        clCreateProgramWithSource(
                context,
                3,
                (const char **)sources,
                NULL,
                &clgp_clerr);
    if (clgp_clerr != CL_SUCCESS) {
#ifdef DEBUG
        fprintf(stderr,
                "clgp: opencl program creation error\n");
#endif
        err = CLGP_CL_ERROR;
        goto end;
    }
    clgp_clerr =
        clBuildProgram(program, 0, NULL, CLFLAGS, NULL, NULL);
    if (clgp_clerr != CL_SUCCESS) {
#ifdef DEBUG
        fprintf(stderr, "clgp: opencl program build error\n");
		for (size_t d = 0; d < ndevices; d++) {
			clGetProgramBuildInfo(
					program,
					devices[d],
					CL_PROGRAM_BUILD_LOG,
					sizeof(build_log),
					build_log,
					NULL);
			fprintf(stderr, 
					"clgp: devices[%zu] build log:\n%s\n", 
					d,
					build_log);
		}
#endif
        err = CLGP_CL_ERROR;
        goto end;
    }
    /* Find the kernels... */
    kernels[DOWNSAMPLEDGAUSS5X5_COLS] = 
        clCreateKernel(program, "downsampledgauss5x5_cols", &clgp_clerr);
    if (clgp_clerr != CL_SUCCESS) {
#ifdef DEBUG
        fprintf(stderr, "clgp: downsampledgauss5x5_cols kernel not found\n");
#endif
        err = CLGP_CL_ERROR;
        goto end;
    }
    kernels[DOWNSAMPLEDGAUSS5X5_ROWS] = 
        clCreateKernel(program, "downsampledgauss5x5_rows", &clgp_clerr);
    if (clgp_clerr != CL_SUCCESS) {
#ifdef DEBUG
        fprintf(stderr, "clgp: downsampledgauss5x5_rows kernel not found\n");
#endif
        err = CLGP_CL_ERROR;
        goto end;
    }
    kernels[DOWNSCALEDGAUSS5X5] = 
        clCreateKernel(program, "downscaledgauss5x5", &clgp_clerr);
    if (clgp_clerr != CL_SUCCESS) {
#ifdef DEBUG
        fprintf(stderr, "clgp: downscaledgauss5x5 kernel not found\n");
#endif
        err = CLGP_CL_ERROR;
        goto end;
    }
    kernels[GAUSS9X9_ROWS] = 
        clCreateKernel(program, "gauss9x9_rows", &clgp_clerr);
    if (clgp_clerr != CL_SUCCESS) {
#ifdef DEBUG
        fprintf(stderr, "clgp: gauss9x9_rows kernel not found\n");
#endif
        err = CLGP_CL_ERROR;
        goto end;
    }
    kernels[GAUSS9X9_COLS] = 
        clCreateKernel(program, "gauss9x9_cols", &clgp_clerr);
    if (clgp_clerr != CL_SUCCESS) {
#ifdef DEBUG
        fprintf(stderr, "clgp: gauss9x9_cols kernel not found\n");
#endif
        err = CLGP_CL_ERROR;
        goto end;
    }

    /* Mark the context as used by our library */
    clgp_clerr = clRetainContext(context);
    if (clgp_clerr != CL_SUCCESS) {
#ifdef DEBUG
        fprintf(stderr, "clgp: context retaining error\n");
#endif
        err = CLGP_CL_ERROR;
        goto end;
    }

end:
	free(devices);

    if (err != 0) {
        clgpRelease(kernels);
    }

    *kernelsptr = kernels;

    return err;
}

/* Release the ressources created by the library */
void
clgpRelease(cl_kernel *kernels)
{
    cl_context context = NULL;
    cl_program program = NULL;

    /* Retrieve context reference from kernels */
    clgp_clerr =
        clGetKernelInfo(
                kernels[DOWNSAMPLEDGAUSS5X5_COLS],
                CL_KERNEL_CONTEXT,
                sizeof(cl_context),
                &context,
                NULL);
    if (clgp_clerr != CL_SUCCESS) {
#ifdef DEBUG
        fprintf(stderr, "clgp: could not access opencl context\n");
#endif
        return;
    }

    /* Retrieve program reference from kernels */
    clgp_clerr =
        clGetKernelInfo(
                kernels[DOWNSAMPLEDGAUSS5X5_COLS],
                CL_KERNEL_PROGRAM,
                sizeof(cl_program),
                &program,
                NULL);
    if (clgp_clerr != CL_SUCCESS) {
#ifdef DEBUG
        fprintf(stderr, "clgp: could not access opencl program\n");
#endif
        return;
    }
    /* Free our program and kernels */
    if (kernels[DOWNSAMPLEDGAUSS5X5_COLS] != NULL) {
        clReleaseKernel(kernels[DOWNSAMPLEDGAUSS5X5_COLS]);
    }
    if (kernels[DOWNSAMPLEDGAUSS5X5_ROWS] != NULL) {
        clReleaseKernel(kernels[DOWNSAMPLEDGAUSS5X5_ROWS]);
    }
    if (kernels[DOWNSCALEDGAUSS5X5] != NULL) {
        clReleaseKernel(kernels[DOWNSCALEDGAUSS5X5]);
    }
    if (kernels[GAUSS9X9_ROWS] != NULL) {
        clReleaseKernel(kernels[GAUSS9X9_ROWS]);
    }
    if (kernels[GAUSS9X9_COLS] != NULL) {
        clReleaseKernel(kernels[GAUSS9X9_COLS]);
    }
    if (program != NULL) {
        clReleaseProgram(program);
    }
    free(kernels);

    /* Release the context from *our* library */
    clReleaseContext(context);
}

/* Util function to get the max level for an image */
size_t
clgpMaxlevel(size_t width, size_t height)
{
    /* Go until the last reduction possible */
    return (size_t)log2f((float)((width > height) ? width : height)) + 1;
}

/* Build an array of images that are the different layers of the classic 
 * gaussian pyramid */
int
clgpEnqueuePyramid(
        cl_command_queue command_queue,
        cl_kernel *kernels,
        cl_mem pyramid_image[],
        cl_mem input_image,
        size_t maxlevel)
{
    int err = 0;

    size_t width = 0;
    size_t height = 0;
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {0, 0, 1};
    size_t level = 0;

    clGetImageInfo(input_image, CL_IMAGE_WIDTH, sizeof(size_t), &width, NULL);
    clGetImageInfo(input_image, CL_IMAGE_HEIGHT, sizeof(size_t), &height, NULL);
    region[0] = width;
    region[1] = height;

    /* First iteration -- just copy the image */
    err = 
        clEnqueueCopyImage(
                command_queue, 
                input_image, 
                pyramid_image[0], 
                origin, 
                origin, 
                region, 
                0, 
                NULL, 
                NULL);
    if (err != 0) {
        goto end;
    }
    /* Now go through the other levels until we can't reduce */
    for (level = 1; level < maxlevel; level++) {
        /* 5x5 blur + downscale */
        err = 
            clgpEnqueueDownscaledGauss5x5(
                    command_queue,
                    kernels,
                    pyramid_image[level],
                    pyramid_image[level-1],
                    width>>(level-1),
                    height>>(level-1));
        if (err != 0) {
            goto end;
        }
    }

end:
    return err;
}

/* Util function to get the max level for an image */
size_t
clgpMaxlevelHalfOctave(size_t width, size_t height)
{
    /* Go until the last reduction possible */
    return 2*clgpMaxlevel(width, height);
}

/* Build an array of images that are the different layers of the half-octave
 * gaussian pyramid */
int
clgpEnqueuePyramidHalfOctave(
        cl_command_queue command_queue,
        cl_kernel *kernels,
        cl_mem pyramid_image[],
        cl_mem input_image,
        size_t maxlevel)
{
    int err = 0;

    size_t width = 0;
    size_t height = 0;
    size_t level = 0;

    clGetImageInfo(input_image, CL_IMAGE_WIDTH, sizeof(size_t), &width, NULL);
    clGetImageInfo(input_image, CL_IMAGE_HEIGHT, sizeof(size_t), &height, NULL);

    /* First iteration manualy */
    /* First half octave */
    err = 
        clgpEnqueueGauss9x9(
                command_queue,
                kernels,
                pyramid_image[0], 
                input_image,
                width,
                height);
    if (err != 0) {
        goto end;
    }
    /* Second half-octave */
    err = 
        clgpEnqueueGauss9x9(
                command_queue,
                kernels,
                pyramid_image[1], 
                pyramid_image[0],
                width,
                height);
    if (err != 0) {
        goto end;
    }
    /* Now go through the other octaves until we can't reduce */
    for (level = 2; level < maxlevel; level++) {
        /* First half octave : 5x5 blur + downscale */
        err = 
            clgpEnqueueDownscaledGauss5x5(
                    command_queue,
                    kernels,
                    pyramid_image[level],
                    pyramid_image[level-1],
                    width>>((level-1)>>1),
                    height>>((level-1)>>1));
        if (err != 0) {
            goto end;
        }

        level++;

        /* Second half octave : 9x9 blur */
        err = 
            clgpEnqueueGauss9x9(
                    command_queue,
                    kernels,
                    pyramid_image[level], 
                    pyramid_image[level-1],
                    width>>(level>>1),
                    height>>(level>>1));
        if (err != 0) {
            goto end;
        }
    }

end:
    return err;
}

/* Build an array of images that are the different layers of the half-octave 
 * gaussian pyramid with sqrt2 layout */
int
clgpEnqueuePyramidSqrt2(
        cl_command_queue command_queue,
        cl_kernel *kernels,
        cl_mem pyramid_image[],
        cl_mem input_image,
        size_t maxlevel)
{
    int err = 0;

    size_t width = 0;
    size_t height = 0;
    size_t level = 0;

    clGetImageInfo(input_image, CL_IMAGE_WIDTH, sizeof(size_t), &width, NULL);
    clGetImageInfo(input_image, CL_IMAGE_HEIGHT, sizeof(size_t), &height, NULL);

    /* First iteration manualy */
    /* First half octave */
    err = 
        clgpEnqueueGauss9x9(
                command_queue,
                kernels,
                pyramid_image[0], 
                input_image,
                width,
                height);
    if (err != 0) {
        goto end;
    }
    /* Second half octave */
    err =
        clgpEnqueueDownsampledGauss5x5_cols(
                command_queue,
                kernels,
                pyramid_image[1], 
                pyramid_image[0], 
                width,
                height);
    if (err != 0) {
        goto end;
    }
    /* Now go through the other octaves until we can't reduce */
    for (level = 2; level < maxlevel; level++) {
        if (level & 0x1) {
            /* Odd level : downsampled 5x5 blur on rows */
            err =
                clgpEnqueueDownsampledGauss5x5_cols(
                        command_queue,
                        kernels,
                        pyramid_image[level], 
                        pyramid_image[level-1], 
                        width >> (level>>1),
                        height >> ((level-1)>>1));
            if (err != 0) {
                goto end;
            }
        }
        else {
            /* Even level: downsampled 5x5 blur on cols */
            err =
                clgpEnqueueDownsampledGauss5x5_rows(
                        command_queue,
                        kernels,
                        pyramid_image[level], 
                        pyramid_image[level-1], 
                        width >> (level>>1),
                        height >> ((level-1)>>1));
            if (err != 0) {
                goto end;
            }
        }
    }

end:
    return err;
}

