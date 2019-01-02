// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Environment.h"
#include "Common/IImpl.h"
#include "Common/IObject.h"
#include "Common/IEnvironmentConnection.h"

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	#include "Object.h"
	#include "Common/Logger.h"
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
CEnvironment::~CEnvironment()
{
	CRY_ASSERT_MESSAGE(g_pIImpl != nullptr, "g_pIImpl mustn't be nullptr during %s", __FUNCTION__);

	for (auto const pConnection : m_connections)
	{
		g_pIImpl->DestructEnvironmentConnection(pConnection);
	}
}

///////////////////////////////////////////////////////////////////////////
void CEnvironment::Set(CObject const& object, float const value) const
{
	for (auto const pConnection : m_connections)
	{
		pConnection->Set(object.GetImplDataPtr(), value);
	}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	// Log the "no-connections" case only on user generated controls.
	if (m_connections.empty())
	{
		Cry::Audio::Log(ELogType::Warning, R"(Environment "%s" set on object "%s" without connections)", GetName(), object.m_name.c_str());
	}

	const_cast<CObject&>(object).StoreEnvironmentValue(GetId(), value);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}
} // namespace CryAudio