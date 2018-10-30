// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseSwitchState.h"
#include <PoolObject.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CSwitchState final : public CBaseSwitchState, public CPoolObject<CSwitchState, stl::PSyncNone>
{
public:

	CSwitchState() = delete;
	CSwitchState(CSwitchState const&) = delete;
	CSwitchState(CSwitchState&&) = delete;
	CSwitchState& operator=(CSwitchState const&) = delete;
	CSwitchState& operator=(CSwitchState&&) = delete;

	explicit CSwitchState(uint32 const id, float const value);
	virtual ~CSwitchState() override = default;
};
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
