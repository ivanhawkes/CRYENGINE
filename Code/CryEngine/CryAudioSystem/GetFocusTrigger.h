// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Entity.h"
#include "Common.h"
#include <CryAudio/IAudioSystem.h>

namespace CryAudio
{
class CGetFocusTrigger final : public Control
{
public:

	CGetFocusTrigger(CGetFocusTrigger const&) = delete;
	CGetFocusTrigger(CGetFocusTrigger&&) = delete;
	CGetFocusTrigger& operator=(CGetFocusTrigger const&) = delete;
	CGetFocusTrigger& operator=(CGetFocusTrigger&&) = delete;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	CGetFocusTrigger()
		: Control(GetFocusTriggerId, EDataScope::Global, s_szGetFocusTriggerName)
	{}
#else
	CGetFocusTrigger()
		: Control(GetFocusTriggerId, EDataScope::Global)
	{}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	~CGetFocusTrigger();

	void Execute() const;
	void AddConnections(TriggerConnections const& connections);
	void Clear();

private:

	TriggerConnections m_connections;
};
} // namespace CryAudio
