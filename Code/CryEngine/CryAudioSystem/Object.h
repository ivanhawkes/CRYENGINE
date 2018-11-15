// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "PropagationProcessor.h"
#include "Common/PoolObject.h"
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
class CEnvironment;
class CTrigger;
class CRequest;
struct SRequestData;

namespace Impl
{
struct IObject;
} // namespace Impl

enum class EObjectFlags : EnumFlagsType
{
	None                  = 0,
	InUse                 = BIT(0),
	Virtual               = BIT(1),
	CanRunOcclusion       = BIT(2),
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	TrackAbsoluteVelocity = BIT(3),
	TrackRelativeVelocity = BIT(4),
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EObjectFlags);

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

struct STriggerInstanceState final : public SUserDataBase
{
	ETriggerStatus flags = ETriggerStatus::None;
	ControlId      triggerId = InvalidControlId;
	size_t         numPlayingEvents = 0;
	size_t         numLoadingEvents = 0;
	float          expirationTimeMS = 0.0f;
	float          remainingTimeMS = 0.0f;
};

// CObject-related typedefs
using ObjectStandaloneFileMap = std::map<CStandaloneFile*, SUserDataBase>;
using ObjectEventSet = std::set<CEvent*>;
using ObjectTriggerStates = std::map<TriggerInstanceId, STriggerInstanceState>;

class CObject final : public IObject, public CPoolObject<CObject, stl::PSyncNone>
{
public:

	explicit CObject(CTransformation const& transformation)
		: m_pImplData(nullptr)
		, m_transformation(transformation)
		, m_flags(EObjectFlags::InUse)
		, m_propagationProcessor(*this)
		, m_entityId(INVALID_ENTITYID)
		, m_numPendingSyncCallbacks(0)
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		, m_maxRadius(0.0f)
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	{}

	CObject() = delete;
	CObject(CObject const&) = delete;
	CObject(CObject&&) = delete;
	CObject&       operator=(CObject const&) = delete;
	CObject&       operator=(CObject&&) = delete;

	ERequestStatus HandleStopTrigger(CTrigger const* const pTrigger);
	void           HandleSetTransformation(CTransformation const& transformation);
	void           HandleSetEnvironment(CEnvironment const* const pEnvironment, float const value);
	void           HandleSetOcclusionType(EOcclusionType const calcType);
	void           HandleSetOcclusionRayOffset(float const offset);
	void           HandleStopFile(char const* const szFile);

	void           Init(char const* const szName, Impl::IObject* const pImplData, EntityId const entityId);
	void           Release();

	// Callbacks
	void                           ReportStartedEvent(CEvent* const pEvent);
	void                           ReportFinishedEvent(CEvent* const pEvent, bool const bSuccess);
	void                           ReportFinishedStandaloneFile(CStandaloneFile* const pStandaloneFile);
	void                           GetStartedStandaloneFileRequestData(CStandaloneFile* const pStandaloneFile, CRequest& request);

	void                           StopAllTriggers();
	ObjectEventSet const&          GetActiveEvents() const { return m_activeEvents; }

	void                           SetOcclusion(float const occlusion);
	void                           ProcessPhysicsRay(CRayInfo* const pRayInfo);
	void                           UpdateOcclusion() { m_propagationProcessor.UpdateOcclusion(); }
	void                           ReleasePendingRays();

	ObjectStandaloneFileMap const& GetActiveStandaloneFiles() const               { return m_activeStandaloneFiles; }

	void                           SetImplDataPtr(Impl::IObject* const pImplData) { m_pImplData = pImplData; }
	Impl::IObject*                 GetImplDataPtr() const                         { return m_pImplData; }

	CTransformation const&         GetTransformation() const                      { return m_transformation; }

	bool                           IsActive() const;

	// Flags / Properties
	EObjectFlags GetFlags() const { return m_flags; }
	void         SetFlag(EObjectFlags const flag);
	void         RemoveFlag(EObjectFlags const flag);

	void         Update(float const deltaTime);
	bool         CanBeReleased() const;

	void         IncrementSyncCallbackCounter() { CryInterlockedIncrement(&m_numPendingSyncCallbacks); }
	void         DecrementSyncCallbackCounter() { CRY_ASSERT(m_numPendingSyncCallbacks >= 1); CryInterlockedDecrement(&m_numPendingSyncCallbacks); }

	void         AddEvent(CEvent* const pEvent);
	void         AddTriggerState(TriggerInstanceId const id, STriggerInstanceState const& triggerInstanceState);
	void         AddStandaloneFile(CStandaloneFile* const pStandaloneFile, SUserDataBase const& userDataBase);
	void         SendFinishedTriggerInstanceRequest(STriggerInstanceState const& triggerInstanceState);

private:

	// CryAudio::IObject
	virtual void     ExecuteTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     StopTrigger(ControlId const triggerId = InvalidControlId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetTransformation(CTransformation const& transformation, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetSwitchState(ControlId const switchId, SwitchStateId const switchStateId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetEnvironment(EnvironmentId const environmentId, float const amount, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetCurrentEnvironments(EntityId const entityToIgnore = 0, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetOcclusionType(EOcclusionType const occlusionType, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     SetOcclusionRayOffset(float const offset, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     LoadTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void     UnloadTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
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
	Impl::IObject*          m_pImplData;
	EObjectFlags            m_flags;
	CTransformation         m_transformation;
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

		CStateDebugDrawData(SwitchStateId const switchStateId);

		CStateDebugDrawData(CStateDebugDrawData const&) = delete;
		CStateDebugDrawData(CStateDebugDrawData&&) = delete;
		CStateDebugDrawData& operator=(CStateDebugDrawData const&) = delete;
		CStateDebugDrawData& operator=(CStateDebugDrawData&&) = delete;

		void                 Update(SwitchStateId const switchStateId);

		SwitchStateId m_currentStateId;
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
