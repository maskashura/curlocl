// Copyright 2016 Wolf (Wolf9466)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <CL/cl.h>

#include "curlutil.h"
#include "curl.h"
#include "curlocl.h"

void CurlInit(CurlCtx *ctx)
{
	for(int i = 0; i < STATE_SIZE + 1; ++i) ctx->Indices[i] = (i * 364) % 729;
	memset(ctx->State, 0x00, STATE_SIZE);
}

void CurlTransform(CurlCtx *ctx)
{
    for(int r = 0; r < 27; ++r)
    {
		int8_t StateCopy[STATE_SIZE + 1];
		memcpy(StateCopy, ctx->State, STATE_SIZE);
		
		for(int i = 0; i < STATE_SIZE; ++i)
			ctx->State[i] = T[StateCopy[ctx->Indices[i]] + (StateCopy[ctx->Indices[i + 1]] << 2) + 5];
	}
}

void Curl(CurlCtx *ctx, int8_t *input, int size)
{
	do
	{
		memcpy(ctx->State, input, size < HASH_SIZE ? size : HASH_SIZE);
		CurlTransform(ctx);
		input += HASH_SIZE;
		size -= HASH_SIZE;
	} while(size > 0);
}

void CurlSqueeze(CurlCtx *ctx, void *output)
{
    memcpy(output, ctx->State, HASH_SIZE);
    CurlTransform(ctx);
}

int run_test(OCLCtx *OCL, OCLCurlCtx *OCLCurl, int8_t in);

int main(int argc, char **argv)
{
	OCLCtx OCL;
	CurlCtx ctx;
	OCLCurlCtx OCLCurl;
	char FinalTX[2673 + 1], RawTXTrytes[2430 + 1];
	int8_t InTrits[486], MidTrits[486], RawTXTrits[7290];
	
	if(argc < 3 || argc > 4)
	{
		PrintErr("Usage: %s <TrunkTrytes> <BranchTrytes> [RawTXTrytes]\n", argv[0]);
	}
	
	if(strlen(argv[1]) != 81 || strlen(argv[2]) != 81)
	{
		PrintErr("Arguments provided must be 81 trytes in length.");
		return(-1);
	}
	
	if(!ValidTryteString(argv[1]) || !ValidTryteString(argv[2]))
	{
		PrintErr("Arguments provided must be valid tryte strings.\n");
		return(-1);
	}
	
	if(argc == 4)
	{
		if(strlen(argv[3]) != 2430)
		{
			PrintErr("RawTXTrytes, if provided, must be 2430 trytes in length.");
			return(-1);
		}
		
		if(!ValidTryteString(argv[3]))
		{
			PrintErr("Arguments provided must be valid tryte strings.\n");
			return(-1);
		}
	}
	
	InitOpenCLCtx(&OCL);
	InitOCLCurlCtx(&OCLCurl, &OCL);
	
	TrytesToTrits(InTrits, argv[1]);
	TrytesToTrits(InTrits + (81 * 3), argv[2]);
	
	if(argc == 3)
	{
		memset(FinalTX, '9', 2430);
		memset(MidTrits, 0x00, 486);
	}
	else
	{
		memcpy(FinalTX, argv[3], 2430);
		TrytesToTrits(RawTXTrits, argv[3]);
		CurlInit(&ctx);
		Curl(&ctx, RawTXTrits, 7290);
		memcpy(MidTrits, ctx.State + HASH_SIZE, STATE_SIZE - HASH_SIZE);
	}
	
	MineTX(&OCL, &OCLCurl, InTrits, MidTrits, FinalTX + 2430);
	
	// DON'T do this before MineTX, or you'll get an erroneous NULL byte
	memcpy(FinalTX + 2430 + 81, argv[1], 81);
	memcpy(FinalTX + 2430 + 81 + 81, argv[2], 81);
	
	FreeOCLCurlCtx(&OCLCurl);
	FreeOCLCtx(&OCL);
	
	/*
	OCLCtx OCL;
	OCLCurlCtx OCLCurl;
	
	InitOpenCLCtx(&OCL);
	InitOCLCurlCtx(&OCLCurl, &OCL);
	
	int ret = run_test(&OCL, &OCLCurl, -1);
	printf("Running Test on [-1]: %s %d\n",  ret == 0 ? "PASS": "FAIL", ret);
	
	ret = run_test(&OCL, &OCLCurl, 0);
	printf("Running Test on [0]: %s %d\n",   ret == 0 ? "PASS": "FAIL", ret);
	
	ret = run_test(&OCL, &OCLCurl, 1);
	printf("Running Test on [1]: %s %d\n", ret == 0 ? "PASS": "FAIL", ret);
	
	FreeOCLCurlCtx(&OCLCurl);
	FreeOCLCtx(&OCL);
	*/
	
	FinalTX[2673] = 0x00;
	
	printf("Final TX: %s\n", FinalTX);
	
	return(0);
	
}

