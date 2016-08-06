// Copyright 2016 Wolf (Wolf9466)

#include <time.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "curlutil.h"

static const int8_t TryteToTritMapping[27][3] = {{0, 0, 0}, {1, 0, 0}, {-1, 1, 0}, {0, 1, 0}, {1, 1, 0}, {-1, -1, 1}, {0, -1, 1}, {1, -1, 1}, {-1, 0, 1}, {0, 0, 1}, {1, 0, 1}, {-1, 1, 1}, {0, 1, 1}, {1, 1, 1}, {-1, -1, -1}, {0, -1, -1}, {1, -1, -1}, {-1, 0, -1}, {0, 0, -1}, {1, 0, -1}, {-1, 1, -1}, {0, 1, -1}, {1, 1, -1}, {-1, -1, 0}, {0, -1, 0}, {1, -1, 0}, {-1, 0, 0}};
static const char TryteGlyphs[27] = "9ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// Little helper function to store 3 trits at a time
static inline void StoreTrits(int8_t *TritDst, const int8_t TritSrc[3], size_t idx)
{
	idx *= 3;
	for(size_t i = 0; i < 3; ++i) TritDst[idx + i] = TritSrc[i];
}

// Trytes must be a NULL terminated string
// Trits array must have strlen(Trytes) * 3 members allocated
void TrytesToTrits(int8_t *Trits, const char *Trytes)
{
	size_t TryteLen = strlen(Trytes);
	
	for(size_t idx = 0; idx < TryteLen; ++idx) StoreTrits(Trits, TryteToTritMapping[strchr(TryteGlyphs, Trytes[idx]) - TryteGlyphs], idx);
}

// Trytes must have len + 1 members allocated
// Trits must have len * 3 members allocated
void TritsToTrytes(char *Trytes, const int8_t *Trits, size_t len)
{
	for(size_t idx0 = 0, idx1 = 0; idx0 < len; ++idx0, idx1 += 3)
	{
		const int8_t tmp = Trits[idx1] + (Trits[idx1 + 1] * 3) + (Trits[idx1 + 2] * 9);
		Trytes[idx0] = TryteGlyphs[(tmp < 0) ? tmp + 27 : tmp];
	}
	
	// Append NULL terminator
	Trytes[len] = 0x00;
}

// Trytes must be a NULL terminated string.
bool ValidTryteString(char *Trytes)
{
	size_t TryteLen = strlen(Trytes);
	
	for(size_t i = 0; i < TryteLen; ++i)
	{
		if(!strchr(TryteGlyphs, Trytes[i])) return(false);
	}
	
	return(true);
}

#ifdef __linux__

TIME_TYPE MinerGetCurTime(void)
{
	TIME_TYPE CurTime;
	clock_gettime(CLOCK_REALTIME, &CurTime);
	return(CurTime);
}

double SecondsElapsed(TIME_TYPE Start, TIME_TYPE End)
{
	double NanosecondsElapsed = 1e9 * (double)(End.tv_sec - Start.tv_sec) + (double)(End.tv_nsec - Start.tv_nsec);
	return(NanosecondsElapsed * 1e-9);
}

#else

TIME_TYPE MinerGetCurTime(void)
{
	return(clock());
}

double SecondsElapsed(TIME_TYPE Start, TIME_TYPE End)
{
	return((double)(End - Start) / CLOCKS_PER_SEC);
}

#endif
