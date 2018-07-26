// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ATLUtils.h"
#include "AudioInternalInterfaces.h"
#include "Common/SharedAudioData.h"
#include "Common.h"
#include <CryAudio/IListener.h>
#include <CrySystem/IStreamEngine.h>
#include <CrySystem/TimeValue.h>
#include <PoolObject.h>
#include <CryString/HashedString.h>

namespace CryAudio
{
struct SAudioTriggerInstanceState;

struct SATLXMLTags
{
	static char const* const szPlatform;
};

namespace Impl
{
struct IObject;
struct IListener;
struct ITrigger;
struct IParameter;
struct ISwitchState;
struct IEnvironment;
struct IEvent;
struct IFile;
struct IStandaloneFile;
} // namespace Impl

enum class EObjectFlags : EnumFlagsType
{
	None                            = 0,
	MovingOrDecaying                = BIT(0),
	TrackAbsoluteVelocity           = BIT(1),
	TrackRelativeVelocity           = BIT(2),
	InUse                           = BIT(3),
	Virtual                         = BIT(4),
	WaitingForInitialTransformation = BIT(5),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EObjectFlags);

enum class EFileFlags : EnumFlagsType
{
	None                      = 0,
	Cached                    = BIT(0),
	NotCached                 = BIT(1),
	NotFound                  = BIT(2),
	MemAllocFail              = BIT(3),
	Removable                 = BIT(4),
	Loading                   = BIT(5),
	UseCounted                = BIT(6),
	NeedsResetToManualLoading = BIT(7),
	Localized                 = BIT(8),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EFileFlags);

template<typename IDType>
class CATLEntity
{
public:

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	explicit CATLEntity(IDType const id, EDataScope const dataScope, char const* const szName)
		: m_id(id)
		, m_dataScope(dataScope)
		, m_name(szName)
	{}
#else
	explicit CATLEntity(IDType const id, EDataScope const dataScope)
		: m_id(id)
		, m_dataScope(dataScope)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	CATLEntity() = delete;
	CATLEntity(CATLEntity const&) = delete;
	CATLEntity(CATLEntity&&) = delete;
	CATLEntity& operator=(CATLEntity const&) = delete;
	CATLEntity& operator=(CATLEntity&&) = delete;

	IDType      GetId() const        { return m_id; }
	EDataScope  GetDataScope() const { return m_dataScope; }

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	char const* GetName() const { return m_name.c_str(); }
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

protected:

	~CATLEntity() = default;

private:

	IDType const     m_id;
	EDataScope const m_dataScope;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};

using Control = CATLEntity<ControlId>;

struct SATLSoundPropagationData
{
	float obstruction = 0.0f;
	float occlusion = 0.0f;
};

class CATLListener final : public CryAudio::IListener
{
public:

	CATLListener() = delete;
	CATLListener(CATLListener const&) = delete;
	CATLListener(CATLListener&&) = delete;
	CATLListener& operator=(CATLListener const&) = delete;
	CATLListener& operator=(CATLListener&&) = delete;

	explicit CATLListener(Impl::IListener* const pImplData)
		: m_pImplData(pImplData)
		, m_isMovingOrDecaying(false)
	{}

	// CryAudio::IListener
	virtual void SetTransformation(CObjectTransformation const& transformation, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	virtual void SetName(char const* const szName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) override;
	// ~CryAudio::IListener

	void                         Update(float const deltaTime);
	void                         HandleSetTransformation(CObjectTransformation const& transformation);
	CObjectTransformation const& GetTransformation() const { return m_transformation; }
	Vec3 const&                  GetVelocity() const       { return m_velocity; }

	Impl::IListener* m_pImplData;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	void HandleSetName(char const* const szName);
	CryFixedStringT<MaxObjectNameLength> m_name;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

private:

	bool                  m_isMovingOrDecaying;
	CObjectTransformation m_transformation;
	Vec3                  m_previousPositionForVelocityCalculation{ ZERO };
	Vec3                  m_velocity{ ZERO };
};

class CATLControlImpl
{
public:

