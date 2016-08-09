// Copyright 2016 Wolf (Wolf9466)

#ifndef __CURLUTIL_H
#define __CURLUTIL_H

#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define PrintErr(...)		fprintf(stderr, __VA_ARGS__)

// Trytes must be a NULL terminated string
// Trits array must have strlen(Trytes) * 3 members allocated
void TrytesToTrits(int8_t *Trits, const char *Trytes);

// Trytes must have len + 1 members allocated
// Trits must have len * 3 members allocated
void TritsToTrytes(char *Trytes, const int8_t *Trits, size_t len);

// Trytes must be a NULL terminated string.
bool ValidTryteString(char *Trytes);

#ifdef __linux__

#define TIME_TYPE	struct timespec

#else

#define TIME_TYPE	clock_t

#endif

TIME_TYPE MinerGetCurTime(void);
double SecondsElapsed(TIME_TYPE Start, TIME_TYPE End);

#endif
