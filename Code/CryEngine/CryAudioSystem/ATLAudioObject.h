// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "GlobalTypedefs.h"
#include "PropagationProcessor.h"
#include <PoolObject.h>
#include <CryAudio/IObject.h>
#include <CrySystem/TimeValue.h>

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
struct IRenderAuxGeom;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

namespace CryAudio
{
static constexpr ControlId OcclusionTypeSwitchId = CCrc32::ComputeLowercase_CompileTime(s_szOcclCalcSwitchName);
static constexpr SwitchStateId OcclusionTypeStateIds[IntegralValue(EOcclusionType::Count)] = {
	InvalidSwitchStateId,
	IgnoreStateId,
	AdaptiveStateId,
	LowStateId,
	MediumStateId,
	HighStateId };

class CSystem;
class CEventManager;
class CFileManager;

enum class ETriggerStatus : EnumFlagsType
{
	None                     = 0,
	Playing                  = BIT(0),
	Loaded                   = BIT(1),
	Loading                  = BIT(2),
	Unloading                = BIT(3),
	CallbackOnExternalThread = BIT(4),
	CallbackOnAudioThread    = BIT(5),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(ETriggerStatus);

struct SAudioTriggerImplState
{
	ETriggerStatus flags = ETriggerStatus::None;
};

struct SUserDataBase
{
	SUserDataBase() = default;

	explicit SUserDataBase(
		void* const pOwnerOverride_,
		void* const pUserData_,
		void* const pUserDataOwner_)
		: pOwnerOverride(pOwnerOverride_)
		, pUserData(pUserData_)
		, pUserDataOwner(pUserDataOwner_)
	{}

	void* pOwnerOverride = nullptr;
	void* pUserData = nullptr;
	void* pUserDataOwner = nullptr;
};

struct SAudioTriggerInstanceState final : public SUserDataBase
{
	ETriggerStatus flags = ETriggerStatus::None;
	ControlId      triggerId = InvalidControlId;
	size_t         numPlayingEvents = 0;
	size_t         numLoadingEvents = 0;
	float          expirationTimeMS = 0.0f;
	float          remainingTimeMS = 0.0f;
};

// CATLAudioObject-related typedefs
using ObjectStandaloneFileMap = std::map<CATLStandaloneFile*, SUserDataBase>;
using ObjectEventSet = std::set<CATLEvent*>;
using ObjectTriggerImplStates = std::map<TriggerImplId, SAudioTriggerImplState>;
using ObjectTriggerStates = std::map<TriggerInstanceId, SAudioTriggerInstanceState>;
using ObjectStateMap = std::map<ControlId, SwitchStateId>;
using ObjectParameterMap = std::map<ControlId, float>;
using ObjectEnvironmentMap = std::map<EnvironmentId, float>;

class CATLAudioObject final : public IObject, public CPoolObject<CATLAudioObject, stl::PSyncMultiThread>
{
public:

	CATLAudioObject();

	CATLAudioObject(CATLAudioObject const&) = delete;
	CATLAudioObject(CATLAudioObject&&) = delete;
	CATLAudioObject& operator=(CATLAudioObject const&) = delete;
	CATLAudioObject& operator=(CATLAudioObject&&) = delete;

	ERequestStatus   HandleStopTrigger(CTrigger const* const pTrigger);
	void             HandleSetTransformation(CObjectTransformation const& transformation, float const distanceToListener);
	void             HandleSetSwitchState(CATLSwitch const* const pSwitch, CATLSwitchState const* const pState);
	void             HandleSetEnvironment(CATLAudioEnvironment const* const pEnvironment, float const amount);
	void             HandleResetEnvironments(AudioEnvironmentLookup const& environmentsLookup);
	void             HandleSetOcclusionType(EOcclusionType const calcType, Vec3 const& listenerPosition);
	void             HandleStopFile(char const* const szFile);

	void             Init(char const* const szName, Impl::IObject* const pImplData, Vec3 const& listenerPosition, EntityId entityId);
	void             Release();

	// Callbacks
	void                           ReportStartedEvent(CATLEvent* const pEvent);
	void                           ReportFinishedEvent(CATLEvent* const pEvent, bool const bSuccess);
	void                           ReportFinishedStandaloneFile(CATLStandaloneFile* const pStandaloneFile);
	void                           ReportFinishedLoadingTriggerImpl(TriggerImplId const audioTriggerImplId, bool const bLoad);
	void                           GetStartedStandaloneFileRequestData(CATLStandaloneFile* const pStandaloneFile, CAudioRequest& request);

	void                           StopAllTriggers();
	ObjectEventSet const&          GetActiveEvents() const { return m_activeEvents; }

	void                           SetObstructionOcclusion(float const obstruction, float const occlusion);
	void                           ProcessPhysicsRay(CAudioRayInfo* const pAudioRayInfo);
	void                           ReleasePendingRays();

	ObjectStandaloneFileMap const& GetActiveStandaloneFiles() const               { return m_activeStandaloneFiles; }

	void                           SetImplDataPtr(Impl::IObject* const pImplData) { m_pImplData = pImplData; }
	Impl::IObject*                 GetImplDataPtr() const                         { return m_pImplData; }

	CObjectTransformation const&   GetTransformation()                            { return m_transformation; }