	CATLControlImpl() = default;
	CATLControlImpl(CATLControlImpl const&) = delete;
	CATLControlImpl(CATLControlImpl&&) = delete;
	CATLControlImpl& operator=(CATLControlImpl const&) = delete;
	CATLControlImpl& operator=(CATLControlImpl&&) = delete;
};

class CATLTriggerImpl : public CATLControlImpl
{
public:

	CATLTriggerImpl() = delete;
	CATLTriggerImpl(CATLTriggerImpl const&) = delete;
	CATLTriggerImpl(CATLTriggerImpl&&) = delete;
	CATLTriggerImpl& operator=(CATLTriggerImpl const&) = delete;
	CATLTriggerImpl& operator=(CATLTriggerImpl&&) = delete;

	explicit CATLTriggerImpl(
		TriggerImplId const audioTriggerImplId,
		Impl::ITrigger const* const pImplData = nullptr)
		: m_audioTriggerImplId(audioTriggerImplId)
		, m_pImplData(pImplData)
	{}

	virtual ~CATLTriggerImpl();

	virtual ERequestStatus Execute(Impl::IObject* const pImplObject, Impl::IEvent* const pImplEvent) const;

	TriggerImplId const         m_audioTriggerImplId;
	Impl::ITrigger const* const m_pImplData;
};

using TriggerConnections = std::vector<CATLTriggerImpl const*>;

class CTrigger final : public Control
{
public:

	CTrigger() = delete;
	CTrigger(CTrigger const&) = delete;
	CTrigger(CTrigger&&) = delete;
	CTrigger& operator=(CTrigger const&) = delete;
	CTrigger& operator=(CTrigger&&) = delete;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	explicit CTrigger(
		ControlId const id,
		EDataScope const dataScope,
		TriggerConnections const& connections,
		float const radius,
		char const* const szName)
		: Control(id, dataScope, szName)
		, m_connections(connections)
		, m_radius(radius)
	{}
#else
	explicit CTrigger(
		ControlId const id,
		EDataScope const dataScope,
		TriggerConnections const& connections,
		float const radius)
		: Control(id, dataScope)
		, m_connections(connections)
		, m_radius(radius)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	~CTrigger();

	void Execute(
		CATLAudioObject& object,
		void* const pOwner = nullptr,
		void* const pUserData = nullptr,
		void* const pUserDataOwner = nullptr,
		ERequestFlags const flags = ERequestFlags::None) const;
	void Execute(
		CATLAudioObject& object,
		TriggerInstanceId const triggerInstanceId,
		SAudioTriggerInstanceState& triggerInstanceState) const;
	void LoadAsync(CATLAudioObject& object, bool const doLoad) const;
	void PlayFile(
		CATLAudioObject& object,
		char const* const szName,
		bool const isLocalized,
		void* const pOwner = nullptr,
		void* const pUserData = nullptr,
		void* const pUserDataOwner = nullptr) const;
	float GetRadius() const { return m_radius; }

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	void PlayFile(CATLAudioObject& object, CATLStandaloneFile* const pFile) const;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

private:

	TriggerConnections const m_connections;
	float const              m_radius;
};

class CLoseFocusTrigger final : public Control
{
public:

	CLoseFocusTrigger() = delete;
	CLoseFocusTrigger(CLoseFocusTrigger const&) = delete;
	CLoseFocusTrigger(CLoseFocusTrigger&&) = delete;
	CLoseFocusTrigger& operator=(CLoseFocusTrigger const&) = delete;
	CLoseFocusTrigger& operator=(CLoseFocusTrigger&&) = delete;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	explicit CLoseFocusTrigger(TriggerConnections const& connections)
		: Control(LoseFocusTriggerId, EDataScope::Global, s_szLoseFocusTriggerName)
		, m_connections(connections)
	{}
#else
	explicit CLoseFocusTrigger(TriggerConnections const& connections)
		: Control(LoseFocusTriggerId, EDataScope::Global)
		, m_connections(connections)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	~CLoseFocusTrigger();

	void Execute() const;

private:

	TriggerConnections const m_connections;
};

class CGetFocusTrigger final : public Control
{
public:

	CGetFocusTrigger() = delete;
	CGetFocusTrigger(CGetFocusTrigger const&) = delete;
	CGetFocusTrigger(CGetFocusTrigger&&) = delete;
	CGetFocusTrigger& operator=(CGetFocusTrigger const&) = delete;
	CGetFocusTrigger& operator=(CGetFocusTrigger&&) = delete;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	explicit CGetFocusTrigger(TriggerConnections const& connections)
		: Control(GetFocusTriggerId, EDataScope::Global, s_szGetFocusTriggerName)
		, m_connections(connections)
	{}
#else
	explicit CGetFocusTrigger(TriggerConnections const& connections)
		: Control(GetFocusTriggerId, EDataScope::Global)
		, m_connections(connections)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	~CGetFocusTrigger();

	void Execute() const;

private:

	TriggerConnections const m_connections;
};

class CMuteAllTrigger final : public Control
{
public:

	CMuteAllTrigger() = delete;
	CMuteAllTrigger(CMuteAllTrigger const&) = delete;
	CMuteAllTrigger(CMuteAllTrigger&&) = delete;
	CMuteAllTrigger& operator=(CMuteAllTrigger const&) = delete;
	CMuteAllTrigger& operator=(CMuteAllTrigger&&) = delete;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	explicit CMuteAllTrigger(TriggerConnections const& connections)
		: Control(MuteAllTriggerId, EDataScope::Global, s_szMuteAllTriggerName)
		, m_connections(connections)
	{}
#else
	explicit CMuteAllTrigger(TriggerConnections const& connections)
		: Control(MuteAllTriggerId, EDataScope::Global)
		, m_connections(connections)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	~CMuteAllTrigger();

	void Execute() const;

private:

	TriggerConnections const m_connections;
};

class CUnmuteAllTrigger final : public Control
{
public:

	CUnmuteAllTrigger() = delete;
	CUnmuteAllTrigger(CUnmuteAllTrigger const&) = delete;
	CUnmuteAllTrigger(CUnmuteAllTrigger&&) = delete;
	CUnmuteAllTrigger& operator=(CUnmuteAllTrigger const&) = delete;
	CUnmuteAllTrigger& operator=(CUnmuteAllTrigger&&) = delete;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	explicit CUnmuteAllTrigger(TriggerConnections const& connections)
		: Control(UnmuteAllTriggerId, EDataScope::Global, s_szUnmuteAllTriggerName)
		, m_connections(connections)
	{}
#else
	explicit CUnmuteAllTrigger(TriggerConnections const& connections)
		: Control(UnmuteAllTriggerId, EDataScope::Global)
		, m_connections(connections)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	~CUnmuteAllTrigger();

	void Execute() const;

private:

	TriggerConnections const m_connections;
};

class CPauseAllTrigger final : public Control
{
public:

	CPauseAllTrigger() = delete;
	CPauseAllTrigger(CPauseAllTrigger const&) = delete;
	CPauseAllTrigger(CPauseAllTrigger&&) = delete;
	CPauseAllTrigger& operator=(CPauseAllTrigger const&) = delete;
	CPauseAllTrigger& operator=(CPauseAllTrigger&&) = delete;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	explicit CPauseAllTrigger(TriggerConnections const& connections)
		: Control(PauseAllTriggerId, EDataScope::Global, s_szPauseAllTriggerName)
		, m_connections(connections)
	{}
#else
	explicit CPauseAllTrigger(TriggerConnections const& connections)
		: Control(PauseAllTriggerId, EDataScope::Global)
		, m_connections(connections)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	~CPauseAllTrigger();

	void Execute() const;

private:

	TriggerConnections const m_connections;
};

class CResumeAllTrigger final : public Control
{
public:

	CResumeAllTrigger() = delete;
	CResumeAllTrigger(CResumeAllTrigger const&) = delete;
	CResumeAllTrigger(CResumeAllTrigger&&) = delete;
	CResumeAllTrigger& operator=(CResumeAllTrigger const&) = delete;
	CResumeAllTrigger& operator=(CResumeAllTrigger&&) = delete;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	explicit CResumeAllTrigger(TriggerConnections const& connections)
		: Control(ResumeAllTriggerId, EDataScope::Global, s_szResumeAllTriggerName)
		, m_connections(connections)
	{}
#else
	explicit CResumeAllTrigger(TriggerConnections const& connections)
		: Control(ResumeAllTriggerId, EDataScope::Global)
		, m_connections(connections)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	~CResumeAllTrigger();

	void Execute() const;

private:

	TriggerConnections const m_connections;
};

// Class for a parameter associated with a middleware parameter
class CParameterImpl final : public CATLControlImpl
{
public:

	CParameterImpl() = default;
	CParameterImpl(CParameterImpl const&) = delete;
	CParameterImpl(CParameterImpl&&) = delete;
	CParameterImpl& operator=(CParameterImpl const&) = delete;
	CParameterImpl& operator=(CParameterImpl&&) = delete;

	explicit CParameterImpl(Impl::IParameter const* const pImplData)
		: m_pImplData(pImplData)
	{}

	virtual ~CParameterImpl();

	virtual void Set(CATLAudioObject const& audioObject, float const value) const;

private:

	Impl::IParameter const* const m_pImplData = nullptr;
};

using ParameterConnections = std::vector<CParameterImpl const*>;

class CParameter final : public Control
{
public:

	CParameter() = delete;
	CParameter(CParameter const&) = delete;
	CParameter(CParameter&&) = delete;
	CParameter& operator=(CParameter const&) = delete;
	CParameter& operator=(CParameter&&) = delete;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	explicit CParameter(
		ControlId const id,
		EDataScope const dataScope,
		ParameterConnections const& connections,
		char const* const szName)
		: Control(id, dataScope, szName)
		, m_connections(connections)
	{}
#else
	explicit CParameter(
		ControlId const id,
		EDataScope const dataScope,
		ParameterConnections const& connections)
		: Control(id, dataScope)
		, m_connections(connections)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	~CParameter();

	void Set(CATLAudioObject const& object, float const value) const;

private:

	ParameterConnections const m_connections;
};

class CAbsoluteVelocityParameter final : public Control
{
public:

	CAbsoluteVelocityParameter() = delete;
	CAbsoluteVelocityParameter(CAbsoluteVelocityParameter const&) = delete;
	CAbsoluteVelocityParameter(CAbsoluteVelocityParameter&&) = delete;
	CAbsoluteVelocityParameter& operator=(CAbsoluteVelocityParameter const&) = delete;
	CAbsoluteVelocityParameter& operator=(CAbsoluteVelocityParameter&&) = delete;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	explicit CAbsoluteVelocityParameter(ParameterConnections const& connections)
		: Control(AbsoluteVelocityParameterId, EDataScope::Global, s_szAbsoluteVelocityParameterName)
		, m_connections(connections)
	{}
#else
	explicit CAbsoluteVelocityParameter(ParameterConnections const& connections)
		: Control(AbsoluteVelocityParameterId, EDataScope::Global)
		, m_connections(connections)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	~CAbsoluteVelocityParameter();

	void Set(CATLAudioObject const& object, float const value) const;

private:

	ParameterConnections const m_connections;
};

class CRelativeVelocityParameter final : public Control
{
public:

	CRelativeVelocityParameter() = delete;
	CRelativeVelocityParameter(CRelativeVelocityParameter const&) = delete;
	CRelativeVelocityParameter(CRelativeVelocityParameter&&) = delete;
	CRelativeVelocityParameter& operator=(CRelativeVelocityParameter const&) = delete;
	CRelativeVelocityParameter& operator=(CRelativeVelocityParameter&&) = delete;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	explicit CRelativeVelocityParameter(ParameterConnections const& connections)
		: Control(RelativeVelocityParameterId, EDataScope::Global, s_szRelativeVelocityParameterName)
		, m_connections(connections)
	{}
#else
	explicit CRelativeVelocityParameter(ParameterConnections const& connections)
		: Control(RelativeVelocityParameterId, EDataScope::Global)
		, m_connections(connections)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	~CRelativeVelocityParameter();

	void Set(CATLAudioObject const& object, float const value) const;

private:

	ParameterConnections const m_connections;
};

class IAudioSwitchStateImpl : public CATLControlImpl
{
public:

	virtual ~IAudioSwitchStateImpl() = default;

	virtual void Set(CATLAudioObject& audioObject) const = 0;
};

class CExternalAudioSwitchStateImpl final : public IAudioSwitchStateImpl
{
public:

	explicit CExternalAudioSwitchStateImpl(Impl::ISwitchState const* const pImplData)
		: m_pImplData(pImplData)
	{}

	virtual ~CExternalAudioSwitchStateImpl() override;

	// IAudioSwitchStateImpl
	virtual void Set(CATLAudioObject& audioObject) const override;
	// ~IAudioSwitchStateImpl

private:

	Impl::ISwitchState const* const m_pImplData;
};

class CATLSwitchState final
{
public:

	using ImplPtrVec = std::vector<IAudioSwitchStateImpl const*>;

	CATLSwitchState() = delete;
	CATLSwitchState(CATLSwitchState const&) = delete;
	CATLSwitchState(CATLSwitchState&&) = delete;
	CATLSwitchState& operator=(CATLSwitchState const&) = delete;
	CATLSwitchState& operator=(CATLSwitchState&&) = delete;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	explicit CATLSwitchState(
		ControlId const audioSwitchId,
		SwitchStateId const audioSwitchStateId,
		ImplPtrVec const& implPtrs,
		char const* const szName)
		: m_audioSwitchStateId(audioSwitchStateId)
		, m_audioSwitchId(audioSwitchId)
		, m_implPtrs(implPtrs)
		, m_name(szName)
	{}
#else
	explicit CATLSwitchState(
		ControlId const audioSwitchId,
		SwitchStateId const audioSwitchStateId,
		ImplPtrVec const& implPtrs)
		: m_audioSwitchStateId(audioSwitchStateId)
		, m_audioSwitchId(audioSwitchId)
		, m_implPtrs(implPtrs)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	SwitchStateId GetId() const       { return m_audioSwitchStateId; }
	SwitchStateId GetParentId() const { return m_audioSwitchId; }

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	char const* GetName() const { return m_name.c_str(); }
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	ImplPtrVec const m_implPtrs;

private:

	SwitchStateId const m_audioSwitchStateId;
	ControlId const     m_audioSwitchId;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};

class CATLSwitch final : public Control
{
public:

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	explicit CATLSwitch(ControlId const audioSwitchId, EDataScope const dataScope, char const* const szName)
		: Control(audioSwitchId, dataScope, szName)
	{}
#else
	explicit CATLSwitch(ControlId const audioSwitchId, EDataScope const dataScope)
		: Control(audioSwitchId, dataScope)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	using AudioStates = std::map<SwitchStateId, CATLSwitchState const*>;
	AudioStates audioSwitchStates;
};

class CATLEnvironmentImpl final : public CATLControlImpl
{
public:

	explicit CATLEnvironmentImpl(Impl::IEnvironment const* const pImplData)
		: m_pImplData(pImplData)
	{}

	~CATLEnvironmentImpl();

	Impl::IEnvironment const* const m_pImplData;
};

class CATLAudioEnvironment final : public CATLEntity<EnvironmentId>
{
public:

	using ImplPtrVec = std::vector<CATLEnvironmentImpl const*>;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	explicit CATLAudioEnvironment(EnvironmentId const audioEnvironmentId, EDataScope const dataScope, ImplPtrVec const& implPtrs, char const* const szName)
		: CATLEntity<EnvironmentId>(audioEnvironmentId, dataScope, szName)
		, m_implPtrs(implPtrs)
	{}
#else
	explicit CATLAudioEnvironment(EnvironmentId const audioEnvironmentId, EDataScope const dataScope, ImplPtrVec const& implPtrs)
		: CATLEntity<EnvironmentId>(audioEnvironmentId, dataScope)
		, m_implPtrs(implPtrs)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	ImplPtrVec const m_implPtrs;
};

class CATLStandaloneFile final : public CPoolObject<CATLStandaloneFile, stl::PSyncNone>
{
public:

	explicit CATLStandaloneFile() = default;

	bool IsPlaying() const { return (m_state == EAudioStandaloneFileState::Playing) || (m_state == EAudioStandaloneFileState::Stopping); }

	CATLAudioObject*          m_pAudioObject = nullptr;
	Impl::IStandaloneFile*    m_pImplData = nullptr;
	EAudioStandaloneFileState m_state = EAudioStandaloneFileState::None;
	CHashedString             m_hashedFilename;

	// Needed only during middleware switch.
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	ControlId m_triggerId = InvalidControlId;
	bool      m_isLocalized = true;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};

class CATLEvent final : public CPoolObject<CATLEvent, stl::PSyncNone>
{
public:

	CATLEvent() = default;

	void      Release();
	void      Stop();
	void      SetDataScope(EDataScope const dataScope) { m_dataScope = dataScope; }
	bool      IsPlaying() const                        { return m_state == EEventState::Playing || m_state == EEventState::PlayingDelayed; }
	void      SetTriggerId(ControlId const id)         { m_triggerId = id; }
	ControlId GetTriggerId() const                     { return m_triggerId; }
	void      SetTriggerRadius(float const radius)     { m_triggerRadius = radius; }
	float     GetTriggerRadius() const                 { return m_triggerRadius; }

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	void              SetTriggerName(char const* const szTriggerName) { m_szTriggerName = szTriggerName; }
	char const* const GetTriggerName() const                          { return m_szTriggerName; }
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	EDataScope        m_dataScope = EDataScope::None;
	CATLAudioObject*  m_pAudioObject = nullptr;
	TriggerImplId     m_audioTriggerImplId = InvalidTriggerImplId;
	TriggerInstanceId m_audioTriggerInstanceId = InvalidTriggerInstanceId;
	EEventState       m_state = EEventState::None;
	Impl::IEvent*     m_pImplData = nullptr;

private:

	ControlId m_triggerId = InvalidControlId;
	float     m_triggerRadius = 0.0f;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	char const* m_szTriggerName = nullptr;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};

class CATLAudioFileEntry final
{
public:

	CATLAudioFileEntry() = delete;
	CATLAudioFileEntry(CATLAudioFileEntry const&) = delete;
	CATLAudioFileEntry(CATLAudioFileEntry&&) = delete;
	CATLAudioFileEntry& operator=(CATLAudioFileEntry const&) = delete;
	CATLAudioFileEntry& operator=(CATLAudioFileEntry&&) = delete;

	explicit CATLAudioFileEntry(char const* const szPath = nullptr, Impl::IFile* const pImplData = nullptr)
		: m_path(szPath)
		, m_size(0)
		, m_useCount(0)
		, m_memoryBlockAlignment(MEMORY_ALLOCATION_ALIGNMENT)
		, m_flags(EFileFlags::NotFound)
		, m_dataScope(EDataScope::All)
		, m_streamTaskType(eStreamTaskTypeCount)
		, m_pMemoryBlock(nullptr)
		, m_pReadStream(nullptr)
		, m_pImplData(pImplData)
	{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		m_timeCached.SetValue(0);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}

	CryFixedStringT<MaxFilePathLength> m_path;
	size_t                             m_size;
	size_t                             m_useCount;
	size_t                             m_memoryBlockAlignment;
	EFileFlags                         m_flags;
	EDataScope                         m_dataScope;
	EStreamTaskType                    m_streamTaskType;
	_smart_ptr<ICustomMemoryBlock>     m_pMemoryBlock;
	IReadStreamPtr                     m_pReadStream;
	Impl::IFile*                       m_pImplData;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	CTimeValue m_timeCached;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};

class CATLPreloadRequest final : public CATLEntity<PreloadRequestId>
{
public:

	using FileEntryIds = std::vector<FileEntryId>;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	explicit CATLPreloadRequest(
		PreloadRequestId const audioPreloadRequestId,
		EDataScope const dataScope,
		bool const bAutoLoad,
		FileEntryIds const& fileEntryIds,
		char const* const szName)
		: CATLEntity<PreloadRequestId>(audioPreloadRequestId, dataScope, szName)
		, m_bAutoLoad(bAutoLoad)
		, m_fileEntryIds(fileEntryIds)
	{}
#else
	explicit CATLPreloadRequest(
		PreloadRequestId const audioPreloadRequestId,
		EDataScope const dataScope,
		bool const bAutoLoad,
		FileEntryIds const& fileEntryIds)
		: CATLEntity<PreloadRequestId>(audioPreloadRequestId, dataScope)
		, m_bAutoLoad(bAutoLoad)
		, m_fileEntryIds(fileEntryIds)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	bool const   m_bAutoLoad;
	FileEntryIds m_fileEntryIds;
};
} // namespace CryAudio
