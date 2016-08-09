// Copyright 2016 Wolf (Wolf9466)

#include <unistd.h>			// for sleep()
#include <CL/cl.h>
#include "curlocl.h"
#include "curlutil.h"

int InitOpenCLCtx(OCLCtx *ctx)
{
	const cl_queue_properties CommandQueueProperties[] = { 0, 0, 0 };
	cl_platform_id *PlatformIDList;
	cl_device_id *DeviceIDList;
	cl_uint entries;
	int retval;
	
	retval = clGetPlatformIDs(0, NULL, &entries);
	
	if(retval != CL_SUCCESS)
	{
		PrintErr("Error %d when calling clGetPlatformIDs for number of platforms.\n", retval);
		return(-1);
	}
	
	PlatformIDList = (cl_platform_id *)malloc(sizeof(cl_platform_id) * entries);
	retval = clGetPlatformIDs(entries, PlatformIDList, NULL);
	
	if(retval != CL_SUCCESS)
	{
		PrintErr("Error %d when calling clGetPlatformIDs for platform ID information.\n", retval);
		free(PlatformIDList);
		return(-1);
	}
	
	ctx->PlatformID = PlatformIDList[0];
	free(PlatformIDList);
	
	retval = clGetDeviceIDs(ctx->PlatformID, CL_DEVICE_TYPE_GPU, 0, NULL, &entries);
	
	if(retval != CL_SUCCESS)
	{
		PrintErr("Error %d when calling clGetDeviceIDs for number of devices.\n", retval);
		return(-1);
	}
	
	DeviceIDList = (cl_device_id *)malloc(sizeof(cl_device_id) * entries);
	
	retval = clGetDeviceIDs(ctx->PlatformID, CL_DEVICE_TYPE_GPU, entries, DeviceIDList, NULL);
	
	if(retval != CL_SUCCESS)
	{
		PrintErr("Error %d when calling clGetDeviceIDs for device ID information.\n", retval);
		free(DeviceIDList);
		return(-1);
	}
	
	ctx->DeviceID = DeviceIDList[0];
	free(DeviceIDList);
	
	ctx->OCLContext = clCreateContext(NULL, 1, &ctx->DeviceID, NULL, NULL, &retval);
	
	if(retval != CL_SUCCESS)
	{
		PrintErr("Error %d when calling clCreateContext.\n", retval);
		return(-1);
	}
	
	ctx->OCLQueue = clCreateCommandQueueWithProperties(ctx->OCLContext, ctx->DeviceID, CommandQueueProperties, &retval);
	
	if(retval != CL_SUCCESS)
	{
		PrintErr("Error %d when calling clCreateCommandQueueWithProperties.\n", retval);
		return(-1);
	}
	
	return(0);
}

// Pointer returned in Output must be freed
static size_t LoadTextFile(char **Output, char *Filename)
{
	size_t len;
	FILE *kernel = fopen(Filename, "rb");
	
	if(!kernel) return(0);
	
	fseek(kernel, 0, SEEK_END);
	len = ftell(kernel);
	fseek(kernel, 0, SEEK_SET);
	
	*Output = (char *)malloc(sizeof(char) * (len + 2));
	len = fread(*Output, sizeof(char), len, kernel);
	Output[0][len] = 0x00;		// NULL terminator
	
	return(len);
}

