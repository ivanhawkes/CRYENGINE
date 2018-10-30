// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "PropagationProcessor.h"
#include <PoolObject.h>
#include <CryAudio/IObject.h>
#include <CrySystem/TimeValue.h>

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
struct IRenderAuxGeom;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

namespace CryAudio
{
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

class CATLAudioObject final : public IObject, public CPoolObject<CATLAudioObject, stl::PSyncNone>
{
public:

	explicit CATLAudioObject(CObjectTransformation const& transformation);

	CATLAudioObject() = delete;
	CATLAudioObject(CATLAudioObject const&) = delete;
	CATLAudioObject(CATLAudioObject&&) = delete;
	CATLAudioObject& operator=(CATLAudioObject const&) = delete;
	CATLAudioObject& operator=(CATLAudioObject&&) = delete;

	ERequestStatus   HandleStopTrigger(CTrigger const* const pTrigger);
	void             HandleSetTransformation(CObjectTransformation const& transformation);
	void             HandleSetEnvironment(CATLAudioEnvironment const* const pEnvironment, float const value);
	void             HandleSetOcclusionType(EOcclusionType const calcType);
	void             HandleSetOcclusionRayOffset(float const offset);
	void             HandleStopFile(char const* const szFile);

	void             Init(char const* const szName, Impl::IObject* const pImplData, EntityId const entityId);
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
	void                           UpdateOcclusion() { m_propagationProcessor.UpdateOcclusion(); }
	void                           ReleasePendingRays();

	ObjectStandaloneFileMap const& GetActiveStandaloneFiles() const               { return m_activeStandaloneFiles; }

	void                           SetImplDataPtr(Impl::IObject* const pImplData) { m_pImplData = pImplData; }
	Impl::IObject*                 GetImplDataPtr() const                         { return m_pImplData; }

	CObjectTransformation const&   GetTransformation() const                      { return m_transformation; }

	bool                           IsActive() const;

	// Flags / Properties
	EObjectFlags GetFlags() const { return m_flags; }
	void         SetFlag(EObjectFlags const flag);
	void         RemoveFlag(EObjectFlags const flag);

	void         Update(float const deltaTime);
	bool         CanBeReleased() const;

	void         IncrementSyncCallbackCounter() { CryInterlockedIncrement(&m_numPendingSyncCallbacks); }
	void         DecrementSyncCallbackCounter() { CRY_ASSERT(m_numPendingSyncCallbacks >= 1); CryInterlockedDecrement(&m_numPendingSyncCallbacks); }

	void         AddEvent(CATLEvent* const pEvent);
	void         AddTriggerState(TriggerInstanceId const id, SAudioTriggerInstanceState const& audioTriggerInstanceState);
	void         AddStandaloneFile(CATLStandaloneFile* const pStandaloneFile, SUserDataBase const& userDataBase);
	void         SendFinishedTriggerInstanceRequest(SAudioTriggerInstanceState const& audioTriggerInstanceState);

private:

	// CryAudio::IObject
	virtual void     ExecuteTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     StopTrigger(ControlId const triggerId = InvalidControlId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetTransformation(CObjectTransformation const& transformation, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetSwitchState(ControlId const audioSwitchId, SwitchStateId const audioSwitchStateId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetEnvironment(EnvironmentId const audioEnvironmentId, float const amount, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetCurrentEnvironments(EntityId const entityToIgnore = 0, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetOcclusionType(EOcclusionType const occlusionType, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetOcclusionRayOffset(float const offset, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     PlayFile(SPlayFileInfo const& playFileInfo, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     StopFile(char const* const szFile, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetName(char const* const szName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual EntityId GetEntityId() const override { return m_entityId; }
	void             ToggleAbsoluteVelocityTracking(bool const enable, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	void             ToggleRelativeVelocityTracking(bool const enable, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	// ~CryAudio::IObject

	void ReportFinishedTriggerInstance(ObjectTriggerStates::iterator const& iter);
	void PushRequest(SRequestData const& requestData, SRequestUserData const& userData);
	bool ExecuteDefaultTrigger(ControlId const id);

	ObjectStandaloneFileMap m_activeStandaloneFiles;
	ObjectEventSet          m_activeEvents;
	ObjectTriggerStates     m_triggerStates;
	ObjectTriggerImplStates m_triggerImplStates;
	Impl::IObject*          m_pImplData;
	EObjectFlags            m_flags;
	CObjectTransformation   m_transformation;
	CPropagationProcessor   m_propagationProcessor;
	EntityId                m_entityId;
	volatile int            m_numPendingSyncCallbacks;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
public:

	void           DrawDebugInfo(IRenderAuxGeom& auxGeom);
	void           ResetObstructionRays()        { m_propagationProcessor.ResetRayData(); }
	float          GetMaxRadius() const          { return m_maxRadius; }
	float          GetOcclusionRayOffset() const { return m_propagationProcessor.GetOcclusionRayOffset(); }

	void           ForceImplementationRefresh(bool const setTransformation);

	ERequestStatus HandleSetName(char const* const szName);
	void           StoreParameterValue(ControlId const id, float const value);
	void           StoreSwitchValue(ControlId const switchId, SwitchStateId const switchStateId);
	void           StoreEnvironmentValue(ControlId const id, float const value);

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
		float         m_currentSwitchColor;

	private:

		static float const s_maxSwitchColor;
		static float const s_minSwitchColor;
		static int const   s_maxToMinUpdates;
	};

	using StateDrawInfoMap = std::map<ControlId, CStateDebugDrawData>;
	mutable StateDrawInfoMap m_stateDrawInfoMap;

	using SwitchStates = std::map<ControlId, SwitchStateId>;
	using Parameters = std::map<ControlId, float>;
	using Environments = std::map<EnvironmentId, float>;

	Parameters   m_parameters;
	SwitchStates m_switchStates;
	Environments m_environments;
	float        m_maxRadius;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};
} // namespace CryAudio
