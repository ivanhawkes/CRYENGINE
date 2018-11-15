// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CrySystem/XML/IXml.h>

struct IRenderAuxGeom;

namespace CryAudio
{
class CEvent;
class CStandaloneFile;

using DeviceId = uint8;

namespace Impl
{
struct IEnvironmentConnection;
struct IEvent;
struct IFile;
struct IListener;
struct IObject;
struct IParameterConnection;
struct ISettingConnection;
struct IStandaloneFileConnection;
struct ISwitchStateConnection;
struct ITriggerConnection;
struct ITriggerInfo;
struct SFileInfo;

struct IImpl
{
	/** @cond */
	virtual ~IImpl() = default;
	/** @endcond */

	/**
	 * Is called at roughly the same rate as the external thread and a minimum rate if the external thread falls below a given threshold.
	 * @return void
	 */
	virtual void Update() = 0;

	/**
	 * Initialize all internal components and the audio middleware.
	 * @param objectPoolSize - Number of objects to preallocate storage for.
	 * @param eventPoolSize - Number of events to preallocate storage for.
	 * @return ERequestStatus::Success if the initialization was successful, ERequestStatus::Failure otherwise.
	 * @see ShutDown
	 */
	virtual ERequestStatus Init(uint16 const objectPoolSize, uint16 const eventPoolSize) = 0;

	/**
	 * Shuts down all of the internal components and the audio middleware.
	 * Note: After a call to ShutDown(), the system can still be reinitialized by calling Init().
	 * @return void
	 * @see Release, Init
	 */
	virtual void ShutDown() = 0;

	/**
	 * The audio system signals that the implementation is about to be released.
	 * @return void
	 * @see ShutDown, Init, Release
	 */
	virtual void OnBeforeRelease() = 0;

	/**
	 * Frees all of the resources used by the class and destroys the instance. This action is not reversible.
	 * @return void
	 * @see ShutDown, Init, OnBeforeRelease
	 */
	virtual void Release() = 0;

	/**
	 * Perform a "hot restart" of the audio middleware. Reset all of the internal data.
	 * @see Release, Init
	 * @return void
	 */
	virtual void OnRefresh() = 0;

	/**
	 * Pass an implementation specific XML node that contains information about the current parsed library.
	 * Is called for each library that contains the node, before calling Init.
	 * @param pNode - an XML node containing information about the current parsed library
	 * @param isLevelSpecific - false: the library is global. true: the library is level specific.
	 * @return void
	 */
	virtual void SetLibraryData(XmlNodeRef const pNode, bool const isLevelSpecific) = 0;

	/**
	 * Called before parsing all libraries for impl data.
	 * @see OnAfterLibraryDataChanged, SetLibraryData
	 * @return void
	 */
	virtual void OnBeforeLibraryDataChanged() = 0;

	/**
	 * Called when parsing all libraries for impl data is finished.
	 * @see OnBeforeLibraryDataChanged, SetLibraryData
	 * @return void
	 */
	virtual void OnAfterLibraryDataChanged() = 0;

	/**
	 * This method is called every time the main Game (or Editor) window loses focus.
	 * @return void
	 * @see OnGetFocus
	 */
	virtual void OnLoseFocus() = 0;

	/**
	 * This method is called every time the main Game (or Editor) window gets focus.
	 * @return void
	 * @see OnLoseFocus
	 */
	virtual void OnGetFocus() = 0;

	/**
	 * Mute all sounds, after this call there should be no audio coming from the audio middleware.
	 * @return void
	 * @see UnmuteAll, StopAllSounds
	 */
	virtual void MuteAll() = 0;

	/**
	 * Restore the audio output of the audio middleware after a call to MuteAll().
	 * @return void
	 * @see MuteAll
	 */
	virtual void UnmuteAll() = 0;

	/**
	 * Pauses playback of all audio events.
	 * @return void
	 * @see ResumeAll
	 */
	virtual void PauseAll() = 0;

	/**
	 * Resumes playback of all audio events.
	 * @return void
	 * @see PauseAll
	 */
	virtual void ResumeAll() = 0;

	/**
	 * Stop all currently playing sounds. Has no effect on anything triggered after this method is called.
	 * @return ERequestStatus::Success if the action was successful, ERequestStatus::Failure otherwise.
	 * @see MuteAll
	 */
	virtual ERequestStatus StopAllSounds() = 0;

	/**
	 * Sets a parameter on all objects.
	 * @param pIParameter - Parameter to set
	 * @param value - Value to set
	 * @return void
	 */
	virtual void SetGlobalParameter(IParameterConnection const* const pIParameterConnection, float const value) = 0;

	/**
	 * Sets a switch state on all objects.
	 * @param pISwitchStateConnection - Switch state to set
	 * @return void
	 */
	virtual void SetGlobalSwitchState(ISwitchStateConnection const* const pISwitchStateConnection) = 0;

	/**
	 * Inform the audio middleware about the memory location of a preloaded audio-data file
	 * @param pFileInfo - audio system-specific information describing the resources used by the preloaded file being reported
	 * @return ERequestStatus::Success if the audio middleware is able to use the preloaded file, ERequestStatus::Failure otherwise
	 * @see UnregisterInMemoryFile
	 */
	virtual ERequestStatus RegisterInMemoryFile(SFileInfo* const pFileInfo) = 0;

	/**
	 * Inform the audio middleware that the memory containing the preloaded audio-data file should no longer be used
	 * @param pFileInfo - audio system-specific information describing the resources used by the preloaded file being invalidated
	 * @return ERequestStatus::Success if the audio middleware was able to unregister the preloaded file supplied, ERequestStatus::Failure otherwise
	 * @see RegisterInMemoryFile
	 */
	virtual ERequestStatus UnregisterInMemoryFile(SFileInfo* const pFileInfo) = 0;

	/**
	 * Parse the implementation-specific XML node that represents an audio file, fill the fields of the struct
	 * referenced by pFileInfo with the data necessary to correctly access and store the file's contents in memory.
	 * Create an object implementing IFile to hold implementation-specific data about the file and store a pointer to it in a member of pFileInfo
	 * @param pRootNode - an XML node containing the necessary information about the file
	 * @param pFileInfo - a pointer to the struct containing the data used by the audio system to load the file into memory
	 * @return ERequestStatus::Success if the XML node was parsed successfully, ERequestStatus::Failure otherwise
	 * @see DestructFile
	 */
	virtual ERequestStatus ConstructFile(XmlNodeRef const pRootNode, SFileInfo* const pFileInfo) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied IFile instance.
	 * Normally, an IFile instance is created by ConstructFile() and a pointer is stored in a member of SFileInfo.
	 * @param pIFile - pointer to the object implementing IFile to be discarded
	 * @return void
	 * @see ConstructFile
	 */
	virtual void DestructFile(IFile* const pIFile) = 0;

	/**
	 * Get the full path to the folder containing the file described by the pFileInfo
	 * @param pFileInfo - audio system-specific information describing the file whose location is being queried
	 * @return A C-string containing the path to the folder where the file corresponding to the pFileInfo is stored
	 */
	virtual char const* const GetFileLocation(SFileInfo* const pFileInfo) = 0;

	/**
	 * Parse the implementation-specific XML node that represents an ITriggerConnection, return a pointer to the data needed for identifying
	 * and using this trigger connection instance inside the AudioImplementation
	 * @param pRootNode - an XML node corresponding to the new ITriggerConnection to be created
	 * @param radius - the max attenuation radius of the trigger. Set to 0.0f for 2D sounds. Used for debug draw.
	 * @return ITrigger pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding ITriggerConnection; nullptr if the new AudioTriggerImplData instance was not created
	 * @see DestructTrigger
	 */
	virtual ITriggerConnection const* ConstructTriggerConnection(XmlNodeRef const pRootNode, float& radius) = 0;

	/**
	 * Construct a trigger with the given info struct, return a pointer to the data needed for identifying
	 * and using this trigger connection instance inside the AudioImplementation
	 * @param info - reference to an info struct corresponding to the new ITriggerConnection to be created
	 * @return ITrigger pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding ITriggerConnection; nullptr if the new ITriggerConnection instance was not created
	 * @see DestructTrigger
	 */
	virtual ITriggerConnection const* ConstructTriggerConnection(ITriggerInfo const* const pITriggerInfo) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied ITrigger instance
	 * @param pITriggerConnection - pointer to the object implementing pITriggerConnection to be discarded
	 * @return void
	 * @see ConstructTrigger
	 */
	virtual void DestructTriggerConnection(ITriggerConnection const* const pITriggerConnection) = 0;

