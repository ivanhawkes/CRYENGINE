// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Common.h"

#include "AudioImpl.h"

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
CImpl* g_pImpl = nullptr;

AkGameObjectID g_listenerId = AK_INVALID_GAME_OBJECT; // To be removed once multi-listener support is implemented.
AkGameObjectID g_globalObjectId = AK_INVALID_GAME_OBJECT;
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