	// Flags / Properties
	EObjectFlags GetFlags() const { return m_flags; }
	void         SetFlag(EObjectFlags const flag);
	void         RemoveFlag(EObjectFlags const flag);
	float        GetMaxRadius() const { return m_maxRadius; }

	void         Update(
		float const deltaTime,
		float const distanceToListener,
		Vec3 const& listenerPosition,
		Vec3 const& listenerVelocity,
		bool const listenerMoved);
	bool CanBeReleased() const;

	void IncrementSyncCallbackCounter() { CryInterlockedIncrement(&m_numPendingSyncCallbacks); }
	void DecrementSyncCallbackCounter() { CRY_ASSERT(m_numPendingSyncCallbacks >= 1); CryInterlockedDecrement(&m_numPendingSyncCallbacks); }

	void AddEvent(CATLEvent* const pEvent);
	void AddTriggerState(TriggerInstanceId const id, SAudioTriggerInstanceState const& audioTriggerInstanceState);
	void AddStandaloneFile(CATLStandaloneFile* const pStandaloneFile, SUserDataBase const& userDataBase);
	void SendFinishedTriggerInstanceRequest(SAudioTriggerInstanceState const& audioTriggerInstanceState);

	static CSystem* s_pAudioSystem;

private:

	// CryAudio::IObject
	virtual void     ExecuteTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     StopTrigger(ControlId const triggerId = InvalidControlId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetTransformation(CObjectTransformation const& transformation, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetSwitchState(ControlId const audioSwitchId, SwitchStateId const audioSwitchStateId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetEnvironment(EnvironmentId const audioEnvironmentId, float const amount, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetCurrentEnvironments(EntityId const entityToIgnore = 0, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     ResetEnvironments(SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetOcclusionType(EOcclusionType const occlusionType, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     PlayFile(SPlayFileInfo const& playFileInfo, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     StopFile(char const* const szFile, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetName(char const* const szName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual EntityId GetEntityId() const override { return m_entityId; }
	// ~CryAudio::IObject

	void ReportFinishedTriggerInstance(ObjectTriggerStates::iterator const& iter);
	void PushRequest(SAudioRequestData const& requestData, SRequestUserData const& userData);
	bool HasActiveData(CATLAudioObject const* const pAudioObject) const;
	void UpdateControls(
		float const deltaTime,
		float const distanceToListener,
		Vec3 const& listenerPosition,
		Vec3 const& listenerVelocity,
		bool const listenerMoved);
	void TryToSetRelativeVelocity(float const relativeVelocity);
	void SetDefaultParameterValue(ControlId const id, float const value) const;
	void ExecuteDefaultTrigger(ControlId const id);

	ObjectStandaloneFileMap m_activeStandaloneFiles;
	ObjectEventSet          m_activeEvents;
	ObjectTriggerStates     m_triggerStates;
	ObjectTriggerImplStates m_triggerImplStates;
	ObjectEnvironmentMap    m_environments;
	ObjectStateMap          m_switchStates;
	Impl::IObject*          m_pImplData;
	float                   m_maxRadius;
	EObjectFlags            m_flags;
	float                   m_previousRelativeVelocity;
	float                   m_previousAbsoluteVelocity;
	CObjectTransformation   m_transformation;
	Vec3                    m_positionForVelocityCalculation;
	Vec3                    m_previousPositionForVelocityCalculation;
	Vec3                    m_velocity;
	CPropagationProcessor   m_propagationProcessor;
	EntityId                m_entityId;
	volatile int            m_numPendingSyncCallbacks;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
public:

	void DrawDebugInfo(
		IRenderAuxGeom& auxGeom,
		Vec3 const& listenerPosition,
		AudioTriggerLookup const& triggers,
		AudioParameterLookup const& parameters,
		AudioSwitchLookup const& switches,
		AudioPreloadRequestLookup const& preloadRequests,
		AudioEnvironmentLookup const& environments) const;
	void ResetObstructionRays() { m_propagationProcessor.ResetRayData(); }

	void ForceImplementationRefresh(
		AudioTriggerLookup const& triggers,
		AudioParameterLookup const& parameters,
		AudioSwitchLookup const& switches,
		AudioEnvironmentLookup const& environments,
		bool const bSet3DAttributes);

	ERequestStatus HandleSetName(char const* const szName);
	void           StoreParameterValue(ControlId const id, float const value);

	CryFixedStringT<MaxObjectNameLength> m_name;

private:

	class CStateDebugDrawData final
	{
	public:

		CStateDebugDrawData(SwitchStateId const audioSwitchState);

		CStateDebugDrawData(CStateDebugDrawData const&) = delete;
		CStateDebugDrawData(CStateDebugDrawData&&) = delete;
		CStateDebugDrawData& operator=(CStateDebugDrawData const&) = delete;
		CStateDebugDrawData& operator=(CStateDebugDrawData&&) = delete;

		void                 Update(SwitchStateId const audioSwitchState);

		SwitchStateId m_currentState;
		float         m_currentAlpha;

	private:

		static float const s_maxAlpha;
		static float const s_minAlpha;
		static int const   s_maxToMinUpdates;
	};

	char const* GetDefaultParameterName(ControlId const id) const;
	char const* GetDefaultTriggerName(ControlId const id) const;

	typedef std::map<ControlId, CStateDebugDrawData> StateDrawInfoMap;

	mutable StateDrawInfoMap m_stateDrawInfoMap;
	ObjectParameterMap       m_parameters;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};
} // namespace CryAudio