	/**
	 * Create an object implementing ConstructStandaloneFile that stores all of the data needed by the AudioImplementation
	 * to identify and use an audio standalone file. Return a pointer to that object.
	 * @param standaloneFile - reference to the CStandaloneFile associated with the IStandaloneFile object we want to construct. It's used as an ID to link the two objects.
	 * @param szFile - full path to the file that wants to be played
	 * @param bLocalized - is the file specified in szFile localized or not
	 * @param pITriggerConnection - if set, routes the playing of the audio file through the specified implementation trigger
	 * @return IStandaloneFile pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding audio standalone file.
	 * @return nullptr if the new IStandaloneFileConnection instance was not created.
	 * @see DestructStandaloneFile
	 */
	virtual IStandaloneFileConnection* ConstructStandaloneFileConnection(CStandaloneFile& standaloneFile, char const* const szFile, bool const bLocalized, ITriggerConnection const* pITriggerConnection = nullptr) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied IStandaloneFileConnection instance
	 * @param pIStandaloneFileConnection - pointer to the object implementing IStandaloneFileConnection to be discarded
	 * @return void
	 * @see ConstructStandaloneFile
	 */
	virtual void DestructStandaloneFileConnection(IStandaloneFileConnection const* const pIStandaloneFileConnection) = 0;

	/**
	 * Parse the implementation-specific XML node that represents an IParameterConnection, return a pointer to the data needed for identifying
	 * and using this IParameterConnection instance inside the AudioImplementation
	 * @param pRootNode - an XML node corresponding to the new IParameterConnection to be created
	 * @return IParameter pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding IParameterConnection; nullptr if the new IParameterConnection instance was not created
	 * @see DestructParameter
	 */
	virtual IParameterConnection const* ConstructParameterConnection(XmlNodeRef const pRootNode) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied IParameterConnection instance
	 * @param pIParameterConnection - pointer to the object implementing IParameterConnection to be discarded
	 * @return void
	 * @see ConstructParameter
	 */
	virtual void DestructParameterConnection(IParameterConnection const* const pIParameterConnection) = 0;

	/**
	 * Parse the implementation-specific XML node that represents an ISwitchStateConnection, return a pointer to the data needed for identifying
	 * and using this ISwitchStateConnection instance inside the AudioImplementation
	 * @param pRootNode - an XML node corresponding to the new ISwitchStateConnection to be created
	 * @return ISwitchState pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding ISwitchStateConnection; nullptr if the new ISwitchStateConnection instance was not created
	 * @see DestructSwitchState
	 */
	virtual ISwitchStateConnection const* ConstructSwitchStateConnection(XmlNodeRef const pRootNode) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied ISwitchStateConnection instance
	 * @param pISwitchStateConnection - pointer to the object implementing ISwitchStateConnection to be discarded
	 * @return void
	 * @see ConstructSwitchState
	 */
	virtual void DestructSwitchStateConnection(ISwitchStateConnection const* const pISwitchStateConnection) = 0;

	/**
	 * Parse the implementation-specific XML node that represents an IEnvironmentConnection, return a pointer to the data needed for identifying
	 * and using this IEnvironmentConnection instance inside the AudioImplementation
	 * @param pRootNode - an XML node corresponding to the new IEnvironmentConnection to be created
	 * @return IEnvironment pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding IEnvironmentConnection; nullptr if the new IEnvironmentConnection instance was not created
	 * @see DestructEnvironment
	 */
	virtual IEnvironmentConnection const* ConstructEnvironmentConnection(XmlNodeRef const pRootNode) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied IEnvironmentConnection instance
	 * @param pIEnvironmentConnection - pointer to the object implementing IEnvironmentConnection to be discarded
	 * @return void
	 * @see ConstructEnvironment
	 */
	virtual void DestructEnvironmentConnection(IEnvironmentConnection const* const pIEnvironmentConnection) = 0;

	/**
	 * Parse the implementation-specific XML node that represents an ISettingConnection, return a pointer to the data needed for identifying
	 * and using this ISettingConnection instance inside the AudioImplementation
	 * @param pRootNode - an XML node corresponding to the new ISettingConnection to be created
	 * @return ISetting pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding ISettingConnection; nullptr if the new ISettingConnection instance was not created
	 * @see DestructSetting
	 */
	virtual ISettingConnection const* ConstructSettingConnection(XmlNodeRef const pRootNode) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied ISettingConnection instance
	 * @param pISettingConnection - pointer to the object implementing ISettingConnection to be discarded
	 * @return void
	 * @see ConstructSetting
	 */
	virtual void DestructSettingConnection(ISettingConnection const* const pISettingConnection) = 0;

	/**
	 * Create an object implementing IObject that stores all of the data needed by the AudioImplementation
	 * to identify and use the GlobalAudioObject.
	 * @return IObject pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding GlobalAudioObject; nullptr if the new IObject instance was not created
	 * @see DestructObject
	 */
	virtual IObject* ConstructGlobalObject() = 0;

