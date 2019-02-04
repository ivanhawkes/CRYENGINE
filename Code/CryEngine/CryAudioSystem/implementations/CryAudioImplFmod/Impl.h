// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IImpl.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CEventInstance;
class CEvent;
class CBaseObject;

class CImpl final : public IImpl
{
public:

	CImpl();
	CImpl(CImpl const&) = delete;
	CImpl(CImpl&&) = delete;
	CImpl& operator=(CImpl const&) = delete;
	CImpl& operator=(CImpl&&) = delete;

	// CryAudio::Impl::IImpl
	virtual void                       Update() override;
	virtual ERequestStatus             Init(uint16 const objectPoolSize) override;
	virtual void                       ShutDown() override;
	virtual void                       OnBeforeRelease() override {}
	virtual void                       Release() override;
	virtual void                       SetLibraryData(XmlNodeRef const pNode, bool const isLevelSpecific) override;
	virtual void                       OnBeforeLibraryDataChanged() override;
	virtual void                       OnAfterLibraryDataChanged() override;
	virtual void                       OnLoseFocus() override;
	virtual void                       OnGetFocus() override;
	virtual void                       MuteAll() override;
	virtual void                       UnmuteAll() override;
	virtual void                       PauseAll() override;
	virtual void                       ResumeAll() override;
	virtual ERequestStatus             StopAllSounds() override;
	virtual void                       RegisterInMemoryFile(SFileInfo* const pFileInfo) override;
	virtual void                       UnregisterInMemoryFile(SFileInfo* const pFileInfo) override;
	virtual ERequestStatus             ConstructFile(XmlNodeRef const pRootNode, SFileInfo* const pFileInfo) override;
	virtual void                       DestructFile(IFile* const pIFile) override;
	virtual char const* const          GetFileLocation(SFileInfo* const pFileInfo) override;
	virtual void                       GetInfo(SImplInfo& implInfo) const override;
	virtual ITriggerConnection*        ConstructTriggerConnection(XmlNodeRef const pRootNode, float& radius) override;
	virtual ITriggerConnection*        ConstructTriggerConnection(ITriggerInfo const* const pITriggerInfo) override;
	virtual void                       DestructTriggerConnection(ITriggerConnection const* const pITriggerConnection) override;
	virtual IParameterConnection*      ConstructParameterConnection(XmlNodeRef const pRootNode) override;
	virtual void                       DestructParameterConnection(IParameterConnection const* const pIParameterConnection) override;
	virtual ISwitchStateConnection*    ConstructSwitchStateConnection(XmlNodeRef const pRootNode) override;
	virtual void                       DestructSwitchStateConnection(ISwitchStateConnection const* const pISwitchStateConnection) override;
	virtual IEnvironmentConnection*    ConstructEnvironmentConnection(XmlNodeRef const pRootNode) override;
	virtual void                       DestructEnvironmentConnection(IEnvironmentConnection const* const pIEnvironmentConnection) override;
	virtual ISettingConnection*        ConstructSettingConnection(XmlNodeRef const pRootNode) override;
	virtual void                       DestructSettingConnection(ISettingConnection const* const pISettingConnection) override;
	virtual IObject*                   ConstructGlobalObject() override;
	virtual IObject*                   ConstructObject(CTransformation const& transformation, char const* const szName = nullptr) override;
	virtual void                       DestructObject(IObject const* const pIObject) override;
	virtual IListener*                 ConstructListener(CTransformation const& transformation, char const* const szName = nullptr) override;
	virtual void                       DestructListener(IListener* const pIListener) override;
	virtual IStandaloneFileConnection* ConstructStandaloneFileConnection(CryAudio::CStandaloneFile& standaloneFile, char const* const szFile, bool const bLocalized, ITriggerConnection const* pITriggerConnection = nullptr) override;
	virtual void                       DestructStandaloneFileConnection(IStandaloneFileConnection const* const pIStandaloneFileConnection) override;
	virtual void                       GamepadConnected(DeviceId const deviceUniqueID) override;
	virtual void                       GamepadDisconnected(DeviceId const deviceUniqueID) override;
	virtual void                       OnRefresh() override;
	virtual void                       SetLanguage(char const* const szLanguage) override;

	// Below data is only used when CRY_AUDIO_IMPL_FMOD_USE_PRODUCTION_CODE is defined!
	virtual void GetFileData(char const* const szName, SFileData& fileData) const override;
	virtual void DrawDebugMemoryInfo(IRenderAuxGeom& auxGeom, float const posX, float& posY, bool const showDetailedInfo) override;
	virtual void DrawDebugInfoList(IRenderAuxGeom& auxGeom, float& posX, float posY, float const debugDistance, char const* const szTextFilter) const override;
	// ~CryAudio::Impl::IImpl

	CEventInstance* ConstructEventInstance(
		TriggerInstanceId const triggerInstanceId,
		uint32 const eventId,
		CEvent const* const pEvent,
		CBaseObject const* const pBaseObject = nullptr);
	void DestructEventInstance(CEventInstance const* const pEventInstance);

private:

	void        CreateVersionString(CryFixedStringT<MaxInfoStringLength>& stringOut) const;
	void        LoadMasterBanks();
	void        UnloadMasterBanks();
	void        MuteMasterBus(bool const shouldMute);
	void        PauseMasterBus(bool const shouldPause);

	FMOD_RESULT LoadBankCustom(char const* const szFileName, FMOD::Studio::Bank** ppBank);

	CryFixedStringT<MaxFilePathLength>    m_regularSoundBankFolder;
	CryFixedStringT<MaxFilePathLength>    m_localizedSoundBankFolder;

	CryFixedStringT<MaxFilePathLength>    m_masterBankPath;
	CryFixedStringT<MaxFilePathLength>    m_masterAssetsBankPath;
	CryFixedStringT<MaxFilePathLength>    m_masterStreamsBankPath;
	CryFixedStringT<MaxFilePathLength>    m_masterStringsBankPath;

	FMOD::Studio::System*                 m_pSystem;
	FMOD::System*                         m_pLowLevelSystem;
	FMOD::Studio::Bank*                   m_pMasterBank;
	FMOD::Studio::Bank*                   m_pMasterAssetsBank;
	FMOD::Studio::Bank*                   m_pMasterStreamsBank;
	FMOD::Studio::Bank*                   m_pMasterStringsBank;
	CryFixedStringT<MaxControlNameLength> m_language;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_PRODUCTION_CODE)
	CryFixedStringT<MaxInfoStringLength> m_name;
	size_t                               m_masterBankSize = 0;
	size_t                               m_masterAssetsBankSize = 0;
	size_t                               m_masterStringsBankSize = 0;
#endif  // CRY_AUDIO_IMPL_FMOD_USE_PRODUCTION_CODE
};
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
