// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PreviewTrigger.h"
#include "Common.h"
#include "Managers.h"
#include "EventManager.h"
#include "Object.h"
#include "Event.h"
#include "Common/IEvent.h"
#include "Common/IImpl.h"
#include "Common/IObject.h"
#include "Common/ITriggerConnection.h"
#include "Common/ITriggerInfo.h"
#include "Common/Logger.h"

namespace CryAudio
{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
CPreviewTrigger::CPreviewTrigger()
	: Control(PreviewTriggerId, EDataScope::Global, s_szPreviewTriggerName)
	, m_pConnection(nullptr)
{
}

//////////////////////////////////////////////////////////////////////////
CPreviewTrigger::~CPreviewTrigger()
{
	CRY_ASSERT_MESSAGE(m_pConnection == nullptr, "There is still a connection during %s", __FUNCTION__);
}

//////////////////////////////////////////////////////////////////////////
void CPreviewTrigger::Execute(Impl::ITriggerInfo const& triggerInfo)
{
	g_pIImpl->DestructTriggerConnection(m_pConnection);
	m_pConnection = nullptr;
	m_pConnection = g_pIImpl->ConstructTriggerConnection(&triggerInfo);

	if (m_pConnection != nullptr)
	{
		STriggerInstanceState triggerInstanceState;
		triggerInstanceState.triggerId = GetId();

		CEvent* const pEvent = g_eventManager.ConstructEvent();
		ERequestStatus const activateResult = g_previewObject.GetImplDataPtr()->ExecuteTrigger(m_pConnection, pEvent->m_pImplData);

		if (activateResult == ERequestStatus::Success || activateResult == ERequestStatus::Pending)
		{
			pEvent->SetTriggerName(GetName());
			pEvent->m_pObject = &g_previewObject;
			pEvent->SetTriggerId(GetId());
			pEvent->m_triggerInstanceId = g_triggerInstanceIdCounter;

			if (activateResult == ERequestStatus::Success)
			{
				pEvent->m_state = EEventState::Playing;
				++(triggerInstanceState.numPlayingEvents);
			}
			else if (activateResult == ERequestStatus::Pending)
			{
				pEvent->m_state = EEventState::Loading;
				++(triggerInstanceState.numLoadingEvents);
			}

			g_previewObject.AddEvent(pEvent);
		}
		else
		{
			g_eventManager.DestructEvent(pEvent);

			if (activateResult != ERequestStatus::SuccessDoNotTrack)
			{
				// No TriggerImpl generated an active event.
				Cry::Audio::Log(ELogType::Warning, R"(Trigger "%s" failed on object "%s")", GetName(), g_previewObject.m_name.c_str());
			}
		}

		if (triggerInstanceState.numPlayingEvents > 0 || triggerInstanceState.numLoadingEvents > 0)
		{
			triggerInstanceState.flags |= ETriggerStatus::Playing;
			g_previewObject.AddTriggerState(g_triggerInstanceIdCounter++, triggerInstanceState);
		}
		else
		{
			// All of the events have either finished before we got here or never started, immediately inform the user that the trigger has finished.
			g_previewObject.SendFinishedTriggerInstanceRequest(triggerInstanceState);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreviewTrigger::Stop()
{
	for (auto const pEvent : g_previewObject.GetActiveEvents())
	{
		CRY_ASSERT_MESSAGE((pEvent != nullptr) && pEvent->IsPlaying(), "Invalid event during %s", __FUNCTION__);
		pEvent->Stop();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreviewTrigger::Clear()
{
	g_pIImpl->DestructTriggerConnection(m_pConnection);
	m_pConnection = nullptr;
}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}      // namespace CryAudio