	/**
	 * Create an object implementing IObject that stores all of the data needed by the AudioImplementation
	 * to identify and use the AudioObject. Return a pointer to that object.
	 * @param transformation - transformation of the object to construct
	 * @param szName - optional name of the object to construct (not used in release builds)
	 * @return IObject pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding GlobalAudioObject; nullptr if the new IObject instance was not created
	 * @see DestructObject
	 */
	virtual IObject* ConstructObject(CTransformation const& transformation, char const* const szName = nullptr) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied IObject instance
	 * @param pIObject - pointer to the object implementing IObject to be discarded
	 * @return void
	 * @see ConstructObject, ConstructGlobalObject
	 */
	virtual void DestructObject(IObject const* const pIObject) = 0;

	/**
	 * Construct an object implementing IListener that stores all of the data needed by the AudioImplementation
	 * to identify and use an AudioListener. Return a pointer to that object.
	 * @param transformation - transformation of the listener to construct
	 * @param szName - optional name of the listener to construct (not used in release builds)
	 * @return CryAudio::Impl::IListener pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding AudioListener; nullptr if the new CryAudio::Impl::IListener instance was not created.
	 * @see DestructListener
	 */
	virtual IListener* ConstructListener(CTransformation const& transformation, char const* const szName = nullptr) = 0;

	/**
	 * Destruct the supplied CryAudio::Impl::IListener instance.
	 * @param pIListener - pointer to the object implementing CryAudio::Impl::IListener to be discarded
	 * @return void
	 * @see ConstructListener
	 */
	virtual void DestructListener(IListener* const pIListener) = 0;

	/**
	 * Create an object implementing IEvent that stores all of the data needed by the AudioImplementation
	 * to identify and use an AudioEvent. Return a pointer to that object.
	 * @param event - implementation event associated with the newly created AudioEvent
	 * @return IEvent pointer to the audio implementation-specific data needed by the audio middleware and the
	 * @return AudioImplementation code to use the corresponding AudioEvent; nullptr if the new IEvent instance was not created
	 * @see DestructEvent
	 */
	virtual IEvent* ConstructEvent(CEvent& event) = 0;

	/**
	 * Free the memory and potentially other resources used by the supplied IEvent instance
	 * @param pIEvent - pointer to the object implementing IEvent to be discarded
	 * @return void
	 * @see ConstructEvent
	 */
	virtual void DestructEvent(IEvent const* const pIEvent) = 0;

	/**
	 * Called whenever a Gamepad gets connected.
	 * This is used by audio middleware that supports controller effects such as rumble.
	 * @param deviceUniqueID - unique device identifier
	 * @return void
	 * @see GamepadDisconnected
	 */
	virtual void GamepadConnected(DeviceId const deviceUniqueID) = 0;

	/**
	 * Called whenever a Gamepad gets disconnected.
	 * This is used by audio middleware that supports controller effects such as rumble.
	 * @param deviceUniqueID - unique device identifier
	 * @return void
	 * @see GamepadConnected
	 */
	virtual void GamepadDisconnected(DeviceId const deviceUniqueID) = 0;

	/**
	 * Informs the audio middleware that the localized sound banks and streamed files need to use
	 * a different language. NOTE: this function DOES NOT unload or reload the currently loaded audio files
	 * @param szLanguage - a C-string representing the CryEngine language
	 * @return void
	 * @see GetFileLocation
	 */
	virtual void SetLanguage(char const* const szLanguage) = 0;

	/**
	 * Return a string of the audio middeware folder name plus a separator. This string is used for building the path
	 * to audio files and audio controls editor data.
	 * @param[out] implInfo - a reference to an instance of SImplInfo
	 * @return void
	 */
	virtual void GetInfo(SImplInfo& implInfo) const = 0;

	//////////////////////////////////////////////////////////////////////////
	// NOTE: The methods below are ONLY USED when INCLUDE_AUDIO_PRODUCTION_CODE is defined!
	//////////////////////////////////////////////////////////////////////////

	/**
	 * Asks the audio implementation to fill the fileData structure with data (e.g. duration of track) relating to the
	 * standalone file referenced in szName.
	 * @param[in] szName - filepath to the standalone file
	 * @param[out] fileData - a reference to an instance of SAudioFileData
	 * @return void
	 */
	virtual void GetFileData(char const* const szName, SFileData& fileData) const = 0;

	/**
	 * Informs the audio middlware that it can draw its debug information.
	 * @param[out] auxGeom - a reference to the IRenderAuxGeom that draws the debug info.
	 * @param[in] posX - x-axis position of the auxGeom.
	 * @param[out] posY - y-axis position of the auxGeom.
	 * @param[in] showDetailedInfo - should detailed memory info be shown or not.
	 * @return void
	 */
	virtual void DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float& posY, bool const showDetailedInfo) = 0;
};
} // namespace Impl
} // namespace CryAudio
