// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"

#include <ATLEntityData.h>
#include <cri_atom_ex.h>

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
class CEvent;
class CTrigger;
class CParameter;
class CSwitchState;

enum class EObjectFlags : EnumFlagsType
{
	None                    = 0,
	MovingOrDecaying        = BIT(0),
	TrackAbsoluteVelocity   = BIT(1),
	TrackVelocityForDoppler = BIT(2),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EObjectFlags);

class CBaseObject : public IObject
{
public:

	CBaseObject(CBaseObject const&) = delete;
	CBaseObject(CBaseObject&&) = delete;
	CBaseObject& operator=(CBaseObject const&) = delete;
	CBaseObject& operator=(CBaseObject&&) = delete;

	CBaseObject();
	virtual ~CBaseObject() override;

	// CryAudio::Impl::IObject
	virtual void                         Update(float const deltaTime) override                                  {}
	virtual void                         SetTransformation(CObjectTransformation const& transformation) override {}
	virtual CObjectTransformation const& GetTransformation() const override                                      { return CObjectTransformation::GetEmptyObject(); }
	virtual ERequestStatus               ExecuteTrigger(ITrigger const* const pITrigger, IEvent* const pIEvent) override;
	virtual void                         StopAllTriggers() override;
	virtual ERequestStatus               PlayFile(IStandaloneFile* const pIStandaloneFile) override;
	virtual ERequestStatus               StopFile(IStandaloneFile* const pIStandaloneFile) override;
	virtual ERequestStatus               SetName(char const* const szName) override;
	virtual void                         ToggleFunctionality(EObjectFunctionality const type, bool const enable) override {}

	// Below data is only used when INCLUDE_ADX2_IMPL_PRODUCTION_CODE is defined!
	virtual void DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY, char const* const szTextFilter) override {}
	// ~CryAudio::Impl::IObject

	void RemoveEvent(CEvent* const pEvent);
	void MutePlayer(CriBool const shouldPause);
	void PausePlayer(CriBool const shouldPause);

protected:

	bool StartEvent(CTrigger const* const pTrigger, CEvent* const pEvent);
	void StopEvent(uint32 const triggerId);
	void PauseEvent(uint32 const triggerId);
	void ResumeEvent(uint32 const triggerId);
	void UpdateVelocityTracking();

	using Events = std::vector<CEvent*>;
	Events              m_events;

	EObjectFlags        m_flags;
	S3DAttributes       m_3dAttributes;
	CriAtomEx3dSourceHn m_p3dSource;
	CriAtomExPlayerHn   m_pPlayer;

#if defined(INCLUDE_ADX2_IMPL_PRODUCTION_CODE)
	float m_absoluteVelocity;
	float m_absoluteVelocityNormalized;
#endif  // INCLUDE_ADX2_IMPL_PRODUCTION_CODE
};
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
