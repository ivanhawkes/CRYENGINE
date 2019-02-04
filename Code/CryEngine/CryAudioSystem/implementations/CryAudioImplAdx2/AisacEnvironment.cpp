// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AisacEnvironment.h"
#include "BaseObject.h"

#include <cri_atom_ex.h>

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
//////////////////////////////////////////////////////////////////////////
void CAisacEnvironment::Set(IObject* const pIObject, float const amount)
{
	auto const pBaseObject = static_cast<CBaseObject const*>(pIObject);

	CriAtomExPlayerHn const pPlayer = pBaseObject->GetPlayer();
	criAtomExPlayer_SetAisacControlByName(pPlayer, static_cast<CriChar8 const*>(m_name.c_str()), static_cast<CriFloat32>(m_multiplier * amount + m_shift));
	criAtomExPlayer_UpdateAll(pPlayer);
}
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
