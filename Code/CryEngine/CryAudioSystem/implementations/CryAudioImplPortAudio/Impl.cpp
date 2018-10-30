// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Impl.h"
#include "Common.h"
#include "Trigger.h"
#include "Event.h"
#include "Object.h"
#include "CVars.h"
#include "Environment.h"
#include "File.h"
#include "Listener.h"
#include "Parameter.h"
#include "Setting.h"
#include "StandaloneFile.h"
#include "SwitchState.h"
#include "GlobalData.h"
#include <Logger.h>
#include <sndfile.hh>
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/IProjectManager.h>
#include <CryAudio/IAudioSystem.h>

#if defined(INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE)
	#include "Debug.h"
	#include <CryRenderer/IRenderAuxGeom.h>
#endif  // INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
uint32 g_triggerPoolSize = 0;
uint32 g_triggerPoolSizeLevelSpecific = 0;

#if defined(INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE)
uint32 g_debugTriggerPoolSize = 0;
#endif  // INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE

//////////////////////////////////////////////////////////////////////////
void CountPoolSizes(XmlNodeRef const pNode, uint32& poolSizes)
{
	uint32 numTriggers = 0;
	pNode->getAttr(s_szTriggersAttribute, numTriggers);
	poolSizes += numTriggers;
}

//////////////////////////////////////////////////////////////////////////
void AllocateMemoryPools(uint32 const objectPoolSize, uint32 const eventPoolSize)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_AudioImpl, 0, "Port Audio Object Pool");
	CObject::CreateAllocator(objectPoolSize);

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_AudioImpl, 0, "Port Audio Event Pool");
	CEvent::CreateAllocator(eventPoolSize);

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_AudioImpl, 0, "Port Audio Trigger Pool");
	CTrigger::CreateAllocator(g_triggerPoolSize);
}

//////////////////////////////////////////////////////////////////////////
void FreeMemoryPools()
{
	CObject::FreeMemoryPool();
	CEvent::FreeMemoryPool();
	CTrigger::FreeMemoryPool();
}

