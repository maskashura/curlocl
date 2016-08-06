// Copyright 2016 Wolf (Wolf9466)

#ifndef __CURL_H_
#define __CURL_H_

#include <stdint.h>

#define HASH_SIZE		243
#define STATE_SIZE		HASH_SIZE * 3

const int8_t T[11] = {1, 0, -1, 0, 1, -1, 0, 0, -1, 1, 0};

typedef struct _CurlCtx
{
    int Indices[STATE_SIZE + 1];
    int8_t State[STATE_SIZE];
} CurlCtx;

void CurlInit(CurlCtx *ctx);
void Curl(CurlCtx *ctx, int8_t *input, int size);
void CurlSqueeze(CurlCtx *ctx, void *output);

#endif
