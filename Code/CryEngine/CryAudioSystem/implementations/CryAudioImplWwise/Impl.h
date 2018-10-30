// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "FileIOHandler.h"
#include <IAudioImpl.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
class CImpl final : public IImpl
{
public:

	CImpl(CImpl const&) = delete;
	CImpl(CImpl&&) = delete;
	CImpl& operator=(CImpl const&) = delete;
	CImpl& operator=(CImpl&&) = delete;

	CImpl();

	// CryAudio::Impl::IImpl
	virtual void                Update() override;
	virtual ERequestStatus      Init(uint32 const objectPoolSize, uint32 const eventPoolSize) override;
	virtual void                ShutDown() override;
	virtual void                Release() override;
	virtual void                SetLibraryData(XmlNodeRef const pNode, bool const isLevelSpecific) override;
	virtual void                OnBeforeLibraryDataChanged() override;
	virtual void                OnAfterLibraryDataChanged() override;
	virtual ERequestStatus      OnLoseFocus() override;
	virtual ERequestStatus      OnGetFocus() override;
	virtual ERequestStatus      MuteAll() override;
	virtual ERequestStatus      UnmuteAll() override;
	virtual ERequestStatus      PauseAll() override;
	virtual ERequestStatus      ResumeAll() override;
	virtual ERequestStatus      StopAllSounds() override;
	virtual void                GamepadConnected(DeviceId const deviceUniqueID) override;
	virtual void                GamepadDisconnected(DeviceId const deviceUniqueID) override;
	virtual void                OnRefresh() override;
	virtual void                SetLanguage(char const* const szLanguage) override;
	virtual ERequestStatus      RegisterInMemoryFile(SFileInfo* const pFileInfo) override;
	virtual ERequestStatus      UnregisterInMemoryFile(SFileInfo* const pFileInfo) override;
	virtual ERequestStatus      ConstructFile(XmlNodeRef const pRootNode, SFileInfo* const pFileInfo) override;
	virtual void                DestructFile(IFile* const pIFile) override;
	virtual char const* const   GetFileLocation(SFileInfo* const pFileInfo) override;
	virtual void                GetInfo(SImplInfo& implInfo) const override;
	virtual ITrigger const*     ConstructTrigger(XmlNodeRef const pRootNode, float& radius) override;
	virtual ITrigger const*     ConstructTrigger(ITriggerInfo const* const pITriggerInfo) override;
	virtual void                DestructTrigger(ITrigger const* const pITrigger) override;
	virtual IParameter const*   ConstructParameter(XmlNodeRef const pRootNode) override;
	virtual void                DestructParameter(IParameter const* const pIParameter) override;
	virtual ISwitchState const* ConstructSwitchState(XmlNodeRef const pRootNode) override;
	virtual void                DestructSwitchState(ISwitchState const* const pISwitchState) override;
	virtual IEnvironment const* ConstructEnvironment(XmlNodeRef const pRootNode) override;
	virtual void                DestructEnvironment(IEnvironment const* const pIEnvironment) override;
	virtual ISetting const*     ConstructSetting(XmlNodeRef const pRootNode) override;
	virtual void                DestructSetting(ISetting const* const pISetting) override;
	virtual IObject*            ConstructGlobalObject() override;
	virtual IObject*            ConstructObject(CObjectTransformation const& transformation, char const* const szName = nullptr) override;
	virtual void                DestructObject(IObject const* const pIObject) override;
	virtual IListener*          ConstructListener(CObjectTransformation const& transformation, char const* const szName = nullptr) override;
	virtual void                DestructListener(IListener* const pIListener) override;
	virtual IEvent*             ConstructEvent(CATLEvent& event) override;
	virtual void                DestructEvent(IEvent const* const pIEvent) override;
	virtual IStandaloneFile*    ConstructStandaloneFile(CATLStandaloneFile& standaloneFile, char const* const szFile, bool const bLocalized, ITrigger const* pITrigger = nullptr) override;
	virtual void                DestructStandaloneFile(IStandaloneFile const* const pIStandaloneFile) override;

	// Below data is only used when INCLUDE_WWISE_IMPL_PRODUCTION_CODE is defined!
	virtual void GetFileData(char const* const szName, SFileData& fileData) const override;
	virtual void DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float& posY, bool const showDetailedInfo) override;
	// ~CryAudio::Impl::IImpl

	void SetPanningRule(int const panningRule);

private:

	bool ParseSwitchOrState(XmlNodeRef const pNode, AkUInt32& outStateOrSwitchGroupId, AkUInt32& outStateOrSwitchId);
	void ParseRtpcImpl(XmlNodeRef const pNode, AkRtpcID& rtpcId, float& multiplier, float& shift);
	void SignalAuxAudioThread();
	void WaitForAuxAudioThread();

	AkGameObjectID                     m_gameObjectId;

	AkBankID                           m_initBankId;
	CFileIOHandler                     m_fileIOHandler;

	CryFixedStringT<MaxFilePathLength> m_regularSoundBankFolder;
	CryFixedStringT<MaxFilePathLength> m_localizedSoundBankFolder;

	struct SInputDeviceInfo
	{
		bool             akIsActiveOutputDevice;
		AkOutputDeviceID akOutputDeviceID;
	};

	using AudioInputDevices = std::map<DeviceId, SInputDeviceInfo>;
	AudioInputDevices m_mapInputDevices;

#if !defined(WWISE_FOR_RELEASE)
	bool m_bCommSystemInitialized;
#endif  // !WWISE_FOR_RELEASE

#if defined(INCLUDE_WWISE_IMPL_PRODUCTION_CODE)
	void GetInitBankSize();
	CryFixedStringT<MaxInfoStringLength> m_name;
	size_t                               m_initBankSize = 0;
#endif  // INCLUDE_WWISE_IMPL_PRODUCTION_CODE

#if defined(WWISE_USE_OCULUS)
	void* m_pOculusSpatializerLibrary;
#endif  // WWISE_USE_OCULUS
};
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
