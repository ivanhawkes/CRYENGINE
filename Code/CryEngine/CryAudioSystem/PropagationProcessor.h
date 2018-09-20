// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ATLEntities.h"
#include <CryPhysics/IPhysics.h>

namespace CryAudio
{
class CATLAudioObject;
struct SATLSoundPropagationData;

static const size_t s_maxRayHits = 10;

class CAudioRayInfo
{
public:

	CAudioRayInfo() = default;

	void Reset();

	CATLAudioObject* pObject = nullptr;
	size_t           samplePosIndex = 0;
	size_t           numHits = 0;
	float            totalSoundOcclusion = 0.0f;
	ray_hit          hits[s_maxRayHits];

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	Vec3  startPosition = ZERO;
	Vec3  direction = ZERO;
	float distanceToFirstObstacle = FLT_MAX;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};

class CPropagationProcessor
{
public:

	CPropagationProcessor() = delete;
	CPropagationProcessor(CPropagationProcessor const&) = delete;
	CPropagationProcessor(CPropagationProcessor&&) = delete;
	CPropagationProcessor& operator=(CPropagationProcessor const&) = delete;
	CPropagationProcessor& operator=(CPropagationProcessor&&) = delete;

	static bool s_bCanIssueRWIs;

	using RayInfoVec = std::vector<CAudioRayInfo>;
	using RayOcclusionVec = std::vector<float>;

	CPropagationProcessor(CObjectTransformation const& transformation, EObjectFlags& flags);
	~CPropagationProcessor();

	void Init(CATLAudioObject* const pObject);

	// PhysicsSystem callback
	static int  OnObstructionTest(EventPhys const* pEvent);
	static void UpdateOcclusionRayFlags();

	void        Update();
	void        SetOcclusionType(EOcclusionType const occlusionType);
	void        GetPropagationData(SATLSoundPropagationData& propagationData) const;
	void        ProcessPhysicsRay(CAudioRayInfo* const pAudioRayInfo);
	void        ReleasePendingRays();
	bool        HasPendingRays() const { return m_remainingRays > 0; }
	bool        HasNewOcclusionValues();
	void        UpdateOcclusion();
	void        SetOcclusionRayOffset(float const offset) { m_occlusionRayOffset = std::max(0.0f, offset); }

private:

	void ProcessObstructionOcclusion();
	void CastObstructionRay(
		Vec3 const& origin,
		size_t const rayIndex,
		size_t const samplePosIndex,
		bool const bSynch);
	void   RunObstructionQuery();
	void   ProcessLow(Vec3 const& up, Vec3 const& side, bool const bSynch);
	void   ProcessMedium(Vec3 const& up, Vec3 const& side, bool const bSynch);
	void   ProcessHigh(Vec3 const& up, Vec3 const& side, bool const bSynch);
	size_t GetNumConcurrentRays() const;
	size_t GetNumSamplePositions() const;
	void   UpdateOcclusionPlanes();
	bool   CanRunOcclusion();

	float                        m_lastQuerriedOcclusion;
	float                        m_occlusion;
	float                        m_currentListenerDistance;
	float                        m_occlusionRayOffset;
	RayOcclusionVec              m_raysOcclusion;

	size_t                       m_remainingRays;
	size_t                       m_rayIndex;

	CObjectTransformation const& m_transformation;
	EObjectFlags&                m_flags;

	RayInfoVec                   m_raysInfo;
	EOcclusionType               m_occlusionType;
	EOcclusionType               m_originalOcclusionType;
	EOcclusionType               m_occlusionTypeWhenAdaptive;

	static int                   s_occlusionRayFlags;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
public:

	static size_t s_totalSyncPhysRays;
	static size_t s_totalAsyncPhysRays;

	void           DrawDebugInfo(IRenderAuxGeom& auxGeom);
	EOcclusionType GetOcclusionType() const             { return m_occlusionType; }
	EOcclusionType GetOcclusionTypeWhenAdaptive() const { return m_occlusionTypeWhenAdaptive; }
	float          GetOcclusionRayOffset() const        { return m_occlusionRayOffset; }
	void           ResetRayData();

private:

	void DrawRay(IRenderAuxGeom& auxGeom, size_t const rayIndex) const;

	struct SRayDebugInfo
	{
		SRayDebugInfo() = default;

		Vec3  begin = ZERO;
		Vec3  end = ZERO;
		Vec3  stableEnd = ZERO;
		float occlusionValue = 0.0f;
		float distanceToNearestObstacle = FLT_MAX;
		int   numHits = 0;
	};

	using RayDebugInfoVec = std::vector<SRayDebugInfo>;

	RayDebugInfoVec m_rayDebugInfos;
	ColorB          m_listenerOcclusionPlaneColor;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};
} // namespace CryAudio