// TXTail is the branch and trunk in trit form - concatenated
int MineTX(OCLCtx *OCL, OCLCurlCtx *OCLCurl, int8_t *TXTail, int8_t *TXMid, char *NonceTrytes)
{
	cl_int retval;
	bool Found = false;
	cl_uint Difficulty = 13;
	int8_t Output[HASH_SIZE] = { 0 };
	size_t GlobalThreads = 4194304, LocalThreads = 64, GlobalOffset = 0;
	
	retval = clEnqueueWriteBuffer(OCL->OCLQueue, OCLCurl->InBuffer, CL_TRUE, 0, sizeof(cl_char) * HASH_SIZE * 2, TXTail, 0, NULL, NULL);
	
	if(retval != CL_SUCCESS)
	{
		printf("Error %d when calling clEnqueueWriteBuffer to fill input buffer.\n", retval);
		return(-1);
	}
	
	retval = clEnqueueWriteBuffer(OCL->OCLQueue, OCLCurl->MidBuffer, CL_TRUE, 0, sizeof(cl_char) * HASH_SIZE * 2, TXMid, 0, NULL, NULL);
	
	if(retval != CL_SUCCESS)
	{
		printf("Error %d when calling clEnqueueWriteBuffer to fill input buffer.\n", retval);
		return(-1);
	}
	
	retval = clSetKernelArg(OCLCurl->CurlKernel, 0, sizeof(cl_mem), &OCLCurl->OutBuffer);
	
	if(retval != CL_SUCCESS)
	{
		printf("Error %d when calling clSetKernelArg for output buffer.\n", retval);
		return(-1);
	}
	
	retval = clSetKernelArg(OCLCurl->CurlKernel, 1, sizeof(cl_mem), &OCLCurl->InBuffer);
	
	if(retval != CL_SUCCESS)
	{
		printf("Error %d when calling clSetKernelArg for input buffer.\n", retval);
		return(-1);
	}
	
	retval = clSetKernelArg(OCLCurl->CurlKernel, 2, sizeof(cl_mem), &OCLCurl->MidBuffer);
	
	if(retval != CL_SUCCESS)
	{
		printf("Error %d when calling clSetKernelArg for midstate buffer.\n", retval);
		return(-1);
	}
	
	retval = clSetKernelArg(OCLCurl->CurlKernel, 3, sizeof(cl_uint), &Difficulty);
	
	if(retval != CL_SUCCESS)
	{
		printf("Error %d when calling clSetKernelArg for difficulty.\n", retval);
		return(-1);
	}
	
	do
	{
		retval = clEnqueueNDRangeKernel(OCL->OCLQueue, OCLCurl->CurlKernel, 1, &GlobalOffset, &GlobalThreads, &LocalThreads, 0, NULL, NULL);
		
		if(retval != CL_SUCCESS)
		{
			printf("Error %d when calling clEnqueueNDRangeKernel for kernel.\n", retval);
			return(-1);
		}
		
		retval = clEnqueueReadBuffer(OCL->OCLQueue, OCLCurl->OutBuffer, CL_TRUE, 0, sizeof(cl_char) * HASH_SIZE, Output, 0, NULL, NULL);
		
		if(retval != CL_SUCCESS)
		{
			printf("Error %d when calling clEnqueueReadBuffer to fetch results.\n", retval);
			return(-1);
		}
		
		clFinish(OCL->OCLQueue);
		GlobalOffset += GlobalThreads;
		
		for(int i = 0; i < HASH_SIZE && !Found; ++i) Found = Output[i] != 0;
	} while(!Found);
	
	TritsToTrytes(NonceTrytes, Output, 81);
	
	return(0);
}
	
