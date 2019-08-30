// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SoundEngine.h"
#include "Common.h"
#include "Event.h"
#include "EventInstance.h"
#include "Impl.h"
#include "Listener.h"
#include "Object.h"
#include "GlobalData.h"
#include <CryAudio/IAudioSystem.h>
#include <CrySystem/File/CryFile.h>
#include <CryString/CryPath.h>

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	#include <Logger.h>
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

#include <SDL.h>
#include <SDL_mixer.h>
#include <queue>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
static string const s_regularAssetsPath = string(CRY_AUDIO_DATA_ROOT) + "/" + g_szImplFolderName + "/" + g_szAssetsFolderName + "/";
constexpr int g_supportedFormats = MIX_INIT_OGG | MIX_INIT_MP3;
constexpr int g_numMixChannels = 512;
constexpr SampleId g_invalidSampleId = 0;
constexpr int g_sampleRate = 48000;
constexpr int g_bufferSize = 4096;

// Samples
using SampleDataMap = std::unordered_map<SampleId, Mix_Chunk*>;
SampleDataMap g_sampleData;

using SampleNameMap = std::unordered_map<SampleId, string>;
SampleNameMap g_samplePaths;

// Channels
struct SChannelData
{
	CObject* pObject;
};

SChannelData g_channels[g_numMixChannels];

using TChannelQueue = std::queue<int>;
TChannelQueue g_freeChannels;

enum class EChannelFinishedRequestQueueId : EnumFlagsType
{
	One,
	Two,
	Count
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EChannelFinishedRequestQueueId);

using ChannelFinishedRequests = std::deque<int>;
ChannelFinishedRequests g_channelFinishedRequests[IntegralValue(EChannelFinishedRequestQueueId::Count)];
CryCriticalSection g_channelFinishedCriticalSection;

