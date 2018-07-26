// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ThreadBackEnd.h
//  Version:     v1.00
//  Created:     07/05/2011 by Christopher Bolte
//  Compilers:   Visual Studio.NET
// -------------------------------------------------------------------------
//  History:
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "ThreadBackEnd.h"
#include "../JobManager.h"
#include "../../System.h"
#include "../../CPUDetect.h"
#include <thread>

///////////////////////////////////////////////////////////////////////////////
JobManager::ThreadBackEnd::CThreadBackEnd::CThreadBackEnd()
	: m_Semaphore()
	, m_nNumWorkerThreads(0)
{
	m_JobQueue.Init();

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	m_pBackEndWorkerProfiler = 0;
#endif
}

///////////////////////////////////////////////////////////////////////////////
JobManager::ThreadBackEnd::CThreadBackEnd::~CThreadBackEnd()
{
}

///////////////////////////////////////////////////////////////////////////////
bool JobManager::ThreadBackEnd::CThreadBackEnd::Init(uint32 nSysMaxWorker)
{
	const int numTempWorkerThreads = 2;
	// find out how many workers to create
#if CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	const uint32 numAvailableCores = 4;
#else
	const int numPhysicalCores = static_cast<int>(std::thread::hardware_concurrency());
	CRY_ASSERT_MESSAGE(numPhysicalCores > 0, "Failed to get physical core count");
	if (!numPhysicalCores)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Failed to get physical core count.");
	}

	// 2 cores are reserved for main thread and render thread,
	// and we still make sure at least one persistent worker thread is created
	const uint32 numAvailableCores = std::max(numPhysicalCores - 2, 1);
#endif

	uint32 numPersistentWorkerThreads = 0;

	if (nSysMaxWorker)
		numPersistentWorkerThreads = std::min(nSysMaxWorker, numAvailableCores);
	else
		numPersistentWorkerThreads = numAvailableCores;

	if (numPersistentWorkerThreads == 0)
		return false;

	m_nNumWorkerThreads = numPersistentWorkerThreads + numTempWorkerThreads;

	m_arrWorkerThreads.resize(m_nNumWorkerThreads);

	for (uint32 i = 0; i < m_nNumWorkerThreads; ++i)
	{
		const bool isTempWorker = i >= numPersistentWorkerThreads;
		m_arrWorkerThreads[i] = new CThreadBackEndWorkerThread(this, m_Semaphore, m_JobQueue, i, isTempWorker);

		const char* name = !isTempWorker ? "Worker" : "Helper";
		if (!gEnv->pThreadManager->SpawnThread(m_arrWorkerThreads[i], "JobSystem_%s_%u", name, i))
		{
			CryFatalError("Error spawning \"JobSystem_%s_%u\" thread.", name, i);
		}
	}
#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	m_pBackEndWorkerProfiler = new JobManager::CWorkerBackEndProfiler;
	m_pBackEndWorkerProfiler->Init(m_nNumWorkerThreads);
#endif

	return true;
}

///////////////////////////////////////////////////////////////////////////////
bool JobManager::ThreadBackEnd::CThreadBackEnd::ShutDown()
{
	// 1. Signal all threads to stop
	uint32 numOfStoppedThreads = 0;
	for (uint32 i = 0; i < m_arrWorkerThreads.size(); ++i)
	{
		if (m_arrWorkerThreads[i] == NULL)
			continue;
		m_arrWorkerThreads[i]->SignalStopWork();
		++numOfStoppedThreads;
	}

	// 2. Release semaphore count to wake up some/all threads waiting on the semaphore
	m_Semaphore.ReleaseAndSignal(); // Will notify all threads

	// 3. Wait for threads to exit and delete worker thread
	for (uint32 i = 0; i < m_arrWorkerThreads.size(); ++i)
	{
		if (m_arrWorkerThreads[i] == NULL)
			continue;

		if (gEnv->pThreadManager->JoinThread(m_arrWorkerThreads[i], eJM_Join))
		{
			delete m_arrWorkerThreads[i];
			m_arrWorkerThreads[i] = NULL;
		}
	}

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	SAFE_DELETE(m_pBackEndWorkerProfiler);
#endif

	return true;
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::ThreadBackEnd::CThreadBackEnd::AddJob(JobManager::CJobDelegator& crJob, const JobManager::TJobHandle cJobHandle, JobManager::SInfoBlock& rInfoBlock)
{
	uint32 nJobPriority = crJob.GetPriorityLevel();
	CJobManager* __restrict pJobManager = CJobManager::Instance();

	/////////////////////////////////////////////////////////////////////////////
	// Acquire Infoblock to use
	uint32 jobSlot;
	m_JobQueue.GetJobSlot(jobSlot, nJobPriority);

#if !defined(_RELEASE)
	pJobManager->IncreaseRunJobs();
#endif

	// copy info block into job queue
	JobManager::SInfoBlock& RESTRICT_REFERENCE rJobInfoBlock = m_JobQueue.jobInfoBlocks[nJobPriority][jobSlot];

	// since we will use the whole InfoBlock, and it is aligned to 128 bytes, clear the cacheline, this is faster than a cachemiss on write
#if CRY_PLATFORM_64BIT
	//STATIC_CHECK( sizeof(JobManager::SInfoBlock) == 512, ERROR_SIZE_OF_SINFOBLOCK_NOT_EQUALS_512 );
#else
	//STATIC_CHECK( sizeof(JobManager::SInfoBlock) == 384, ERROR_SIZE_OF_SINFOBLOCK_NOT_EQUALS_384 );
#endif

	// first cache line needs to be persistent
	ResetLine128(&rJobInfoBlock, 128);
	ResetLine128(&rJobInfoBlock, 256);
#if CRY_PLATFORM_64BIT
	ResetLine128(&rJobInfoBlock, 384);
#endif

	/////////////////////////////////////////////////////////////////////////////
	// Initialize the InfoBlock
	rInfoBlock.AssignMembersTo(&rJobInfoBlock);

	JobManager::CJobManager::CopyJobParameter(crJob.GetParamDataSize(), rJobInfoBlock.GetParamAddress(), crJob.GetJobParamData());

	assert(rInfoBlock.jobInvoker);

	const uint32 cJobId = cJobHandle->jobId;
	rJobInfoBlock.jobId = (unsigned char)cJobId;

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	assert(cJobId < JobManager::detail::eJOB_FRAME_STATS_MAX_SUPP_JOBS);
	m_pBackEndWorkerProfiler->RegisterJob(cJobId, pJobManager->GetJobName(rInfoBlock.jobInvoker));
	rJobInfoBlock.frameProfIndex = (unsigned char)m_pBackEndWorkerProfiler->GetProfileIndex();
#endif

	/////////////////////////////////////////////////////////////////////////////
	// initialization finished, make all visible for worker threads
	// the producing thread won't need the info block anymore, flush it from the cache
	FlushLine128(&rJobInfoBlock, 0);
	FlushLine128(&rJobInfoBlock, 128);
#if CRY_PLATFORM_64BIT
	FlushLine128(&rJobInfoBlock, 256);
	FlushLine128(&rJobInfoBlock, 384);
#endif

	MemoryBarrier();
	m_JobQueue.jobInfoBlockStates[nJobPriority][jobSlot].SetReady();

	// Release semaphore count to signal the workers that work is available
	m_Semaphore.ReleaseAndSignal();
}

///////////////////////////////////////////////////////////////////////////////
bool JobManager::ThreadBackEnd::CThreadBackEnd::KickTempWorker()
{
	for (CThreadBackEndWorkerThread* pWorker : m_arrWorkerThreads)
	{
		if (pWorker->KickTempWorker())
		{
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
bool JobManager::ThreadBackEnd::CThreadBackEnd::StopTempWorker()
{
	for (CThreadBackEndWorkerThread* pWorker : m_arrWorkerThreads)
	{
		if (pWorker->StopTempWorker())
		{
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
void JobManager::ThreadBackEnd::CThreadBackEndWorkerThread::SignalStopWork()
{
	m_bStop = true;

	if (m_pTempWorkerInfo)
	{
		CryAutoLock<CryMutexFast>(m_pTempWorkerInfo->doWorkLock);
		m_pTempWorkerInfo->doWorkCnd.Notify();
	}
}

inline CFrameProfiler* GetFrameProfilerForName(const char* name)
{
	static std::vector<std::pair<const char*, CFrameProfiler*>> s_profilers;
	static CryRWLock s_profilersLock;

	s_profilersLock.RLock();
	for (auto& p : s_profilers)
	{
		if (p.first == name) // compare pointer address. not content.
		{
			s_profilersLock.RUnlock();
			return p.second;
		}
	}
	s_profilersLock.RUnlock();

	s_profilersLock.WLock();
	for (auto& p : s_profilers)
	{
		if (p.first == name) // compare pointer address. not content.
		{
			s_profilersLock.WUnlock();
			return p.second;
		}
	}
	CFrameProfiler* pNewProfiler = new CFrameProfiler(PROFILE_SYSTEM, EProfileDescription::REGION, name, "", 0);
	s_profilers.reserve(256);
	s_profilers.push_back(std::make_pair(name, pNewProfiler));
	s_profilersLock.WUnlock();

	return pNewProfiler;
}

//////////////////////////////////////////////////////////////////////////
void JobManager::ThreadBackEnd::CThreadBackEndWorkerThread::ThreadEntry()
{
	// set up thread id
	JobManager::detail::SetWorkerThreadId(m_nId);

#if defined(JOB_SPIN_DURING_IDLE)
	HANDLE nThreadID = GetCurrentThread();
#endif

	uint64 nTicksInJobExecution = 0;
	const float fMinTimeInJobExecution = 1.0f;

	CJobManager* __restrict pJobManager = CJobManager::Instance();
	do
	{
		SInfoBlock infoBlock;
		uint32 nPriorityLevel = ~0;
		bool hasJob = false;

		// TEMP WORKER - Check if it should be active
		if (m_pTempWorkerInfo && !m_pTempWorkerInfo->doWork)
		{
			m_pTempWorkerInfo->doWorkLock.Lock();
			while (!m_pTempWorkerInfo->doWork && !m_bStop)
			{
				//CRY_PROFILE_REGION(PROFILE_SYSTEM, "TempWorker - In DISABLED state");
				m_pTempWorkerInfo->doWorkCnd.Wait(m_pTempWorkerInfo->doWorkLock);
			}
			m_pTempWorkerInfo->doWorkLock.Unlock();
			continue;
		}

		{

			//CRY_PROFILE_REGION(PROFILE_SYSTEM, "Get Job (Normal)");

			///////////////////////////////////////////////////////////////////////////
			// multiple steps to get a job of the queue
			// 1. get our job slot index
			uint64 currentPushIndex = ~0;
			uint64 currentPullIndex = ~0;
			uint64 newPullIndex = ~0;

			uint attemptToGetJobCount = 0;

			do
			{
				// Check if we have a job or we failed often enough to believe that there is no more work available.
				// We cannot Wait() here on the semaphore as there might be work on the fallback queue
				if (hasJob || attemptToGetJobCount++ > 5)
				{
					break;
				}

				// volatile load
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID// emulate a 64bit atomic read on PC platfom
				currentPullIndex = CryInterlockedCompareExchange64(alias_cast<volatile int64*>(&m_rJobQueue.pull.index), 0, 0);
				currentPushIndex = CryInterlockedCompareExchange64(alias_cast<volatile int64*>(&m_rJobQueue.push.index), 0, 0);
#else
				currentPullIndex = *const_cast<volatile uint64*>(&m_rJobQueue.pull.index);
				currentPushIndex = *const_cast<volatile uint64*>(&m_rJobQueue.push.index);
#endif
				// spin if the updated push ptr didn't reach us yet
				if (currentPushIndex == currentPullIndex)
				{
					continue;
				}

				// compute priority level from difference between push/pull
				if (!JobManager::SJobQueuePos::IncreasePullIndex(currentPullIndex, currentPushIndex, newPullIndex, nPriorityLevel,
				                                                 m_rJobQueue.GetMaxWorkerQueueJobs(eHighPriority), m_rJobQueue.GetMaxWorkerQueueJobs(eRegularPriority), m_rJobQueue.GetMaxWorkerQueueJobs(eLowPriority), m_rJobQueue.GetMaxWorkerQueueJobs(eStreamPriority)))
				{
					continue;
				}

				// stop spinning when we successfully got the index
				hasJob = CryInterlockedCompareExchange64(alias_cast<volatile int64*>(&m_rJobQueue.pull.index), newPullIndex, currentPullIndex) == currentPullIndex;

			}
			while (true);

			if (hasJob)
			{
				// We got a job, reduce the counter
				//CRY_PROFILE_REGION(PROFILE_SYSTEM, "Aquire");
				m_rWorkSyncVar.Aquire();
			}
			else
			{
				// Wait for new work
				{
					//CRY_PROFILE_REGION(PROFILE_SYSTEM, "Wait for work");
					// We failed to get a job. Check if there is still work available or wait for new work
					m_rWorkSyncVar.Wait();
				}

				// Try acquire work
				continue;
			}

			if (hasJob)
			{
				// compute our jobslot index from the only increasing publish index
				uint32 nExtractedCurIndex = static_cast<uint32>(JobManager::SJobQueuePos::ExtractIndex(currentPullIndex, nPriorityLevel));
				uint32 nNumWorkerQUeueJobs = m_rJobQueue.GetMaxWorkerQueueJobs(nPriorityLevel);
				uint32 nJobSlot = nExtractedCurIndex & (nNumWorkerQUeueJobs - 1);

				// 2. Wait till the produces has finished writing all data to the SInfoBlock
				JobManager::detail::SJobQueueSlotState* pJobInfoBlockState = &m_rJobQueue.jobInfoBlockStates[nPriorityLevel][nJobSlot];
				int iter = 0;
				while (!pJobInfoBlockState->IsReady())
				{
					//CRY_PROFILE_REGION(PROFILE_SYSTEM, "JobWorkerThread: About to sleep");
					CrySleep(0);
				}

				// 3. Get a local copy of the info block as as soon as it is ready to be used
				JobManager::SInfoBlock* pCurrentJobSlot = &m_rJobQueue.jobInfoBlocks[nPriorityLevel][nJobSlot];
				pCurrentJobSlot->AssignMembersTo(&infoBlock);
				JobManager::CJobManager::CopyJobParameter(infoBlock.paramSize << 4, infoBlock.GetParamAddress(), pCurrentJobSlot->GetParamAddress());

				// 4. Remark the job state as suspended
				MemoryBarrier();
				pJobInfoBlockState->SetNotReady();

				// 5. Mark the jobslot as free again
				MemoryBarrier();
				pCurrentJobSlot->Release((1 << JobManager::SJobQueuePos::eBitsPerPriorityLevel) / m_rJobQueue.GetMaxWorkerQueueJobs(nPriorityLevel));
			}

		}

		//CRY_PROFILE_REGION(PROFILE_SYSTEM, "JobWorkerThread: Execute Job");

		///////////////////////////////////////////////////////////////////////////
		// now we have a valid SInfoBlock to start work on it

		assert(infoBlock.jobInvoker);
		assert(infoBlock.GetParamAddress());

		// store job start time
#if defined(JOBMANAGER_SUPPORT_PROFILING)
		SJobProfilingData* pJobProfilingData = gEnv->GetJobManager()->GetProfilingData(infoBlock.profilerIndex);
		pJobProfilingData->nStartTime = gEnv->pTimer->GetAsyncTime();
		pJobProfilingData->nWorkerThread = GetWorkerThreadId();
#endif

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
		const uint64 nStartTime = JobManager::IWorkerBackEndProfiler::GetTimeSample();
#endif

		{
			// call delegator function to invoke job entry
#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
			const char* jobName = pJobManager->GetJobName(infoBlock.jobInvoker);

			char job_info[128];
			CFrameProfiler* pProfiler = GetFrameProfilerForName(jobName);
			CFrameProfilerSection frameProfilerSection2(pProfiler, jobName, jobName, EProfileDescription::SECTION);
			BROFILER_SECTION(jobName)

			cry_sprintf(job_info, "%s (Prio %u)", jobName, nPriorityLevel);

			CRYPROFILE_SCOPE_PROFILE_MARKER(job_info);
			CRYPROFILE_SCOPE_PLATFORM_MARKER(job_info);
#endif

			uint64 nJobStartTicks = CryGetTicks();

			if (infoBlock.jobLambdaInvoker)
			{
				infoBlock.jobLambdaInvoker();
			}
			else
			{
				(*infoBlock.jobInvoker)(infoBlock.GetParamAddress());
			}
			nTicksInJobExecution += CryGetTicks() - nJobStartTicks;
		}

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
		JobManager::IWorkerBackEndProfiler* workerProfiler = m_pThreadBackend->GetBackEndWorkerProfiler();
		const uint64 nEndTime = JobManager::IWorkerBackEndProfiler::GetTimeSample();
		workerProfiler->RecordJob(infoBlock.frameProfIndex, m_nId, static_cast<const uint32>(infoBlock.jobId), static_cast<const uint32>(nEndTime - nStartTime));
#endif

		IF (infoBlock.GetJobState(), 1)
		{
			SJobState* pJobState = infoBlock.GetJobState();
			pJobState->SetStopped();
		}
#if defined(JOBMANAGER_SUPPORT_PROFILING)
		pJobProfilingData->nEndTime = gEnv->pTimer->GetAsyncTime();
#endif

	}
	while (m_bStop == false);
}

///////////////////////////////////////////////////////////////////////////////
bool JobManager::ThreadBackEnd::CThreadBackEndWorkerThread::KickTempWorker()
{
	if (!IsTempWorker())
	{
		return false;
	}

	// Check if worker is already acquired
	if (!m_pTempWorkerInfo->doWork)
	{
		CryAutoLock<CryMutexFast>(m_pTempWorkerInfo->doWorkLock);

		// Check if some other thread managed to acquire the worker ahead of us
		if (!m_pTempWorkerInfo->doWork)
		{
			// We can acquire the worker
			m_pTempWorkerInfo->doWork = true; // Signal worker to stop work
			m_pTempWorkerInfo->doWorkCnd.Notify();
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
bool JobManager::ThreadBackEnd::CThreadBackEndWorkerThread::StopTempWorker()
{
	if (!IsTempWorker())
	{
		return false;
	}

	// Find an active worker
	if (m_pTempWorkerInfo->doWork)
	{
		CryAutoLock<CryMutexFast>(m_pTempWorkerInfo->doWorkLock);

		// Check if some other thread managed to modify this worker ahead of us
		if (m_pTempWorkerInfo->doWork)
		{
			// We can stop the worker
			m_pTempWorkerInfo->doWork = false; // Signal worker to stop work
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
ILINE void IncrQueuePullPointer(INT_PTR& rCurPullAddr, const INT_PTR cIncr, const INT_PTR cQueueStart, const INT_PTR cQueueEnd)
{
	const INT_PTR cNextPull = rCurPullAddr + cIncr;
	rCurPullAddr = (cNextPull >= cQueueEnd) ? cQueueStart : cNextPull;
}

///////////////////////////////////////////////////////////////////////////////
JobManager::ThreadBackEnd::CThreadBackEndWorkerThread::CThreadBackEndWorkerThread(CThreadBackEnd* pThreadBackend, detail::CWorkStatusSyncVar& rSemaphore, JobManager::SJobQueue_ThreadBackEnd& rJobQueue, uint32 nId, bool bIsTempWorker) :
	m_rWorkSyncVar(rSemaphore),
	m_rJobQueue(rJobQueue),
	m_bStop(false),
	m_nId(nId),
	m_pThreadBackend(pThreadBackend)
{
	m_pTempWorkerInfo = bIsTempWorker ? new STempWorkerInfo() : nullptr;
}

///////////////////////////////////////////////////////////////////////////////
JobManager::ThreadBackEnd::CThreadBackEndWorkerThread::~CThreadBackEndWorkerThread()
{
	SAFE_DELETE(m_pTempWorkerInfo);
}