int InitOCLCurlCtx(OCLCurlCtx *ctx, OCLCtx *OCL)
{
	size_t len;
	cl_int retval;
	cl_build_status status;
	char *KernelSource, *BuildLog;
	
	len = LoadTextFile(&KernelSource, "curl.cl");
	
	if(!len)
	{
		PrintErr("Error loading OpenCL source file curl.cl.\n");
		return(-1);
	}
	
	ctx->OCLProgram = clCreateProgramWithSource(OCL->OCLContext, 1, (const char **)&KernelSource, NULL, &retval);
	
	if(retval != CL_SUCCESS)
	{
		PrintErr("Error %d when calling clCreateProgramWithSource on the contents of %s.\n", retval, "curl.cl");
		return(-1);
	}
	
	retval = clBuildProgram(ctx->OCLProgram, 1, &OCL->DeviceID, "", NULL, NULL);
	
	if(retval != CL_SUCCESS)
	{
		PrintErr("Error %d when calling clBuildProgram.\n", retval);
		
		retval = clGetProgramBuildInfo(ctx->OCLProgram, OCL->DeviceID, CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
	
		if(retval != CL_SUCCESS)
		{
			PrintErr("Error %d when calling clGetProgramBuildInfo for length of build log output.\n", retval);
			return(-1);
		}
		
		BuildLog = (char *)malloc(sizeof(char) * (len + 2));
		
		retval = clGetProgramBuildInfo(ctx->OCLProgram, OCL->DeviceID, CL_PROGRAM_BUILD_LOG, len, BuildLog, NULL);
		
		if(retval != CL_SUCCESS)
		{
			PrintErr("Error %d when calling clGetProgramBuildInfo for build log.\n", retval);
			return(-1);
		}
		
		PrintErr("Build Log:\n%s\n", BuildLog);
		
		free(BuildLog);
		
		return(-1);
	}
	
	do
	{
		retval = clGetProgramBuildInfo(ctx->OCLProgram, OCL->DeviceID, CL_PROGRAM_BUILD_STATUS, sizeof(cl_build_status), &status, NULL);
		if(retval != CL_SUCCESS)
		{
			PrintErr("Error %d when calling clGetProgramBuildInfo for status of build.\n", retval);
			return(-1);
		}
		sleep(1);
	} while(status == CL_BUILD_IN_PROGRESS);
	
	retval = clGetProgramBuildInfo(ctx->OCLProgram, OCL->DeviceID, CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
	
	if(retval != CL_SUCCESS)
	{
		PrintErr("Error %d when calling clGetProgramBuildInfo for length of build log output.\n", retval);
		return(-1);
	}
	
	BuildLog = (char *)malloc(sizeof(char) * (len + 2));
	
	retval = clGetProgramBuildInfo(ctx->OCLProgram, OCL->DeviceID, CL_PROGRAM_BUILD_LOG, len, BuildLog, NULL);
	
	if(retval != CL_SUCCESS)
	{
		PrintErr("Error %d when calling clGetProgramBuildInfo for build log.\n", retval);
		return(-1);
	}
	
	PrintErr("Build Log:\n%s\n", BuildLog);
	
	free(BuildLog);
	free(KernelSource);
	
	ctx->CurlKernel = clCreateKernel(ctx->OCLProgram, "curl", &retval);
	
	if(retval != CL_SUCCESS)
	{
		PrintErr("Error %d when calling clCreateKernel for kernel %s.\n", retval, "curl");
		return(-1);
	}
	
	ctx->InBuffer = clCreateBuffer(OCL->OCLContext, CL_MEM_READ_ONLY, sizeof(cl_char) * 486, NULL, &retval);
	
	if(retval != CL_SUCCESS)
	{
		PrintErr("Error %d when calling clCreateBuffer to create input buffer.\n", retval);
		return(-1);
	}
	
	ctx->MidBuffer = clCreateBuffer(OCL->OCLContext, CL_MEM_READ_ONLY, sizeof(cl_char) * 486, NULL, &retval);
	
	if(retval != CL_SUCCESS)
	{
		PrintErr("Error %d when calling clCreateBuffer to create midstate buffer.\n", retval);
		return(-1);
	}
	
	ctx->OutBuffer = clCreateBuffer(OCL->OCLContext, CL_MEM_READ_WRITE, sizeof(cl_char) * 243, NULL, &retval);
	
	if(retval != CL_SUCCESS)
	{
		PrintErr("Error %d when calling clCreateBuffer to create output buffer.\n", retval);
		return(-1);
	}
}

void FreeOCLCurlCtx(OCLCurlCtx *OCLCurl)
{
	clReleaseMemObject(OCLCurl->OutBuffer);
	clReleaseMemObject(OCLCurl->MidBuffer);
	clReleaseMemObject(OCLCurl->InBuffer);
	clReleaseKernel(OCLCurl->CurlKernel);
	clReleaseProgram(OCLCurl->OCLProgram);
}

void FreeOCLCtx(OCLCtx *OCL)
{
	clReleaseCommandQueue(OCL->OCLQueue);
	clReleaseContext(OCL->OCLContext);
}
