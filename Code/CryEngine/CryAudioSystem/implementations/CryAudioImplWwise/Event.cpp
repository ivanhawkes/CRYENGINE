// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Event.h"
#include "Common.h"
#include "EventInstance.h"
#include "Impl.h"
#include "BaseObject.h"

#include <AK/SoundEngine/Common/AkSoundEngine.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
//////////////////////////////////////////////////////////////////////////
void EndEventCallback(AkCallbackType callbackType, AkCallbackInfo* pCallbackInfo)
{
	if ((callbackType == AK_EndOfEvent) && !g_pImpl->IsToBeReleased() && (pCallbackInfo->pCookie != nullptr))
	{
		auto const pEventInstance = static_cast<CEventInstance*>(pCallbackInfo->pCookie);
		pEventInstance->SetToBeRemoved();
	}
}

//////////////////////////////////////////////////////////////////////////
void PrepareEventCallback(
	AkUniqueID eventId,
	void const* pBankPtr,
	AKRESULT wwiseResult,
	AkMemPoolId memPoolId,
	void* pCookie)
{
	auto const pEventInstance = static_cast<CEventInstance*>(pCookie);

	if (pEventInstance != nullptr)
	{
		pEventInstance->SetPlayingId(eventId);
	}
}

//////////////////////////////////////////////////////////////////////////
ETriggerResult CEvent::Execute(IObject* const pIObject, TriggerInstanceId const triggerInstanceId)
{
	ETriggerResult result = ETriggerResult::Failure;

	auto const pBaseObject = static_cast<CBaseObject*>(pIObject);

#if defined(CRY_AUDIO_IMPL_WWISE_USE_PRODUCTION_CODE)
	CEventInstance* const pEventInstance = g_pImpl->ConstructEventInstance(triggerInstanceId, m_id, m_maxAttenuation, pBaseObject, this);
#else
	CEventInstance* const pEventInstance = g_pImpl->ConstructEventInstance(triggerInstanceId, m_id, m_maxAttenuation);
#endif      // CRY_AUDIO_IMPL_WWISE_USE_PRODUCTION_CODE

	pBaseObject->SetAuxSendValues();

	AkPlayingID const playingId = AK::SoundEngine::PostEvent(m_id, pBaseObject->GetId(), AK_EndOfEvent, &EndEventCallback, pEventInstance);

	if (playingId != AK_INVALID_PLAYING_ID)
	{
#if defined(CRY_AUDIO_IMPL_WWISE_USE_PRODUCTION_CODE)
		{
			CryAutoLock<CryCriticalSection> const lock(CryAudio::Impl::Wwise::g_cs);
			g_playingIds[playingId] = pEventInstance;
		}
#endif      // CRY_AUDIO_IMPL_WWISE_USE_PRODUCTION_CODE

		pEventInstance->SetPlayingId(playingId);
		pBaseObject->AddEventInstance(pEventInstance);

		result = (pEventInstance->GetState() == EEventInstanceState::Virtual) ? ETriggerResult::Virtual : ETriggerResult::Playing;
	}
	else
	{
		g_pImpl->DestructEventInstance(pEventInstance);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CEvent::Stop(IObject* const pIObject)
{
	auto const pBaseObject = static_cast<CBaseObject*>(pIObject);
	pBaseObject->StopEvent(m_id);
}
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
