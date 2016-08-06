#define HASH_SIZE		243
#define STATE_SIZE		HASH_SIZE * 3

char TLookup(char idx)
{
	char A, B, C, D;
	
	D = idx & 1;
	C = (idx >> 1) & 1;
	B = (idx >> 2) & 1;
	A = (idx >> 3) & 1;
	
	char f1 = (B & (~C) & D) | (A & (~C) & (~D)) | ((~A) & (~B) & C & (~D));
	if(f1 & 1) return(-1);
	
	char f0 = (A & D) |  (~(A | C | D));
	return(f0 & 1);
}

//#define Indices(idx) 	(mul24(364, idx) % 729)

void TransformBlock(char *state, __local short *Indices)
{
	#pragma unroll 3
	for(int r = 0; r < 27; ++r)
	{
		char stateCopy[STATE_SIZE + 1];
		
		#pragma unroll 3
		for(int i = 0; i < STATE_SIZE; ++i) stateCopy[i] = state[i];
		
		#pragma unroll 3
		for(int i = 0; i < STATE_SIZE; ++i)
			state[i] = TLookup(stateCopy[Indices[i]] + (stateCopy[Indices[i+1]] << 2) + 5 );
	}
}

__kernel void curl(__global char *OutBuffer, __global const char *InBuffer, __global const char *MidBuffer, const uint Difficulty)
{
	uint Nonce = get_global_id(0);
	char State[STATE_SIZE + 1] = { 0 };
	__local short Indices[STATE_SIZE];
	bool Found = true;
	
	for(int i = get_local_id(0), x = 0; x < 12; i += get_local_size(0), ++x)
	{
		Indices[i] = (i * 364) % 729;
	}
	
	if(get_local_id(0) < 25)
	{
		Indices[704 + get_local_id(0)] = ((704 + get_local_id(0)) * 364) % 729;
	}
	
	mem_fence(CLK_LOCAL_MEM_FENCE);
	
	for(uint i = 0, NonceCopy = Nonce; i < 32; ++i, NonceCopy >>= 1) State[i] = NonceCopy & 1;
	
	#pragma unroll 1
	for(int i = HASH_SIZE; i < STATE_SIZE; ++i) State[i] = MidBuffer[i];
	
	TransformBlock(State, Indices);
	
	#pragma unroll 1
	for(int i = 0; i < 2; ++i)
	{
		#pragma unroll 1
		for(int x = i * HASH_SIZE, z = 0; x < ((i + 1) * HASH_SIZE); ++x, ++z) State[z] = InBuffer[x];
			
		TransformBlock(State, Indices);
	}
	
	for(int i = 0; i < Difficulty && Found; ++i) Found = (State[HASH_SIZE - i - 1] != 0);
	
	if(Found)
	{
		for(int i = 0; i < 32; ++i, Nonce >>= 1) OutBuffer[i] = Nonce & 1;
		
		#pragma unroll 1
		for(int i = 32; i < HASH_SIZE; ++i) OutBuffer[i] = 0;
	}
}
	