//////////////////////////////////////////////////////////////////////////
void SoundEngine::UnloadSample(SampleId const nID)
{
	Mix_Chunk* pSample = stl::find_in_map(g_sampleData, nID, nullptr);
	if (pSample != nullptr)
	{
		Mix_FreeChunk(pSample);
		stl::member_find_and_erase(g_sampleData, nID);
	}
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Error, "Could not find sample with id %d", nID);
	}
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void ProcessChannelFinishedRequests(ChannelFinishedRequests& queue)
{
	if (!queue.empty())
	{
		for (int const finishedChannelId : queue)
		{
			CObject const* const pObject = g_channels[finishedChannelId].pObject;

			if (pObject != nullptr)
			{
				for (auto const pEventInstance : pObject->m_eventInstances)
				{
					if (pEventInstance != nullptr)
					{
						ChannelList::iterator const channelsEnd = pEventInstance->m_channels.end();

						for (ChannelList::iterator channelIt = pEventInstance->m_channels.begin(); channelIt != channelsEnd; ++channelIt)
						{
							if (*channelIt == finishedChannelId)
							{
								pEventInstance->m_channels.erase(channelIt);

								if (pEventInstance->m_channels.empty())
								{
									pEventInstance->SetToBeRemoved();
								}

								break;
							}
						}
					}
				}

				g_channels[finishedChannelId].pObject = nullptr;
			}

			g_freeChannels.push(finishedChannelId);
		}

		queue.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void ChannelFinishedPlaying(int nChannel)
{
	if (nChannel >= 0 && nChannel < g_numMixChannels)
	{
		CryAutoLock<CryCriticalSection> autoLock(g_channelFinishedCriticalSection);
		g_channelFinishedRequests[IntegralValue(EChannelFinishedRequestQueueId::One)].push_back(nChannel);
	}
}

//////////////////////////////////////////////////////////////////////////
void LoadMetadata(string const& path, bool const isLocalized)
{
	string const rootDir = isLocalized ? s_localizedAssetsPath : s_regularAssetsPath;

	_finddata_t fd;
	ICryPak* pCryPak = gEnv->pCryPak;
	intptr_t handle = pCryPak->FindFirst(rootDir + path + "*.*", &fd);

	if (handle != -1)
	{
		do
		{
			string const name = fd.name;

			if (name != "." && name != ".." && !name.empty())
			{
				if (fd.attrib & _A_SUBDIR)
				{
					LoadMetadata(path + name + "/", isLocalized);
				}
				else
				{
					string::size_type const posExtension = name.rfind('.');

					if (posExtension != string::npos)
					{
						string const fileExtension = name.data() + posExtension;

						if ((_stricmp(fileExtension, ".mp3") == 0) ||
						    (_stricmp(fileExtension, ".ogg") == 0) ||
						    (_stricmp(fileExtension, ".wav") == 0))
						{
							if (path.empty())
							{
								g_samplePaths[StringToId(name.c_str())] = rootDir + name;
							}
							else
							{
								string const pathName = path + name;
								g_samplePaths[StringToId(pathName.c_str())] = rootDir + pathName;
							}
						}
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
bool SoundEngine::Init()
{
	if (SDL_Init(SDL_INIT_AUDIO) < 0)
	{
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
		Cry::Audio::Log(ELogType::Error, "SDL::SDL_Init() returned: %s", SDL_GetError());
#endif    // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

		return false;
	}

	int const loadedFormats = Mix_Init(g_supportedFormats);

	if ((loadedFormats & g_supportedFormats) != g_supportedFormats)
	{
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
		Cry::Audio::Log(ELogType::Error, R"(SDLMixer::Mix_Init() failed to init support for format flags %d with error "%s")", g_supportedFormats, Mix_GetError());
#endif    // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

		return false;
	}

	if (Mix_OpenAudio(g_sampleRate, MIX_DEFAULT_FORMAT, 2, g_bufferSize) < 0)
	{
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
		Cry::Audio::Log(ELogType::Error, R"(SDLMixer::Mix_OpenAudio() failed to init the SDL Mixer API with error "%s")", Mix_GetError());
#endif    // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

		return false;
	}

	Mix_AllocateChannels(g_numMixChannels);

	g_bMuted = false;
	Mix_Volume(-1, SDL_MIX_MAXVOLUME);

	for (int i = 0; i < g_numMixChannels; ++i)
	{
		g_freeChannels.push(i);
	}

	Mix_ChannelFinished(ChannelFinishedPlaying);

	LoadMetadata("", false);
	LoadMetadata("", true);

	return true;
}

//////////////////////////////////////////////////////////////////////////
void FreeAllSampleData()
{
	Mix_HaltChannel(-1);
	SampleDataMap::const_iterator it = g_sampleData.begin();
	SampleDataMap::const_iterator end = g_sampleData.end();

	for (; it != end; ++it)
	{
		Mix_FreeChunk(it->second);
	}

	g_sampleData.clear();
	g_samplePaths.clear();

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	g_loadedSampleSize = 0;
#endif    // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void SoundEngine::Release()
{
	FreeAllSampleData();
	g_objects.clear();
	g_objects.shrink_to_fit();
	Mix_Quit();
	Mix_CloseAudio();
	SDL_Quit();
}

//////////////////////////////////////////////////////////////////////////
void SoundEngine::Refresh()
{
	FreeAllSampleData();
	LoadMetadata("", false);
	LoadMetadata("", true);
}

//////////////////////////////////////////////////////////////////////////
const SampleId SoundEngine::LoadSampleFromMemory(void* pMemory, size_t const size, string const& samplePath, SampleId const overrideId)
{
	SampleId const id = (overrideId != 0) ? overrideId : StringToId(samplePath.c_str());
	Mix_Chunk* pSample = stl::find_in_map(g_sampleData, id, nullptr);

	if (pSample != nullptr)
	{
		Mix_FreeChunk(pSample);

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
		Cry::Audio::Log(ELogType::Warning, "Loading sample %s which had already been loaded", samplePath.c_str());
#endif    // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
	}

	SDL_RWops* pData = SDL_RWFromMem(pMemory, size);

	if (pData)
	{
		pSample = Mix_LoadWAV_RW(pData, 0);

		if (pSample != nullptr)
		{
			g_sampleData[id] = pSample;
			g_samplePaths[id] = samplePath;

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
			g_loadedSampleSize += size;
#endif      // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

			return id;
		}
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Error, R"(SDL Mixer failed to load sample. Error: "%s")", Mix_GetError());
		}
#endif    // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
	}
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	else
	{
		Cry::Audio::Log(ELogType::Error, R"(SDL Mixer failed to transform the audio data. Error: "%s")", SDL_GetError());
	}
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

	return g_invalidSampleId;
}

//////////////////////////////////////////////////////////////////////////
bool LoadSampleImpl(const SampleId id, const string& samplePath)
{
	bool bSuccess = true;
	Mix_Chunk* pSample = Mix_LoadWAV(samplePath.c_str());

	if (pSample != nullptr)
	{
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
		SampleNameMap::const_iterator it = g_samplePaths.find(id);

		if (it != g_samplePaths.end() && it->second != samplePath)
		{
			Cry::Audio::Log(ELogType::Error, "Loaded a Sample with the already existing ID %u, but from a different path source path '%s' <-> '%s'.", static_cast<uint>(id), it->second.c_str(), samplePath.c_str());
		}
		if (stl::find_in_map(g_sampleData, id, nullptr) != nullptr)
		{
			Cry::Audio::Log(ELogType::Error, "Loading sample '%s' which had already been loaded", samplePath.c_str());
		}
#endif    // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
		g_sampleData[id] = pSample;
		g_samplePaths[id] = samplePath;
	}
	else
	{
		// Sample could be inside a pak file so we need to open it manually and load it from the raw file
		size_t const fileSize = gEnv->pCryPak->FGetSize(samplePath);
		FILE* const pFile = gEnv->pCryPak->FOpen(samplePath, "rb");

		if (pFile && fileSize > 0)
		{
			void* const pData = CryModuleMalloc(fileSize);
			gEnv->pCryPak->FReadRawAll(pData, fileSize, pFile);
			SampleId const newId = SoundEngine::LoadSampleFromMemory(pData, fileSize, samplePath, id);

			if (newId == g_invalidSampleId)
			{
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
				Cry::Audio::Log(ELogType::Error, R"(SDL Mixer failed to load sample %s. Error: "%s")", samplePath.c_str(), Mix_GetError());
#endif        // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

				bSuccess = false;
			}

			CryModuleFree(pData);
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
const SampleId SoundEngine::LoadSample(string const& sampleFilePath, bool const onlyMetadata, bool const isLoacalized)
{
	SampleId const id = StringToId(sampleFilePath.c_str());

	if (stl::find_in_map(g_sampleData, id, nullptr) == nullptr)
	{
		if (onlyMetadata)
		{
			string const assetPath = isLoacalized ? s_localizedAssetsPath : s_regularAssetsPath;
			g_samplePaths[id] = assetPath + sampleFilePath;
		}
		else if (!LoadSampleImpl(id, sampleFilePath))
		{
			return g_invalidSampleId;
		}
	}

	return id;
}

//////////////////////////////////////////////////////////////////////////
void SoundEngine::Pause()
{
	Mix_Pause(-1);
	Mix_PauseMusic();
}

//////////////////////////////////////////////////////////////////////////
void SoundEngine::Resume()
{
	Mix_Resume(-1);
	Mix_ResumeMusic();
}

//////////////////////////////////////////////////////////////////////////
void SoundEngine::Stop()
{
	Mix_HaltChannel(-1);
}

//////////////////////////////////////////////////////////////////////////
void SoundEngine::Mute()
{
	Mix_Volume(-1, 0);
	g_bMuted = true;
}

//////////////////////////////////////////////////////////////////////////
void SoundEngine::UnMute()
{
	Objects::const_iterator objectIt = g_objects.begin();
	Objects::const_iterator const objectEnd = g_objects.end();

	for (; objectIt != objectEnd; ++objectIt)
	{
		CObject* const pObject = *objectIt;

		if (pObject != nullptr)
		{
			EventInstances::const_iterator eventIt = pObject->m_eventInstances.begin();
			EventInstances::const_iterator const eventEnd = pObject->m_eventInstances.end();

			for (; eventIt != eventEnd; ++eventIt)
			{
				CEventInstance* const pEventInstance = *eventIt;

				if (pEventInstance != nullptr)
				{
					CEvent const& event = pEventInstance->GetEvent();

					float const volumeMultiplier = GetVolumeMultiplier(pObject, event.GetSampleId());
					int const mixVolume = GetAbsoluteVolume(event.GetVolume(), volumeMultiplier);

					ChannelList::const_iterator channelIt = pEventInstance->m_channels.begin();
					ChannelList::const_iterator const channelEnd = pEventInstance->m_channels.end();

					for (; channelIt != channelEnd; ++channelIt)
					{
						Mix_Volume(*channelIt, mixVolume);
					}
				}
			}
		}
	}

	g_bMuted = false;
}

//////////////////////////////////////////////////////////////////////////
ETriggerResult SoundEngine::ExecuteEvent(CObject* const pObject, CEvent* const pEvent, TriggerInstanceId const triggerInstanceId)
{
	ETriggerResult result = ETriggerResult::Failure;

	CEvent::EActionType const type = pEvent->GetType();
	SampleId const sampleId = pEvent->GetSampleId();

	if (type == CEvent::EActionType::Start)
	{
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
		CEventInstance* const pEventInstance = g_pImpl->ConstructEventInstance(
			triggerInstanceId,
			*pEvent,
			*pObject);
#else
		CEventInstance* const pEventInstance = g_pImpl->ConstructEventInstance(
			triggerInstanceId,
			*pEvent);
#endif      // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

		// Start playing samples
		Mix_Chunk* pSample = stl::find_in_map(g_sampleData, sampleId, nullptr);

		if (pSample == nullptr)
		{
			// Trying to play sample that hasn't been loaded yet, load it in place
			// NOTE: This should be avoided as it can cause lag in audio playback
			string const& samplePath = g_samplePaths[sampleId];

			if (LoadSampleImpl(sampleId, samplePath))
			{
				pSample = stl::find_in_map(g_sampleData, sampleId, nullptr);
			}

			if (pSample == nullptr)
			{
				return ETriggerResult::Failure;
			}
		}

		if (!g_freeChannels.empty())
		{
			int const channelID = g_freeChannels.front();

			if (channelID >= 0)
			{
				g_freeChannels.pop();

				float const volumeMultiplier = GetVolumeMultiplier(pObject, sampleId);
				int const mixVolume = GetAbsoluteVolume(pEvent->GetVolume(), volumeMultiplier);
				Mix_Volume(channelID, g_bMuted ? 0 : mixVolume);

				int const fadeInTime = pEvent->GetFadeInTime();
				int const loopCount = pEvent->GetNumLoops();
				int channel = -1;

				if (fadeInTime > 0)
				{
					channel = Mix_FadeInChannel(channelID, pSample, loopCount, fadeInTime);
				}
				else
				{
					channel = Mix_PlayChannel(channelID, pSample, loopCount);
				}

				if (channel != -1)
				{
					// Get distance and angle from the listener to the object
					float distance = 0.0f;
					float angle = 0.0f;

					if (pObject->GetListener() != nullptr)
					{
						GetDistanceAngleToObject(pObject->GetListener()->GetTransformation(), pObject->GetTransformation(), distance, angle);
					}

					SetChannelPosition(pEventInstance->GetEvent(), channelID, distance, angle);

					g_channels[channelID].pObject = pObject;
					pEventInstance->m_channels.push_back(channelID);
				}
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
				else
				{
					Cry::Audio::Log(ELogType::Error, "Could not play sample. Error: %s", Mix_GetError());
				}
#endif          // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
			}
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
			else
			{
				Cry::Audio::Log(ELogType::Error, "Could not play sample. Error: %s", Mix_GetError());
			}
#endif        // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
		}
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Error, "Ran out of free audio channels. Are you trying to play more than %d samples?", g_numMixChannels);
		}
#endif      // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

		if (!pEventInstance->m_channels.empty())
		{
			// If any sample was added then add the event to the object
			pObject->m_eventInstances.push_back(pEventInstance);
			result = ETriggerResult::Playing;
		}
	}
	else
	{
		for (auto const pEventInstance : pObject->m_eventInstances)
		{
			if (pEventInstance->GetEvent().GetSampleId() == sampleId)
			{
				switch (type)
				{
				case CEvent::EActionType::Stop:
					{
						pEventInstance->Stop();
						break;
					}
				case CEvent::EActionType::Pause:
					{
						pEventInstance->Pause();
						break;
					}
				case CEvent::EActionType::Resume:
					{
						pEventInstance->Resume();
						break;
					}
				default:
					{
						break;
					}
				}
			}
		}

		result = ETriggerResult::DoNotTrack;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void SoundEngine::Update()
{
	{
		CryAutoLock<CryCriticalSection> lock(g_channelFinishedCriticalSection);
		g_channelFinishedRequests[IntegralValue(EChannelFinishedRequestQueueId::One)].swap(g_channelFinishedRequests[IntegralValue(EChannelFinishedRequestQueueId::Two)]);
	}

	ProcessChannelFinishedRequests(g_channelFinishedRequests[IntegralValue(EChannelFinishedRequestQueueId::Two)]);
}
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio
