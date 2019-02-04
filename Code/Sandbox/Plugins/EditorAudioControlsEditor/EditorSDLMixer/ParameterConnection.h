// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseConnection.h"

#include <PoolObject.h>
#include <CryAudioImplSDLMixer/GlobalData.h>

namespace ACE
{
namespace Impl
{
namespace SDLMixer
{
class CParameterConnection final : public CBaseConnection, public CryAudio::CPoolObject<CParameterConnection, stl::PSyncNone>
{
public:

	CParameterConnection() = delete;
	CParameterConnection(CParameterConnection const&) = delete;
	CParameterConnection(CParameterConnection&&) = delete;
	CParameterConnection& operator=(CParameterConnection const&) = delete;
	CParameterConnection& operator=(CParameterConnection&&) = delete;

	explicit CParameterConnection(
		ControlId const id,
		float const mult = CryAudio::Impl::SDL_mixer::g_defaultParamMultiplier,
		float const shift = CryAudio::Impl::SDL_mixer::g_defaultParamShift)
		: CBaseConnection(id)
		, m_mult(mult)
		, m_shift(shift)
	{}

	virtual ~CParameterConnection() override = default;

	// CBaseConnection
	virtual void Serialize(Serialization::IArchive& ar) override;
	// ~CBaseConnection

	float GetMultiplier() const { return m_mult; }
	float GetShift() const      { return m_shift; }

private:

	float m_mult;
	float m_shift;
};
} // namespace SDLMixer
} // namespace Impl
} // namespace ACE
