// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FileLoader.h"

#include "AudioControlsEditorPlugin.h"
#include "Control.h"

#include <CrySystem/File/CryFile.h>
#include <QtUtil.h>
#include <CryGame/IGameFramework.h>
#include <ILevelSystem.h>

#include <QRegularExpression>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
EAssetType TagToType(char const* const szTag)
{
	EAssetType type = EAssetType::None;

	if (_stricmp(szTag, CryAudio::g_szSwitchTag) == 0)
	{
		type = EAssetType::Switch;
	}
	else if (_stricmp(szTag, CryAudio::g_szStateTag) == 0)
	{
		type = EAssetType::State;
	}
	else if (_stricmp(szTag, CryAudio::g_szEnvironmentTag) == 0)
	{
		type = EAssetType::Environment;
	}
	else if (_stricmp(szTag, CryAudio::g_szParameterTag) == 0)
	{
		type = EAssetType::Parameter;
	}
	else if (_stricmp(szTag, CryAudio::g_szTriggerTag) == 0)
	{
		type = EAssetType::Trigger;
	}
	else if (_stricmp(szTag, CryAudio::g_szPreloadRequestTag) == 0)
	{
		type = EAssetType::Preload;
	}
	else if (_stricmp(szTag, CryAudio::g_szSettingTag) == 0)
	{
		type = EAssetType::Setting;
	}
	else
	{
		type = EAssetType::None;
	}

	return type;
}

//////////////////////////////////////////////////////////////////////////
CAsset* AddUniqueFolderPath(CAsset* pParent, QString const& path)
{
	QStringList const folderNames = path.split(QRegularExpression(R"((\\|\/))"), QString::SkipEmptyParts);

	int const numFolders = folderNames.length();

	for (int i = 0; i < numFolders; ++i)
	{
		if (!folderNames[i].isEmpty())
		{
			CAsset* const pChild = g_assetsManager.CreateFolder(QtUtil::ToString(folderNames[i]), pParent);

			if (pChild != nullptr)
			{
				pParent = pChild;
			}
		}
	}

	return pParent;
}

//////////////////////////////////////////////////////////////////////////
void CreateDefaultControls()
{
	// Create default controls if they don't exist.
	// These controls need to always exist in your project!
	CAsset* const pLibrary = static_cast<CAsset*>(g_assetsManager.CreateLibrary(CryAudio::g_szDefaultLibraryName));

	if (pLibrary != nullptr)
	{
		EAssetFlags const flags = (EAssetFlags::IsDefaultControl | EAssetFlags::IsHiddenInResourceSelector);

		g_assetsManager.CreateDefaultControl(CryAudio::g_szGetFocusTriggerName, EAssetType::Trigger, pLibrary, flags, "Unmutes all audio. Gets triggered when the editor window gets focus.");
		g_assetsManager.CreateDefaultControl(CryAudio::g_szLoseFocusTriggerName, EAssetType::Trigger, pLibrary, flags, "Mutes all audio. Gets triggered when the editor window loses focus.");
		g_assetsManager.CreateDefaultControl(CryAudio::g_szMuteAllTriggerName, EAssetType::Trigger, pLibrary, flags, "Mutes all audio. Gets triggered when the editor mute action is used.");
		g_assetsManager.CreateDefaultControl(CryAudio::g_szUnmuteAllTriggerName, EAssetType::Trigger, pLibrary, flags, "Unmutes all audio. Gets triggered when the editor unmute action is used.");
		g_assetsManager.CreateDefaultControl(CryAudio::g_szPauseAllTriggerName, EAssetType::Trigger, pLibrary, flags, "Pauses playback of all audio.");
		g_assetsManager.CreateDefaultControl(CryAudio::g_szResumeAllTriggerName, EAssetType::Trigger, pLibrary, flags, "Resumes playback of all audio.");
	}
}

//////////////////////////////////////////////////////////////////////////
void LoadConnections(XmlNodeRef const pRoot, CControl* const pControl)
{
	int const numChildren = pRoot->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const pNode = pRoot->getChild(i);
		pControl->LoadConnectionFromXML(pNode);
	}
}

//////////////////////////////////////////////////////////////////////////
void LoadLibraryEditorData(XmlNodeRef const pLibraryNode, CAsset& library)
{
	string description = "";
	pLibraryNode->getAttr(g_szDescriptionAttribute, description);

	if (!description.IsEmpty())
	{
		library.SetDescription(description);
	}
}

//////////////////////////////////////////////////////////////////////////
void LoadFolderData(XmlNodeRef const pFolderNode, CAsset& parentAsset)
{
	CAsset* const pAsset = AddUniqueFolderPath(&parentAsset, pFolderNode->getAttr(CryAudio::g_szNameAttribute));

	if (pAsset != nullptr)
	{
		string description = "";
		pFolderNode->getAttr(g_szDescriptionAttribute, description);

		if (!description.IsEmpty())
		{
			pAsset->SetDescription(description);
		}

		int const numChildren = pFolderNode->getChildCount();

		for (int i = 0; i < numChildren; ++i)
		{
			LoadFolderData(pFolderNode->getChild(i), *pAsset);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void LoadAllFolders(XmlNodeRef const pFoldersNode, CAsset& library)
{
	if (pFoldersNode != nullptr)
	{
		int const numChildren = pFoldersNode->getChildCount();

		for (int i = 0; i < numChildren; ++i)
		{
			LoadFolderData(pFoldersNode->getChild(i), library);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void LoadControlsEditorData(XmlNodeRef const pParentNode)
{
	if (pParentNode != nullptr)
	{
		EAssetType const controlType = TagToType(pParentNode->getTag());
		string description = "";
		pParentNode->getAttr(g_szDescriptionAttribute, description);

		if ((controlType != EAssetType::None) && !description.IsEmpty())
		{
			CControl* const pControl = g_assetsManager.FindControl(pParentNode->getAttr(CryAudio::g_szNameAttribute), controlType);

			if (pControl != nullptr)
			{
				pControl->SetDescription(description);
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////
void LoadAllControlsEditorData(XmlNodeRef const pControlsNode)
{
	if (pControlsNode != nullptr)
	{
		int const numChildren = pControlsNode->getChildCount();

		for (int i = 0; i < numChildren; ++i)
		{
			LoadControlsEditorData(pControlsNode->getChild(i));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void LoadEditorData(XmlNodeRef const pEditorDataNode, CAsset& library)
{
	int const numChildren = pEditorDataNode->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const pChild = pEditorDataNode->getChild(i);

		if (pChild->isTag(g_szLibraryNodeTag))
		{
			LoadLibraryEditorData(pChild, library);
		}
		else if (pChild->isTag(g_szFoldersNodeTag))
		{
			LoadAllFolders(pChild, library);
		}
		else if (pChild->isTag(g_szControlsNodeTag))
		{
			LoadAllControlsEditorData(pChild);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFileLoader::LoadAll()
{
	LoadScopes();
	LoadControls();
}

//////////////////////////////////////////////////////////////////////////
void CFileLoader::LoadControls()
{
	// load the global controls
	LoadAllLibrariesInFolder(g_assetsManager.GetConfigFolderPath(), "");

	// load the level specific controls
	_finddata_t fd;
	ICryPak* pCryPak = gEnv->pCryPak;
	intptr_t handle = pCryPak->FindFirst(g_assetsManager.GetConfigFolderPath() + CryAudio::g_szLevelsFolderName + "/*.*", &fd);

	if (handle != -1)
	{
		do
		{
			if (fd.attrib & _A_SUBDIR)
			{
				string name = fd.name;

				if ((name != ".") && (name != ".."))
				{
					LoadAllLibrariesInFolder(g_assetsManager.GetConfigFolderPath(), name);

					if (!g_assetsManager.ScopeExists(fd.name))
					{
						// if the control doesn't exist it
						// means it is not a real level in the
						// project so it is flagged as LocalOnly
						g_assetsManager.AddScope(fd.name, true);
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}

	CreateDefaultControls();
}

//////////////////////////////////////////////////////////////////////////
void CFileLoader::LoadAllLibrariesInFolder(string const& folderPath, string const& level)
{
	string path = folderPath;

	if (!level.empty())
	{
		path = path + CryAudio::g_szLevelsFolderName + "/" + level + "/";
	}

	string const searchPath = path + "*.xml";
	ICryPak* const pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	intptr_t const handle = pCryPak->FindFirst(searchPath.c_str(), &fd);

	if (handle != -1)
	{
		do
		{
			string fileName = path + fd.name;

			if (_stricmp(PathUtil::GetExt(fileName), "xml") == 0)
			{
				XmlNodeRef const root = GetISystem()->LoadXmlFromFile(fileName);

				if (root != nullptr)
				{
					if (_stricmp(root->getTag(), CryAudio::g_szRootNodeTag) == 0)
					{
						m_loadedFilenames.insert(fileName.MakeLower());
						string file = fd.name;

						if (root->haveAttr(CryAudio::g_szNameAttribute))
						{
							file = root->getAttr(CryAudio::g_szNameAttribute);
						}

						int version = 1;
						root->getAttr(CryAudio::g_szVersionAttribute, version);
						PathUtil::RemoveExtension(file);
						LoadControlsLibrary(root, fileName, level, file, static_cast<uint8>(version));
					}
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] Failed parsing audio system data file %s", fileName);
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFileLoader::LoadControlsLibrary(XmlNodeRef const pRoot, string const& filepath, string const& level, string const& filename, uint8 const version)
{
	// Always create a library file, even if no proper formatting is present.
	CLibrary* const pLibrary = g_assetsManager.CreateLibrary(filename);

	if (pLibrary != nullptr)
	{
		pLibrary->SetPakStatus(EPakStatus::InPak, gEnv->pCryPak->IsFileExist(filepath.c_str(), ICryPak::eFileLocation_InPak));
		pLibrary->SetPakStatus(EPakStatus::OnDisk, gEnv->pCryPak->IsFileExist(filepath.c_str(), ICryPak::eFileLocation_OnDisk));

		int const controlTypeCount = pRoot->getChildCount();

		for (int i = 0; i < controlTypeCount; ++i)
		{
			XmlNodeRef const pNode = pRoot->getChild(i);

			if (pNode != nullptr)
			{
				if (pNode->isTag(CryAudio::g_szEditorDataTag))
				{
					LoadEditorData(pNode, *pLibrary);
				}
				else
				{
					Scope const scope = level.empty() ? g_globalScopeId : g_assetsManager.GetScope(level);
					int const numControls = pNode->getChildCount();

					for (int j = 0; j < numControls; ++j)
					{
						LoadControl(pNode->getChild(j), scope, pLibrary);
					}
				}
			}
		}

		if (version < g_currentFileVersion)
		{
			pLibrary->SetModified(true, true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CControl* CFileLoader::LoadControl(XmlNodeRef const pNode, Scope const scope, CAsset* const pParentItem)
{
	CControl* pControl = nullptr;

	if (pNode != nullptr)
	{
		CAsset* const pFolderItem = AddUniqueFolderPath(pParentItem, QtUtil::ToQString(pNode->getAttr(g_szPathAttribute)));

		if (pFolderItem != nullptr)
		{
			string const name = pNode->getAttr(CryAudio::g_szNameAttribute);
			EAssetType const controlType = TagToType(pNode->getTag());

			// Don't load deprecated default controls.
			if (((controlType == EAssetType::Parameter) && ((name.compareNoCase("absolute_velocity") == 0) || (name.compareNoCase("relative_velocity") == 0))))
			{
				pParentItem->SetModified(true, true);
			}
			else
			{
				pControl = g_assetsManager.CreateControl(name, controlType, pFolderItem);
			}

			if (pControl != nullptr)
			{
				switch (controlType)
				{
				case EAssetType::Switch:
					{
						int const stateCount = pNode->getChildCount();

						for (int i = 0; i < stateCount; ++i)
						{
							LoadControl(pNode->getChild(i), scope, pControl);
						}
					}
					break;
				case EAssetType::Preload:
				case EAssetType::Setting:
					LoadPlatformSpecificConnections(pNode, pControl);
					break;
				default:
					LoadConnections(pNode, pControl);
					break;
				}

				pControl->SetScope(scope);
			}
		}
	}

	return pControl;
}

//////////////////////////////////////////////////////////////////////////
void CFileLoader::LoadScopes()
{
	ILevelSystem* const pLevelSystem = gEnv->pGameFramework->GetILevelSystem();

	if (pLevelSystem != nullptr)
	{
		int const levelCount = pLevelSystem->GetLevelCount();

		for (int i = 0; i < levelCount; ++i)
		{
			string const& name = pLevelSystem->GetLevelInfo(i)->GetName();
			g_assetsManager.AddScope(name);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFileLoader::CreateInternalControls()
{
	// Create internal default controls.
	// These controls are hidden in the ACE and don't get written to XML!
	CAsset* const pLibrary = static_cast<CAsset*>(g_assetsManager.CreateLibrary(CryAudio::g_szDefaultLibraryName));

	CRY_ASSERT_MESSAGE(pLibrary != nullptr, "Default library could not get created during %s", __FUNCTION__);

	if (pLibrary != nullptr)
	{
		pLibrary->SetDescription("Contains all engine default controls.");
		pLibrary->SetFlags(pLibrary->GetFlags() | EAssetFlags::IsDefaultControl);

		EAssetFlags const flags = (EAssetFlags::IsDefaultControl | EAssetFlags::IsInternalControl);
		g_assetsManager.CreateDefaultControl("do_nothing", EAssetType::Trigger, pLibrary, flags, "Used to bypass the default stop behavior of the audio system.");
	}
}

//////////////////////////////////////////////////////////////////////////
void CFileLoader::LoadPlatformSpecificConnections(XmlNodeRef const pNode, CControl* const pControl)
{
	if (_stricmp(pNode->getAttr(CryAudio::g_szTypeAttribute), CryAudio::g_szDataLoadType) == 0)
	{
		pControl->SetAutoLoad(true);
	}
	else
	{
		pControl->SetAutoLoad(false);
	}

	int const numChildren = pNode->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		// Skip unused data from previous format
		XmlNodeRef const pPlatformNode = pNode->getChild(i);

		if (_stricmp(pPlatformNode->getTag(), CryAudio::g_szPlatformTag) == 0)
		{
			// Get the index for that platform name
			int platformIndex = -1;
			bool foundPlatform = false;
			char const* const szPlatformName = pPlatformNode->getAttr(CryAudio::g_szNameAttribute);

			for (auto const szPlatform : g_platforms)
			{
				++platformIndex;

				if (_stricmp(szPlatformName, szPlatform) == 0)
				{
					foundPlatform = true;
					break;
				}
			}

			if (!foundPlatform)
			{
				m_errorCodeMask |= EErrorCode::UnkownPlatform;
				pControl->SetModified(true, true);
			}

			int const numConnections = pPlatformNode->getChildCount();

			for (int j = 0; j < numConnections; ++j)
			{
				XmlNodeRef const pConnectionNode = pPlatformNode->getChild(j);

				if (pConnectionNode != nullptr)
				{
					pControl->LoadConnectionFromXML(pConnectionNode, platformIndex);
				}
			}
		}
	}
}
} // namespace ACE