int8_t TestVector0[] = { -1, 0, -1, 1, -1, -1, 0, 0, -1, -1, 0, 1, -1, 1, 0, 0, 1, 1, 1, -1, 1, -1, 1, 0, 0, -1, 0, -1, 1, 1, 1, 0, 1, 0, 0, -1, 1, -1, -1, 1, -1, -1, 1, -1, -1, 0, 1, 0, -1, -1, -1, 0, -1, 1, 1, 1, -1, -1, 1, 1, 1, 1, 1, 0, -1, 0, 0, 1, 0, -1, 1, 0, 0, 0, 1, 1, 1, -1, -1, -1, 0, 0, -1, 1, 0, 1, 1, 0, 0, 1, -1, 1, 0, 1, 1, 1, 1, -1, -1, 0, 0, 0, -1, -1, 1, 0, 0, -1, 0, 1, 1, -1, -1, -1, 0, 1, -1, 1, -1, 1, 1, 1, 0, -1, -1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1, -1, 1, -1, 1, 1, 1, 0, -1, 1, 1, -1, 0, 0, 1, 0, 1, 1, -1, 1, -1, -1, -1, -1, -1, -1, 0, 1, 1, 0, -1, -1, 0, 1, 0, 0, -1, -1, -1, 1, 0, 0, 1, -1, -1, 0, 1, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, 0, 1, 0, 1, 1, 0, 1, -1, 1, 0, -1, 0, -1, 0, 1, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, -1, 1, -1, 1, 1, 0, -1, 0, 0, -1, 1, -1, -1, 0, -1, 0, 1, -1, -1, 1, 1, 0, -1, 0};
int8_t TestVector1[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int8_t TestVector2[] = { -1, 1, 1, 0, -1, -1, 0, 1, 1, -1, 1, -1, 1, -1, 1, 0, 0, 0, 0, -1, 0, 1, 0, -1, 1, 1, 0, -1, 1, -1, 1, -1, 0, -1, 0, 1, 0, -1, 1, 0, -1, 0, -1, -1, -1, 1, 1, -1, 1, -1, 0, 0, 0, 1, -1, 0, -1, 1, 0, -1, -1, 0, 1, -1, 0, 1, 1, 0, -1, -1, 0, 0, 0, -1, 1, 1, 1, 0, 1, 0, 1, 0, -1, -1, 0, -1, 0, -1, -1, 1, 1, -1, -1, 0, 0, 0, 0, 0, 1, 1, 1, -1, 1, 1, -1, 1, 1, 1, 0, 0, 1, 0, -1, -1, 0, 0, -1, -1, -1, -1, 1, -1, 1, 0, 0, -1, -1, 1, 0, -1, 1, -1, -1, 0, 0, 1, -1, 1, -1, 1, -1, -1, -1, 0, 1, 1, -1, 1, 1, 0, 0, -1, 0, 1, -1, 1, 0, 1, -1, 1, 1, 0, -1, 0, -1, -1, 0, 0, 1, -1, -1, 1, -1, -1, 0, -1, 1, 0, 1, -1, -1, 1, 1, -1, 1, -1, 0, 1, 0, 1, -1, 1, 1, 0, 1, -1, -1, 1, 0, 1, -1, -1, -1, -1, 0, -1, 1, 0, -1, 1, 0, 1, 0, -1, 1, -1, 0, 1, 0, 1, -1, -1, -1, 0, -1, 1, 1, 0, -1, 1, 0, 0, -1, 1, -1, 0, 1, 1, -1, 0, 0, 0, 0};


int run_test(OCLCtx *OCL, OCLCurlCtx *OCLCurl, int8_t in)
{
	CurlCtx ctx;
	cl_int retval;
	int8_t input[HASH_SIZE], output[HASH_SIZE];
	size_t GlobalThreads = 16777216, LocalThreads = 64;
	
	if (in < -1 || in > 1)
	{
		fprintf(stderr, "Invalid input\n");
		return(-1);
	}
	
	memset(input, in, HASH_SIZE);
	memset(output, 0x00, HASH_SIZE);
	
	//CurlInit(&ctx);
	//Curl(&ctx, input, HASH_SIZE);
	//CurlSqueeze(&ctx, output);
	
	retval = clEnqueueWriteBuffer(OCL->OCLQueue, OCLCurl->InBuffer, CL_TRUE, 0, sizeof(cl_char) * HASH_SIZE, input, 0, NULL, NULL);
	
	if(retval != CL_SUCCESS)
	{
		printf("Error %d when calling clEnqueueWriteBuffer to fill input buffer.\n", retval);
		return(-1);
	}
	
	retval = clSetKernelArg(OCLCurl->CurlKernel, 0, sizeof(cl_mem), &OCLCurl->OutBuffer);
	
	if(retval != CL_SUCCESS)
	{
		printf("Error %d when calling clSetKernelArg for output buffer.\n", retval);
		return(-1);
	}
	
	retval = clSetKernelArg(OCLCurl->CurlKernel, 1, sizeof(cl_mem), &OCLCurl->InBuffer);
	
	if(retval != CL_SUCCESS)
	{
		printf("Error %d when calling clSetKernelArg for input buffer.\n", retval);
		return(-1);
	}
	
	TIME_TYPE Begin, End;
	
	Begin = MinerGetCurTime();
	
	retval = clEnqueueNDRangeKernel(OCL->OCLQueue, OCLCurl->CurlKernel, 1, NULL, &GlobalThreads, &LocalThreads, 0, NULL, NULL);
	
	if(retval != CL_SUCCESS)
	{
		printf("Error %d when calling clEnqueueNDRangeKernel for kernel.\n", retval);
		return(-1);
	}
	
	retval = clEnqueueReadBuffer(OCL->OCLQueue, OCLCurl->OutBuffer, CL_TRUE, 0, sizeof(cl_char) * HASH_SIZE, output, 0, NULL, NULL);
	
	if(retval != CL_SUCCESS)
	{
		printf("Error %d when calling clEnqueueReadBuffer to fetch results.\n", retval);
		return(-1);
	}
	
	clFinish(OCL->OCLQueue);
	
	End = MinerGetCurTime();
	
	double seconds = SecondsElapsed(Begin, End);
	
	printf("16777216 hashes done in %.2f seconds for a hashrate of %.2fH/s.\n", seconds, 16777216.0 / seconds);
	
	switch(in)
	{
		case -1:
			return(memcmp(TestVector0, output, HASH_SIZE));
		case 0:
			return(memcmp(TestVector1, output, HASH_SIZE));
		case 1:
			return(memcmp(TestVector2, output, HASH_SIZE));
	}
}

