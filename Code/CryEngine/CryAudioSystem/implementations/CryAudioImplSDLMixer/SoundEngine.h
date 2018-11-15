// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <CryAudio/IAudioInterfacesCommonData.h>

namespace CryAudio
{
class CEvent;
class CStandaloneFile;

namespace Impl
{
namespace SDL_mixer
{
static string s_localizedAssetsPath = "";

class CEvent;
class CObject;
class CStandaloneFile;
class CTrigger;

namespace SoundEngine
{
using FnEventCallback = void (*)(CryAudio::CEvent&);
using FnStandaloneFileCallback = void (*)(CryAudio::CStandaloneFile&, char const*);

// Global events
bool Init();
void Release();
void Refresh();
void Update();

void Pause();
void Resume();

void Mute();
void UnMute();
void Stop();

// Load / Unload samples
const SampleId LoadSample(string const& sampleFilePath, bool const onlyMetadata, bool const isLoacalized);
const SampleId LoadSampleFromMemory(void* pMemory, const size_t size, const string& samplePath, const SampleId id = 0);
void           UnloadSample(const SampleId id);

// Events
ERequestStatus ExecuteEvent(CObject* const pObject, CTrigger const* const pTrigger, CEvent* const pEvent);
ERequestStatus PlayFile(CObject* const pObject, CStandaloneFile* const pStandaloneFile);

// stops all the events associated with this trigger
bool StopTrigger(CTrigger const* const pTrigger);

// Callbacks
void RegisterEventFinishedCallback(FnEventCallback pCallbackFunction);
void RegisterStandaloneFileFinishedCallback(FnStandaloneFileCallback pCallbackFunction);
} // namespace SoundEngine
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio
