// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <ITriggerConnection.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
static string s_localizedAssetsPath = "";

class CEventInstance;
class CObject;
class CEvent;

namespace SoundEngine
{
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
ETriggerResult ExecuteEvent(CObject* const pObject, CEvent* const pEvent, TriggerInstanceId const triggerInstanceId);
} // namespace SoundEngine
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio
