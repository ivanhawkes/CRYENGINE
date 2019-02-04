// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Entity.h"
#include "Common.h"
#include <CryAudio/IAudioSystem.h>

namespace CryAudio
{
class CLoseFocusTrigger final : public Control
{
public:

	CLoseFocusTrigger(CLoseFocusTrigger const&) = delete;
	CLoseFocusTrigger(CLoseFocusTrigger&&) = delete;
	CLoseFocusTrigger& operator=(CLoseFocusTrigger const&) = delete;
	CLoseFocusTrigger& operator=(CLoseFocusTrigger&&) = delete;

#if defined(CRY_AUDIO_USE_PRODUCTION_CODE)
	CLoseFocusTrigger()
		: Control(g_loseFocusTriggerId, EDataScope::Global, g_szLoseFocusTriggerName)
	{}
#else
	CLoseFocusTrigger()
		: Control(g_loseFocusTriggerId, EDataScope::Global)
	{}
#endif // CRY_AUDIO_USE_PRODUCTION_CODE

	~CLoseFocusTrigger();

	void Execute() const;
	void AddConnections(TriggerConnections const& connections);
	void Clear();

private:

	TriggerConnections m_connections;
};
} // namespace CryAudio
