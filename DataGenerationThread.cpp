#include <iostream>
#include <time.h>
#include <unistd.h>

#include "ChainWalkContext.h"
#include "DataGenerationThread.h"

CDataGenerationThread::CDataGenerationThread(void)
{
	memset(zBuffer, 0x00, sizeof(zBuffer));
	bDataReady = 0;
	bShutdown = 0;
	m_nChainsCalculated = 0;
}

CDataGenerationThread::~CDataGenerationThread(void)
{
}

void CDataGenerationThread::threadProc()
{
	#ifdef WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
	#else
	#endif

	const DataGenerationThreadParameters *Parameters = (const DataGenerationThreadParameters *)Params;
	CChainWalkContext cwc;
	// Make a pointer to the beginning of the data block
	uint64 *ptrCurrent = (uint64*)zBuffer;
	// Make a pointer to the end of the data block.
	uint64 *ptrEnd = (uint64*)(zBuffer + DATA_CHUNK_SIZE);
	uint64 nSeed = Parameters->nChainStart;	
	while(m_nChainsCalculated < Parameters->nChainCount && bTerminateThreadFlag != 1)
	{
		*ptrCurrent = nSeed;
		cwc.SetIndex(nSeed++);
		// Increase the pointer location
		ptrCurrent++;
		int nPos;
		for (nPos = 0; nPos < Parameters->nRainbowChainLen - 1; nPos++)
		{
			cwc.IndexToPlain();
			cwc.PlainToHash();
			cwc.HashToIndex(nPos);
		}

		*ptrCurrent = cwc.GetIndex();
		// Increase the pointer location
		ptrCurrent++;
		// We counted another chain
		m_nChainsCalculated++;
		// The data buffer is full.. Let's swap buffers
		if(ptrCurrent >= ptrEnd)
		{
			// The old buffer isn't emptied yet. So we have to wait before writing our data
			while(bDataReady == 1 && bTerminateThreadFlag != 1)
			{
				Sleep(1);
			}
			// Copy the data over
			memcpy(zDataChunk, zBuffer, DATA_CHUNK_SIZE);
			// Mark the buffer as full
			bDataReady = 1;
			// Reset the data pointer
			ptrCurrent = (uint64*)zBuffer;			
		}
	}
	bTerminateThreadFlag = 1;
}
