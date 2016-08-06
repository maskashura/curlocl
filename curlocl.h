// Copyright 2016 Wolf (Wolf9466)

#ifndef __OCLCURL_H
#define __OCLCURL_H

typedef struct _OCLCtx
{
	cl_platform_id PlatformID;
	cl_device_id DeviceID;
	cl_context OCLContext;
	cl_command_queue OCLQueue;
} OCLCtx;

typedef struct _OCLCurlCtx
{
	cl_mem InBuffer, MidBuffer, OutBuffer;
	cl_program OCLProgram;
	cl_kernel CurlKernel;
} OCLCurlCtx;

int InitOpenCLCtx(OCLCtx *ctx);
int InitOCLCurlCtx(OCLCurlCtx *ctx, OCLCtx *OCL);
void FreeOCLCurlCtx(OCLCurlCtx *OCLCurl);
void FreeOCLCtx(OCLCtx *OCL);


#endif