//////////////////////////////////////////////////////////////////////////
bool GetSoundInfo(char const* const szPath, SF_INFO& sfInfo, PaStreamParameters& streamParameters)
{
	bool success = false;

	ZeroStruct(sfInfo);
	SNDFILE* const pSndFile = sf_open(szPath, SFM_READ, &sfInfo);

	if (pSndFile != nullptr)
	{
		streamParameters.device = Pa_GetDefaultOutputDevice();
		streamParameters.channelCount = sfInfo.channels;
		streamParameters.suggestedLatency = Pa_GetDeviceInfo(streamParameters.device)->defaultLowOutputLatency;
		streamParameters.hostApiSpecificStreamInfo = nullptr;
		int const subFormat = sfInfo.format & SF_FORMAT_SUBMASK;

		switch (subFormat)
		{
		case SF_FORMAT_PCM_16:
		case SF_FORMAT_PCM_24:
			streamParameters.sampleFormat = paInt16;
			break;
		case SF_FORMAT_PCM_32:
			streamParameters.sampleFormat = paInt32;
			break;
		case SF_FORMAT_FLOAT:
		case SF_FORMAT_VORBIS:
			streamParameters.sampleFormat = paFloat32;
			break;
		}

		if (sf_close(pSndFile) != 0)
		{
			Cry::Audio::Log(ELogType::Error, "Failed to close sound file %s", szPath);
		}

		success = true;
	}
	else
	{
		Cry::Audio::Log(ELogType::Error, "Failed to open sound file %s", szPath);
	}

	return success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::Init(uint32 const objectPoolSize, uint32 const eventPoolSize)
{
	AllocateMemoryPools(objectPoolSize, eventPoolSize);

	char const* szAssetDirectory = gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryRelative();

#if defined(INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE)
	if (strlen(szAssetDirectory) == 0)
	{
		Cry::Audio::Log(ELogType::Error, "<Audio - PortAudio>: No asset folder set!");
		szAssetDirectory = "no-asset-folder-set";
	}

	m_name = Pa_GetVersionText();
#endif  // INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE

	m_regularSoundBankFolder = szAssetDirectory;
	m_regularSoundBankFolder += "/";
	m_regularSoundBankFolder += AUDIO_SYSTEM_DATA_ROOT;
	m_regularSoundBankFolder += "/";
	m_regularSoundBankFolder += s_szImplFolderName;
	m_regularSoundBankFolder += "/";
	m_regularSoundBankFolder += s_szAssetsFolderName;
	m_localizedSoundBankFolder = m_regularSoundBankFolder;

	if (ICVar* const pCVar = gEnv->pConsole->GetCVar("g_languageAudio"))
	{
		SetLanguage(pCVar->GetString());
	}

	PaError const err = Pa_Initialize();

	if (err != paNoError)
	{
		Cry::Audio::Log(ELogType::Error, "Failed to initialize PortAudio: %s", Pa_GetErrorText(err));
	}

	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
void CImpl::ShutDown()
{
	PaError const err = Pa_Terminate();

	if (err != paNoError)
	{
		Cry::Audio::Log(ELogType::Error, "Failed to shut down PortAudio: %s", Pa_GetErrorText(err));
	}

}

///////////////////////////////////////////////////////////////////////////
void CImpl::Release()
{
	delete this;
	g_cvars.UnregisterVariables();

	FreeMemoryPools();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetLibraryData(XmlNodeRef const pNode, bool const isLevelSpecific)
{
	if (isLevelSpecific)
	{
		uint32 triggerLevelPoolSize;
		CountPoolSizes(pNode, triggerLevelPoolSize);

		g_triggerPoolSizeLevelSpecific = std::max(g_triggerPoolSizeLevelSpecific, triggerLevelPoolSize);
	}
	else
	{
		CountPoolSizes(pNode, g_triggerPoolSize);
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnBeforeLibraryDataChanged()
{
	g_triggerPoolSize = 0;
	g_triggerPoolSizeLevelSpecific = 0;

#if defined(INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE)
	g_debugTriggerPoolSize = 0;
#endif  // INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnAfterLibraryDataChanged()
{
	g_triggerPoolSize += g_triggerPoolSizeLevelSpecific;

#if defined(INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE)
	// Used to hide pools without allocations in debug draw.
	g_debugTriggerPoolSize = g_triggerPoolSize;
#endif  // INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE

	g_triggerPoolSize = std::max<uint32>(1, g_triggerPoolSize);
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::OnLoseFocus()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::OnGetFocus()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::MuteAll()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::UnmuteAll()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::StopAllSounds()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::PauseAll()
{
	return ERequestStatus::Success;
}

///////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::ResumeAll()
{
	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::RegisterInMemoryFile(SFileInfo* const pFileInfo)
{
	ERequestStatus requestResult = ERequestStatus::Failure;

	if (pFileInfo != nullptr)
	{
		CFile* const pFileData = static_cast<CFile*>(pFileInfo->pImplData);

		if (pFileData != nullptr)
		{
			requestResult = ERequestStatus::Success;
		}
		else
		{
			Cry::Audio::Log(ELogType::Error, "Invalid AudioFileEntryData passed to the PortAudio implementation of RegisterInMemoryFile");
		}
	}

	return requestResult;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::UnregisterInMemoryFile(SFileInfo* const pFileInfo)
{
	ERequestStatus requestResult = ERequestStatus::Failure;

	if (pFileInfo != nullptr)
	{
		CFile* const pFileData = static_cast<CFile*>(pFileInfo->pImplData);

		if (pFileData != nullptr)
		{
			requestResult = ERequestStatus::Success;
		}
		else
		{
			Cry::Audio::Log(ELogType::Error, "Invalid AudioFileEntryData passed to the PortAudio implementation of UnregisterInMemoryFile");
		}
	}

	return requestResult;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CImpl::ConstructFile(XmlNodeRef const pRootNode, SFileInfo* const pFileInfo)
{
	return ERequestStatus::Failure;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructFile(IFile* const pIFile)
{
	delete pIFile;
}

//////////////////////////////////////////////////////////////////////////
char const* const CImpl::GetFileLocation(SFileInfo* const pFileInfo)
{
	char const* szResult = nullptr;

	if (pFileInfo != nullptr)
	{
		szResult = pFileInfo->bLocalized ? m_localizedSoundBankFolder.c_str() : m_regularSoundBankFolder.c_str();
	}

	return szResult;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GetInfo(SImplInfo& implInfo) const
{
#if defined(INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE)
	implInfo.name = m_name.c_str();
#else
	implInfo.name = "name-not-present-in-release-mode";
#endif  // INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE
	implInfo.folderName = s_szImplFolderName;
}

///////////////////////////////////////////////////////////////////////////
IObject* CImpl::ConstructGlobalObject()
{
	return new CObject();
}

///////////////////////////////////////////////////////////////////////////
IObject* CImpl::ConstructObject(CObjectTransformation const& transformation, char const* const szName /*= nullptr*/)
{
	auto pObject = new CObject();
	stl::push_back_unique(m_constructedObjects, pObject);

	return static_cast<IObject*>(pObject);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructObject(IObject const* const pIObject)
{
	auto const pObject = static_cast<CObject const*>(pIObject);
	stl::find_and_erase(m_constructedObjects, pObject);
	delete pObject;
}

///////////////////////////////////////////////////////////////////////////
IListener* CImpl::ConstructListener(CObjectTransformation const& transformation, char const* const szName /*= nullptr*/)
{
	g_pListener = new CListener(transformation);

#if defined(INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE)
	if (szName != nullptr)
	{
		g_pListener->SetName(szName);
	}
#endif  // INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE

	return static_cast<IListener*>(g_pListener);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructListener(IListener* const pIListener)
{
	CRY_ASSERT_MESSAGE(pIListener == g_pListener, "pIListener is not g_pListener");
	delete g_pListener;
	g_pListener = nullptr;
}

//////////////////////////////////////////////////////////////////////////
IEvent* CImpl::ConstructEvent(CATLEvent& event)
{
	return static_cast<IEvent*>(new CEvent(event));
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructEvent(IEvent const* const pIEvent)
{
	delete pIEvent;
}

//////////////////////////////////////////////////////////////////////////
IStandaloneFile* CImpl::ConstructStandaloneFile(CATLStandaloneFile& standaloneFile, char const* const szFile, bool const bLocalized, ITrigger const* pITrigger /*= nullptr*/)
{
	return static_cast<IStandaloneFile*>(new CStandaloneFile);
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructStandaloneFile(IStandaloneFile const* const pIStandaloneFile)
{
	delete pIStandaloneFile;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GamepadConnected(DeviceId const deviceUniqueID)
{
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GamepadDisconnected(DeviceId const deviceUniqueID)
{
}

///////////////////////////////////////////////////////////////////////////
ITrigger const* CImpl::ConstructTrigger(XmlNodeRef const pRootNode, float& radius)
{
	CTrigger* pTrigger = nullptr;
	char const* const szTag = pRootNode->getTag();

	if (_stricmp(szTag, s_szEventTag) == 0)
	{
		char const* const szLocalized = pRootNode->getAttr(s_szLocalizedAttribute);
		bool const isLocalized = (szLocalized != nullptr) && (_stricmp(szLocalized, s_szTrueValue) == 0);

		stack_string path = (isLocalized ? m_localizedSoundBankFolder.c_str() : m_regularSoundBankFolder.c_str());
		path += "/";
		stack_string const folderName = pRootNode->getAttr(s_szPathAttribute);

		if (!folderName.empty())
		{
			path += folderName.c_str();
			path += "/";
		}

		stack_string const name = pRootNode->getAttr(s_szNameAttribute);
		path += name.c_str();

		SF_INFO sfInfo;
		PaStreamParameters streamParameters;

		if (GetSoundInfo(path.c_str(), sfInfo, streamParameters))
		{
			CryFixedStringT<16> const eventTypeString(pRootNode->getAttr(s_szTypeAttribute));
			EEventType const eventType = eventTypeString.compareNoCase(s_szStartValue) == 0 ? EEventType::Start : EEventType::Stop;

			int numLoops = 0;
			pRootNode->getAttr(s_szLoopCountAttribute, numLoops);
			// --numLoops because -1: play infinite, 0: play once, 1: play twice, etc...
			--numLoops;
			// Max to -1 to stay backwards compatible.
			numLoops = std::max(-1, numLoops);

			pTrigger = new CTrigger(
				StringToId(path.c_str()),
				numLoops,
				static_cast<double>(sfInfo.samplerate),
				eventType,
				path.c_str(),
				streamParameters,
				folderName.c_str(),
				name.c_str(),
				isLocalized);

			m_triggers.push_back(pTrigger);
		}
	}
	else
	{
		Cry::Audio::Log(ELogType::Warning, "Unknown PortAudio tag: %s", szTag);
	}

	return static_cast<ITrigger*>(pTrigger);
}

//////////////////////////////////////////////////////////////////////////
ITrigger const* CImpl::ConstructTrigger(ITriggerInfo const* const pITriggerInfo)
{
#if defined(INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE)
	ITrigger const* pITrigger = nullptr;
	auto const pTriggerInfo = static_cast<STriggerInfo const*>(pITriggerInfo);

	if (pTriggerInfo != nullptr)
	{
		stack_string path = (pTriggerInfo->isLocalized ? m_localizedSoundBankFolder.c_str() : m_regularSoundBankFolder.c_str());
		path += "/";
		stack_string const folderName = pTriggerInfo->path.c_str();

		if (!folderName.empty())
		{
			path += folderName.c_str();
			path += "/";
		}

		stack_string const name = pTriggerInfo->name.c_str();
		path += name.c_str();

		SF_INFO sfInfo;
		PaStreamParameters streamParameters;

		if (GetSoundInfo(path.c_str(), sfInfo, streamParameters))
		{
			pITrigger = static_cast<ITrigger const*>(new CTrigger(StringToId(path.c_str()), 0, static_cast<double>(sfInfo.samplerate), EEventType::Start, path.c_str(), streamParameters, folderName.c_str(), name.c_str(), pTriggerInfo->isLocalized));
		}
	}

	return pITrigger;
#else
	return nullptr;
#endif  // INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructTrigger(ITrigger const* const pITrigger)
{
	delete pITrigger;
}

///////////////////////////////////////////////////////////////////////////
IParameter const* CImpl::ConstructParameter(XmlNodeRef const pRootNode)
{
	return static_cast<IParameter*>(new CParameter);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructParameter(IParameter const* const pIParameter)
{
	delete pIParameter;
}

///////////////////////////////////////////////////////////////////////////
ISwitchState const* CImpl::ConstructSwitchState(XmlNodeRef const pRootNode)
{
	return static_cast<ISwitchState*>(new CSwitchState);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructSwitchState(ISwitchState const* const pISwitchState)
{
	delete pISwitchState;
}

///////////////////////////////////////////////////////////////////////////
IEnvironment const* CImpl::ConstructEnvironment(XmlNodeRef const pRootNode)
{
	return static_cast<IEnvironment*>(new CEnvironment);
}

///////////////////////////////////////////////////////////////////////////
void CImpl::DestructEnvironment(IEnvironment const* const pIEnvironment)
{
	delete pIEnvironment;
}

//////////////////////////////////////////////////////////////////////////
ISetting const* CImpl::ConstructSetting(XmlNodeRef const pRootNode)
{
	return static_cast<ISetting*>(new CSetting);
}

//////////////////////////////////////////////////////////////////////////
void CImpl::DestructSetting(ISetting const* const pISetting)
{
	delete pISetting;
}

//////////////////////////////////////////////////////////////////////////
void CImpl::OnRefresh()
{
	m_triggers.clear();
}

//////////////////////////////////////////////////////////////////////////
void CImpl::SetLanguage(char const* const szLanguage)
{
	if (szLanguage != nullptr)
	{
		bool const shouldReload = !m_language.IsEmpty() && (m_language.compareNoCase(szLanguage) != 0);

		m_language = szLanguage;
		char const* szAssetDirectory = gEnv->pSystem->GetIProjectManager()->GetCurrentAssetDirectoryRelative();
		m_localizedSoundBankFolder = szAssetDirectory;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += PathUtil::GetLocalizationFolder().c_str();
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += szLanguage;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += AUDIO_SYSTEM_DATA_ROOT;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += s_szImplFolderName;
		m_localizedSoundBankFolder += "/";
		m_localizedSoundBankFolder += s_szAssetsFolderName;

		if (shouldReload)
		{
			UpdateLocalizedTriggers();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::UpdateLocalizedTriggers()
{
	for (auto const pTrigger : m_triggers)
	{
		if (pTrigger->m_isLocalized)
		{
			stack_string path = m_localizedSoundBankFolder.c_str();
			path += "/";

			if (!pTrigger->m_folder.empty())
			{
				path += pTrigger->m_folder.c_str();
				path += "/";
			}

			path += pTrigger->m_name.c_str();

			SF_INFO sfInfo;
			PaStreamParameters streamParameters;

			GetSoundInfo(path.c_str(), sfInfo, streamParameters);
			pTrigger->sampleRate = static_cast<double>(sfInfo.samplerate);
			pTrigger->streamParameters = streamParameters;
			pTrigger->filePath = path.c_str();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CImpl::GetFileData(char const* const szName, SFileData& fileData) const
{
}

#if defined(INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
void DrawMemoryPoolInfo(
	IRenderAuxGeom& auxGeom,
	float const posX,
	float& posY,
	stl::SPoolMemoryUsage const& mem,
	stl::SMemoryUsage const& pool,
	char const* const szType)
{
	CryFixedStringT<MaxMiscStringLength> memUsedString;

	if (mem.nUsed < 1024)
	{
		memUsedString.Format("%" PRISIZE_T " Byte", mem.nUsed);
	}
	else
	{
		memUsedString.Format("%" PRISIZE_T " KiB", mem.nUsed >> 10);
	}

	CryFixedStringT<MaxMiscStringLength> memAllocString;

	if (mem.nAlloc < 1024)
	{
		memAllocString.Format("%" PRISIZE_T " Byte", mem.nAlloc);
	}
	else
	{
		memAllocString.Format("%" PRISIZE_T " KiB", mem.nAlloc >> 10);
	}

	posY += g_debugSystemLineHeight;
	auxGeom.Draw2dLabel(posX, posY, g_debugSystemFontSize, g_debugSystemColorTextPrimary.data(), false,
	                    "[%s] In Use: %" PRISIZE_T " | Constructed: %" PRISIZE_T " (%s) | Memory Pool: %s",
	                    szType, pool.nUsed, pool.nAlloc, memUsedString.c_str(), memAllocString.c_str());
}
#endif  // INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE

//////////////////////////////////////////////////////////////////////////
void CImpl::DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float& posY, bool const showDetailedInfo)
{
#if defined(INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE)
	CryModuleMemoryInfo memInfo;
	ZeroStruct(memInfo);
	CryGetMemoryInfoForModule(&memInfo);

	CryFixedStringT<MaxMiscStringLength> memInfoString;
	auto const memAlloc = static_cast<uint32>(memInfo.allocated - memInfo.freed);

	if (memAlloc < 1024)
	{
		memInfoString.Format("%s (Total Memory: %u Byte)", m_name.c_str(), memAlloc);
	}
	else
	{
		memInfoString.Format("%s (Total Memory: %u KiB)", m_name.c_str(), memAlloc >> 10);
	}

	auxGeom.Draw2dLabel(posX, posY, g_debugSystemHeaderFontSize, g_debugSystemColorHeader.data(), false, memInfoString.c_str());

	if (showDetailedInfo)
	{
		{
			auto& allocator = CObject::GetAllocator();
			DrawMemoryPoolInfo(auxGeom, posX, posY, allocator.GetTotalMemory(), allocator.GetCounts(), "Objects");
		}

		{
			auto& allocator = CEvent::GetAllocator();
			DrawMemoryPoolInfo(auxGeom, posX, posY, allocator.GetTotalMemory(), allocator.GetCounts(), "Events");
		}

		if (g_debugTriggerPoolSize > 0)
		{
			auto& allocator = CTrigger::GetAllocator();
			DrawMemoryPoolInfo(auxGeom, posX, posY, allocator.GetTotalMemory(), allocator.GetCounts(), "Triggers");
		}
	}

	Vec3 const& listenerPosition = g_pListener->GetTransformation().GetPosition();
	Vec3 const& listenerDirection = g_pListener->GetTransformation().GetForward();
	char const* const szName = g_pListener->GetName();

	posY += g_debugSystemLineHeight;
	auxGeom.Draw2dLabel(posX, posY, g_debugSystemFontSize, g_debugSystemColorListenerActive.data(), false, "Listener: %s | PosXYZ: %.2f %.2f %.2f | FwdXYZ: %.2f %.2f %.2f", szName, listenerPosition.x, listenerPosition.y, listenerPosition.z, listenerDirection.x, listenerDirection.y, listenerDirection.z);
#endif  // INCLUDE_PORTAUDIO_IMPL_PRODUCTION_CODE
}
} // namespace PortAudio
} // namespace Impl
} // namespace CryAudio
