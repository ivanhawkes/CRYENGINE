// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IPlugin.h>
#include <IEditor.h>

#include "AssetsManager.h"

namespace ACE
{
class CImplementationManager;
extern CAssetsManager g_assetsManager;
extern CImplementationManager g_implementationManager;

enum class EReloadFlags : CryAudio::EnumFlagsType
{
	None = 0,
	ReloadSystemControls = BIT(0),
	ReloadImplData = BIT(1),
	ReloadScopes = BIT(2),
	SendSignals = BIT(3),
	BackupConnections = BIT(4), };
CRY_CREATE_ENUM_FLAG_OPERATORS(EReloadFlags);

class CAudioControlsEditorPlugin final : public IPlugin, public ISystemEventListener
{
public:

	CAudioControlsEditorPlugin(CAudioControlsEditorPlugin const&) = delete;
	CAudioControlsEditorPlugin(CAudioControlsEditorPlugin&&) = delete;
	CAudioControlsEditorPlugin& operator=(CAudioControlsEditorPlugin const&) = delete;
	CAudioControlsEditorPlugin& operator=(CAudioControlsEditorPlugin&&) = delete;

	CAudioControlsEditorPlugin();
	virtual ~CAudioControlsEditorPlugin() override;

	// IPlugin
	virtual int32       GetPluginVersion() override     { return 1; }
	virtual char const* GetPluginName() override        { return "Audio Controls Editor"; }
	virtual char const* GetPluginDescription() override { return "The Audio Controls Editor enables browsing and configuring audio events exposed from the audio middleware"; }
	// ~IPlugin

	static void       SaveData();
	static void       ReloadData(EReloadFlags const flags);
	static void       ExecuteTrigger(string const& sTriggerName);
	static void       StopTriggerExecution();
	static EErrorCode GetLoadingErrorMask() { return s_loadingErrorMask; }

	static CCrySignal<void()> SignalOnBeforeLoad;
	static CCrySignal<void()> SignalOnAfterLoad;
	static CCrySignal<void()> SignalOnBeforeSave;
	static CCrySignal<void()> SignalOnAfterSave;

private:

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	static void ReloadImplData(EReloadFlags const flags);

	static FileNames           s_currentFilenames;
	static CryAudio::ControlId s_audioTriggerId;

	static EErrorCode          s_loadingErrorMask;
};
} // namespace ACE
