// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Entity.h"
#include "Common.h"
#include "Common/PoolObject.h"

namespace CryAudio
{
class CObject;
struct STriggerInstanceState;

namespace Impl
{
struct IObject;
} // namespace Impl

class CTrigger final : public Control, public CPoolObject<CTrigger, stl::PSyncNone>
{
public:

	CTrigger() = delete;
	CTrigger(CTrigger const&) = delete;
	CTrigger(CTrigger&&) = delete;
	CTrigger& operator=(CTrigger const&) = delete;
	CTrigger& operator=(CTrigger&&) = delete;

#if defined(CRY_AUDIO_USE_PRODUCTION_CODE)
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
		TriggerConnections const& connections)
		: Control(id, dataScope)
		, m_connections(connections)
	{}
#endif // CRY_AUDIO_USE_PRODUCTION_CODE

	~CTrigger();

	void Execute(
		CObject& object,
		void* const pOwner = nullptr,
		void* const pUserData = nullptr,
		void* const pUserDataOwner = nullptr,
		ERequestFlags const flags = ERequestFlags::None) const;
	void Stop(Impl::IObject* const pIObject) const;
	void PlayFile(
		CObject& object,
		char const* const szName,
		bool const isLocalized,
		void* const pOwner = nullptr,
		void* const pUserData = nullptr,
		void* const pUserDataOwner = nullptr) const;

#if defined(CRY_AUDIO_USE_PRODUCTION_CODE)
	void Execute(
		CObject& object,
		TriggerInstanceId const triggerInstanceId,
		STriggerInstanceState& triggerInstanceState,
		uint16 const triggerCounter) const;
	float GetRadius() const { return m_radius; }
	void  PlayFile(CObject& object, CStandaloneFile* const pFile) const;
#endif // CRY_AUDIO_USE_PRODUCTION_CODE

private:

	TriggerConnections const m_connections;

#if defined(CRY_AUDIO_USE_PRODUCTION_CODE)
	float const m_radius;
#endif // CRY_AUDIO_USE_PRODUCTION_CODE
};
} // namespace CryAudio
