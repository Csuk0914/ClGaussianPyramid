#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <CL/cl.h>

#include <cv.h>
#include <highgui.h>

#include <clgp.h>
#include <utils.h>

/* Get the x origin of the level in the pyramid image */
#define LEVEL_ORIGIN_X(level, width, height) \
        ((level == 0) \
            ? 0 \
            : (int)((( (1.f - powf(0.5f, (float)(level>>1)) ) / (1.f-0.5f)) + 1.f)*(float)width))

/* Get the y origin of the level in the pyramid image */
#define LEVEL_ORIGIN_Y(level, width, height) \
        ((level <= 2) ? 0 : (level & 0x1)*(height>>(level>>1))) 

int
main(int argc, char *argv[])
{
    cl_int err = 0;

    cl_device_id device;
    cl_context context = 0;
    cl_command_queue queue = 0;

    IplImage *ipl_input = NULL, *ipl_tmp = NULL, *ipl_pyramid = NULL;

    cl_image_format imageformat = {CL_BGRA, CL_UNSIGNED_INT8};
    cl_mem climage_input, climage_pyramid[32];
    int maxlevel = 0, level = 0;
    
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {0, 0, 1};

    struct timeval start, stop;
    double compute_time = 0., total_time = 0.;


    /* OpenCL init, using our utils functions */
    clgpMaxflopsDevice(&device);

    /* Create a context on this device */
    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Could not create a context for the device\n");
        exit(1);
    }

    /* Create a command queue for the library */
    queue = clCreateCommandQueue(context, device, 0, &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Could not create a command queue for the device\n");
        exit(1);
    }

    /* Initialize the clgp library */
    if (clgpInit(context, queue) != 0) {
        fprintf(stderr, "Could not init clgp library\n");
        exit(1);
    }


    /* Load image */
    ipl_input = cvLoadImage(argv[1], CV_LOAD_IMAGE_COLOR);
    if (ipl_input == NULL) {
        fprintf(stderr, "Could not load file %s\n", argv[1]);
        exit(1);
    }

    /* Convert ipl_input to an acceptable OpenCL format (we don't do 
     * {RGB,UINT8}) */
    ipl_tmp = 
        cvCreateImage(
                cvSize(ipl_input->width, ipl_input->height), 
                ipl_input->depth, 
                4);
    cvCvtColor(ipl_input, ipl_tmp, CV_BGR2BGRA);
    cvReleaseImage(&ipl_input);
    ipl_input = ipl_tmp;
    ipl_tmp = NULL;

    /* Create ipl_pyramid image */
    ipl_pyramid = 
        cvCreateImage(
                cvSize(ipl_input->width*3, ipl_input->height),
                ipl_input->depth, 
                4);
    cvRectangle(
            ipl_pyramid,
            cvPoint(0, 0),
            cvPoint(ipl_pyramid->width, ipl_pyramid->height),
            cvScalar(0, 0, 0, 0),
            CV_FILLED,
            0,
            0);

    /* Create buffers on device */
    climage_input =
        clgpCreateImage2D(
                CL_MEM_READ_ONLY,
                &imageformat,
                ipl_input->width,
                ipl_input->height,
                &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Could not allocate climage_input\n");
        exit(1);
    }

    maxlevel = clgpMaxlevel(ipl_input->width, ipl_input->height);
    for (level = 0; level < maxlevel; level++) {
        climage_pyramid[level] =
            clgpCreateImage2D(
                    CL_MEM_READ_WRITE,
                    &imageformat,
                    ipl_input->width >> (level>>1),
                    ipl_input->height >> (level>>1),
                    &err);
        if (err != CL_SUCCESS) {
            fprintf(stderr, "Could not allocate climage_pyramid[%d]\n", level);
            exit(1);
        }
    }


    /* Send input image on device */
    gettimeofday(&start, NULL);
    region[0] = ipl_input->width;
    region[1] = ipl_input->height;
    err = 
        clEnqueueReadImage(
                queue,
                climage_input,
                CL_TRUE,
                origin,
                region,
                ipl_input->widthStep,
                0,
                ipl_input->imageData,
                0,
                NULL,
                NULL);
    clFinish(queue);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Could not copy ipl_input data on device (%d)\n", err);
        exit(1);
    }
    gettimeofday(&stop, NULL);
    total_time = 
        (stop.tv_sec-start.tv_sec)*1000.f + (stop.tv_usec-start.tv_usec)/1000.f;


    /* At last, call our pyramid function */
    gettimeofday(&start, NULL);
    clgpBuildPyramid(
            climage_pyramid, 
            climage_input, 
            ipl_input->width, 
            ipl_input->height);
    clFinish(queue);
    gettimeofday(&stop, NULL);
    compute_time = 
        (stop.tv_sec-start.tv_sec)*1000.f + (stop.tv_usec-start.tv_usec)/1000.f;
    total_time += compute_time;


    /* Retrieve images */
    gettimeofday(&start, NULL);
    for (level = 0; level < maxlevel; level++) {
        region[0] = ipl_input->width >> (level>>1);
        region[1] = ipl_input->height >> (level>>1);
        err = 
            clEnqueueReadImage(
                    queue,
                    climage_pyramid[level],
                    CL_TRUE,
                    origin,
                    region,
                    ipl_pyramid->widthStep,
                    0,
                    (void *)((char *)((char *)ipl_pyramid->imageData + LEVEL_ORIGIN_Y(level, ipl_input->width, ipl_input->height)*ipl_pyramid->widthStep) + LEVEL_ORIGIN_X(level, ipl_input->width, ipl_input->height)*ipl_pyramid->nChannels),
                    0,
                    NULL,
                    NULL);
    }
    clFinish(queue);
    if (err != CL_SUCCESS) {
        fprintf(stderr, 
                "Could not copy climage_pyramid data on host (%d)\n", err);
        exit(1);
    }
    gettimeofday(&stop, NULL);
    total_time += 
        (stop.tv_sec-start.tv_sec)*1000.f + (stop.tv_usec-start.tv_usec)/1000.f;


    /* Release the clgp library */
    clgpRelease();


    /* Release device ressources */
    clReleaseMemObject(climage_input);
    for (level = 0; level < maxlevel; level++) {
        clReleaseMemObject(climage_pyramid[level]);
    }

    clReleaseContext(context);
    clReleaseCommandQueue(queue);


    /* Show results */
    printf(" * Pyramid time: %f ms\n", compute_time);
    printf(" * Total time: %f ms\n", total_time);
    /* Display */
    cvNamedWindow("gaussian pyramid", CV_WINDOW_AUTOSIZE);
    cvShowImage("gaussian pyramid", ipl_pyramid);
    while (cvWaitKey(0) != -1) {}
    cvDestroyWindow("gaussian pyramid");

    cvReleaseImage(&ipl_input);
    cvReleaseImage(&ipl_pyramid);


    return 0;
}

