// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProjectLoader.h"

#include "Common.h"
#include "Impl.h"
#include "Utils.h"

#include <CryAudioImplFmod/GlobalData.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/XML/IXml.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
// Paths
constexpr char const* g_szEventFoldersPath = "/metadata/eventfolder/";
constexpr char const* g_szParametersFoldersPath = "/metadata/parameterpresetfolder/";
constexpr char const* g_szSnapshotGroupsPath = "/metadata/snapshotgroup/";
constexpr char const* g_szMixerGroupsPath = "/metadata/group/";
constexpr char const* g_szEventsPath = "/metadata/event/";
constexpr char const* g_szParametersPath = "/metadata/parameterpreset/";
constexpr char const* g_szSnapshotsPath = "/metadata/snapshot/";
constexpr char const* g_szReturnsPath = "/metadata/return/";
constexpr char const* g_szVcasPath = "/metadata/vca/";
constexpr char const* g_szBankPath = "/metadata/bank/";

using ItemNames = std::vector<string>;

//////////////////////////////////////////////////////////////////////////
void AddNonStreamsBank(ItemNames& banks, string const& fileName)
{
	size_t const pos = fileName.rfind(".streams.bank");

	if (pos == string::npos)
	{
		banks.emplace_back(fileName);
	}
}

//////////////////////////////////////////////////////////////////////////
CProjectLoader::CProjectLoader(
	string const& projectPath,
	string const& banksPath,
	string const& localizedBanksPath,
	CItem& rootItem,
	ItemCache& itemCache,
	CImpl const& impl)
	: m_rootItem(rootItem)
	, m_itemCache(itemCache)
	, m_projectPath(projectPath)
	, m_impl(impl)
{
	CItem* const pSoundBanksFolder = CreateItem(s_soundBanksFolderName, EItemType::EditorFolder, &m_rootItem, EItemFlags::IsContainer);
	LoadBanks(banksPath, false, *pSoundBanksFolder);
	LoadBanks(localizedBanksPath, true, *pSoundBanksFolder);

	CItem* const pEventsFolder = CreateItem(s_eventsFolderName, EItemType::EditorFolder, &m_rootItem, EItemFlags::IsContainer);
	CItem* const pParametersFolder = CreateItem(s_parametersFolderName, EItemType::EditorFolder, &m_rootItem, EItemFlags::IsContainer);
	CItem* const pSnapshotsFolder = CreateItem(s_snapshotsFolderName, EItemType::EditorFolder, &m_rootItem, EItemFlags::IsContainer);
	CItem* const pReturnsFolder = CreateItem(s_returnsFolderName, EItemType::EditorFolder, &m_rootItem, EItemFlags::IsContainer);
	CItem* const pVcasFolder = CreateItem(s_vcasFolderName, EItemType::EditorFolder, &m_rootItem, EItemFlags::IsContainer);

	ParseFolder(projectPath + g_szEventFoldersPath, *pEventsFolder, rootItem);          // Event folders
	ParseFolder(projectPath + g_szParametersFoldersPath, *pParametersFolder, rootItem); // Parameter folders
	ParseFolder(projectPath + g_szSnapshotGroupsPath, *pSnapshotsFolder, rootItem);     // Snapshot groups
	ParseFolder(projectPath + g_szMixerGroupsPath, *pReturnsFolder, rootItem);          // Mixer groups
	ParseFolder(projectPath + g_szEventsPath, *pEventsFolder, rootItem);                // Events
	ParseFolder(projectPath + g_szParametersPath, *pParametersFolder, rootItem);        // Parameters
	ParseFolder(projectPath + g_szSnapshotsPath, *pSnapshotsFolder, rootItem);          // Snapshots
	ParseFolder(projectPath + g_szReturnsPath, *pReturnsFolder, rootItem);              // Returns
	ParseFolder(projectPath + g_szVcasPath, *pVcasFolder, rootItem);                    // VCAs
	ParseFolder(projectPath + g_szBankPath, rootItem, rootItem);                        // Audio tables of banks

	if (!m_audioTableInfos.empty())
	{
		CItem* const pKeysFolder = CreateItem(s_keysFolderName, EItemType::EditorFolder, &m_rootItem, EItemFlags::IsContainer);
		LoadKeys(*pKeysFolder);
		RemoveEmptyEditorFolders(pKeysFolder);
	}

	RemoveEmptyMixerGroups();

	RemoveEmptyEditorFolders(pSoundBanksFolder);
	RemoveEmptyEditorFolders(pEventsFolder);
	RemoveEmptyEditorFolders(pParametersFolder);
	RemoveEmptyEditorFolders(pSnapshotsFolder);
	RemoveEmptyEditorFolders(pReturnsFolder);
	RemoveEmptyEditorFolders(pVcasFolder);
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadBanks(string const& folderPath, bool const isLocalized, CItem& parent)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(folderPath + "/*.bank", &fd);

	if (handle != -1)
	{
		EItemFlags const flags = isLocalized ? EItemFlags::IsLocalized : EItemFlags::None;

		// We have to exclude the Master Bank, for this we look
		// for the file that ends with "strings.bank" as it is guaranteed
		// to have the same name as the Master Bank and there should be unique.
		ItemNames banks;
		ItemNames masterBankNames;

		do
		{
			string const fileName = fd.name;

			if ((fileName != ".") && (fileName != "..") && !fileName.empty())
			{
				if (isLocalized)
				{
					AddNonStreamsBank(banks, fileName);
				}
				else
				{
					size_t const pos = fileName.rfind(".strings.bank");

					if (pos != string::npos)
					{
						masterBankNames.emplace_back(fileName.substr(0, pos));
					}
					else
					{
						AddNonStreamsBank(banks, fileName);
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		for (string const& bankName : banks)
		{
			bool canCreateItem = isLocalized;

			if (!canCreateItem)
			{
				canCreateItem = true;

				for (auto const& masterBankName : masterBankNames)
				{
					if ((bankName.compareNoCase(0, masterBankName.length(), masterBankName) == 0))
					{
						canCreateItem = false;
						break;
					}
				}
			}

			if (canCreateItem)
			{
				string const filePath = folderPath + "/" + bankName;
				EPakStatus const pakStatus = pCryPak->IsFileExist(filePath.c_str(), ICryPak::eFileLocation_OnDisk) ? EPakStatus::OnDisk : EPakStatus::None;

				CreateItem(bankName, EItemType::Bank, &parent, flags, pakStatus, filePath);
			}
		}

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::ParseFolder(string const& folderPath, CItem& editorFolder, CItem& parent)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(folderPath + "*.xml", &fd);

	if (handle != -1)
	{
		do
		{
			string const filename = fd.name;

			if ((filename != ".") && (filename != "..") && !filename.empty())
			{
				string const filePath = folderPath + filename;
				EPakStatus const pakStatus = pCryPak->IsFileExist(filePath.c_str(), ICryPak::eFileLocation_OnDisk) ? EPakStatus::OnDisk : EPakStatus::None;

				ParseFile(filePath, editorFolder, pakStatus);
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::ParseFile(string const& filepath, CItem& parent, EPakStatus const pakStatus)
{
	if (GetISystem()->GetIPak()->IsFileExist(filepath))
	{
		XmlNodeRef const rootNode = GetISystem()->LoadXmlFromFile(filepath);

		if (rootNode.isValid())
		{
			CItem* pItem = nullptr;
			int const numChildren = rootNode->getChildCount();

			for (int i = 0; i < numChildren; ++i)
			{
				XmlNodeRef const childNode = rootNode->getChild(i);

				if (childNode.isValid())
				{
					char const* const szClassName = childNode->getAttr("class");

					if ((_stricmp(szClassName, "EventFolder") == 0) || (_stricmp(szClassName, "ParameterPresetFolder") == 0))
					{
						LoadFolder(childNode, parent, pakStatus);
					}
					else if (_stricmp(szClassName, "SnapshotGroup") == 0)
					{
						LoadSnapshotGroup(childNode, parent, pakStatus);
					}
					else if (_stricmp(szClassName, "Event") == 0)
					{
						pItem = LoadEvent(childNode, parent, pakStatus);
					}
					else if (_stricmp(szClassName, "Snapshot") == 0)
					{
						LoadSnapshot(childNode, parent, pakStatus);
					}
					else if (_stricmp(szClassName, "ParameterPreset") == 0)
					{
						LoadParameter(childNode, parent, pakStatus);
					}
					else if (_stricmp(szClassName, "MixerReturn") == 0)
					{
						LoadReturn(childNode, parent, pakStatus);
					}
					else if (_stricmp(szClassName, "MixerGroup") == 0)
					{
						LoadMixerGroup(childNode, parent, pakStatus);
					}
					else if (_stricmp(szClassName, "MixerVCA") == 0)
					{
						LoadVca(childNode, parent, pakStatus);
					}
					else if (_stricmp(szClassName, "AudioTable") == 0)
					{
						LoadAudioTable(childNode);
					}
					else if (_stricmp(szClassName, "ProgrammerSound") == 0)
					{
						if ((pItem != nullptr) && (pItem->GetType() == EItemType::Event))
						{
							string const pathName = Utils::GetPathName(pItem, m_rootItem);
							CImpl::s_programmerSoundEvents.emplace_back(pathName);
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::ParseKeysFile(string const& filePath, CItem& parent)
{
	CCryFile file;

	if (file.Open(filePath, "r"))
	{
		size_t const length = file.GetLength();
		string allText;

		if (length > 0)
		{
			std::vector<char> buffer;
			buffer.resize(length, '\n');
			file.ReadRaw(&buffer[0], length);
			allText.assign(&buffer[0], length);

			int linePos = 0;
			string line;

			while (!(line = allText.Tokenize("\r\n", linePos)).empty())
			{
				int keyPos = 0;
				string const key = line.Tokenize(",", keyPos).Trim();
				CreateItem(key, EItemType::Key, &parent, EItemFlags::None);
			}
		}

		file.Close();
	}
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::GetContainer(string const& id, EItemType const type, CItem& parent, EPakStatus const pakStatus)
{
	CItem* pItem = &parent;
	auto folder = m_containerIds.find(id);

	if (folder != m_containerIds.end())
	{
		pItem = (*folder).second;
	}
	else
	{
		// If folder not found parse the file corresponding to it and try looking for it again.
		switch (type)
		{
		case EItemType::Folder:
			{
				ParseFile(m_projectPath + g_szEventFoldersPath + id + ".xml", parent, pakStatus);
				ParseFile(m_projectPath + g_szParametersFoldersPath + id + ".xml", parent, pakStatus);
				break;
			}
		case EItemType::MixerGroup:
			{
				ParseFile(m_projectPath + g_szMixerGroupsPath + id + ".xml", parent, pakStatus);
				break;
			}
		default:
			{
				break;
			}
		}

		folder = m_containerIds.find(id);

		if (folder != m_containerIds.end())
		{
			pItem = (*folder).second;
		}
	}

	return pItem;
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadContainer(XmlNodeRef const& node, EItemType const type, string const& relationshipParamName, CItem& parent, EPakStatus const pakStatus)
{
	string name = "";
	CItem* pParent = &parent;
	int const numChildren = node->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const childNode = node->getChild(i);
		string const attribName = childNode->getAttr("name");

		if (attribName.compareNoCase("name") == 0)
		{
			// Get the container name.
			XmlNodeRef const nameNode = childNode->getChild(0);

			if (nameNode.isValid())
			{
				name = nameNode->getContent();
			}
		}
		else if (attribName.compareNoCase(relationshipParamName) == 0)
		{
			// Get the container parent.
			XmlNodeRef const parentContainerNode = childNode->getChild(0);

			if (parentContainerNode.isValid())
			{
				string const parentContainerId = parentContainerNode->getContent();
				pParent = GetContainer(parentContainerId, type, parent, pakStatus);
			}
		}
	}

	CItem* const pContainer = CreateItem(name, type, pParent, EItemFlags::IsContainer, pakStatus);
	m_containerIds[node->getAttr("id")] = pContainer;
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadSnapshotGroup(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	string name = "";
	ItemNames snapshotsItems;
	int const numChildren = node->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const childNode = node->getChild(i);
		char const* const szAttribName = childNode->getAttr("name");

		if (_stricmp(szAttribName, "name") == 0)
		{
			XmlNodeRef const nameNode = childNode->getChild(0);

			if (nameNode.isValid())
			{
				name = nameNode->getContent();
			}
		}
		else if (_stricmp(szAttribName, "items") == 0)
		{
			int const itemCount = childNode->getChildCount();

			for (int j = 0; j < itemCount; ++j)
			{
				XmlNodeRef const itemNode = childNode->getChild(j);

				if (itemNode.isValid())
				{
					snapshotsItems.emplace_back(itemNode->getContent());
				}
			}
		}
	}

	CItem* const pItem = CreateItem(name, EItemType::Folder, &parent, EItemFlags::IsContainer, pakStatus);

	if (!snapshotsItems.empty())
	{
		for (auto const& snapshotId : snapshotsItems)
		{
			m_snapshotGroupItems[snapshotId] = pItem;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadFolder(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	LoadContainer(node, EItemType::Folder, "folder", parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadMixerGroup(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	LoadContainer(node, EItemType::MixerGroup, "output", parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::LoadItem(XmlNodeRef const& node, EItemType const type, CItem& parent, EPakStatus const pakStatus)
{
	CItem* pItem = nullptr;
	string itemName = "";
	CItem* pParent = &parent;
	int const numChildren = node->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const childNode = node->getChild(i);

		if (childNode.isValid())
		{
			char const* const szTag = childNode->getTag();

			if (_stricmp(szTag, "property") == 0)
			{
				string const paramName = childNode->getAttr("name");

				if (paramName == "name")
				{
					XmlNodeRef const valueNode = childNode->getChild(0);

					if (valueNode.isValid())
					{
						itemName = valueNode->getContent();
					}
				}
			}
			else if (_stricmp(szTag, "relationship") == 0)
			{
				char const* const relationshipName = childNode->getAttr("name");

				if ((_stricmp(relationshipName, "folder") == 0) || (_stricmp(relationshipName, "output") == 0))
				{
					XmlNodeRef const valueNode = childNode->getChild(0);

					if (valueNode.isValid())
					{
						string const parentContainerId = valueNode->getContent();
						auto const folder = m_containerIds.find(parentContainerId);

						if (folder != m_containerIds.end())
						{
							pParent = (*folder).second;
						}
					}
				}
			}
		}
	}

	if (type == EItemType::Snapshot)
	{
		string const id = node->getAttr("id");

		auto const snapshotGroupPair = m_snapshotGroupItems.find(id);

		if (snapshotGroupPair != m_snapshotGroupItems.end())
		{
			pParent = (*snapshotGroupPair).second;
		}
	}

	pItem = CreateItem(itemName, type, pParent, EItemFlags::None, pakStatus);

	return pItem;
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::LoadEvent(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	return LoadItem(node, EItemType::Event, parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadSnapshot(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	LoadItem(node, EItemType::Snapshot, parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadReturn(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	CItem* const pReturn = LoadItem(node, EItemType::Return, parent, pakStatus);

	if (pReturn != nullptr)
	{
		auto pParent = static_cast<CItem*>(pReturn->GetParent());

		while ((pParent != nullptr) && (pParent->GetType() == EItemType::MixerGroup))
		{
			m_emptyMixerGroups.erase(std::remove(m_emptyMixerGroups.begin(), m_emptyMixerGroups.end(), pParent), m_emptyMixerGroups.end());
			pParent = static_cast<CItem*>(pParent->GetParent());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadParameter(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	LoadItem(node, EItemType::Parameter, parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadVca(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	LoadItem(node, EItemType::VCA, parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadAudioTable(XmlNodeRef const& node)
{
	string name = "";
	bool isLocalized = false;
	bool includeSubdirs = false;

	int const numChildren = node->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const childNode = node->getChild(i);

		if (childNode.isValid())
		{
			char const* const szTag = childNode->getTag();

			if (_stricmp(szTag, "property") == 0)
			{
				string const paramName = childNode->getAttr("name");

				if (paramName == "sourceDirectory")
				{
					XmlNodeRef const valueNode = childNode->getChild(0);

					if (valueNode.isValid())
					{
						name = m_projectPath + "/" + valueNode->getContent();
					}
				}
				else if (paramName == "isLocalized")
				{
					XmlNodeRef const valueNode = childNode->getChild(0);

					if ((valueNode.isValid()) && (_stricmp(valueNode->getContent(), "true") == 0))
					{
						isLocalized = true;
					}
				}
				else if (paramName == "includeSubDirectories")
				{
					XmlNodeRef const valueNode = childNode->getChild(0);

					if ((valueNode.isValid()) && (_stricmp(valueNode->getContent(), "true") == 0))
					{
						includeSubdirs = true;
					}
				}
			}
		}
	}

	if (!name.empty())
	{
		m_audioTableInfos.emplace(SAudioTableInfo(name, isLocalized, includeSubdirs));
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadKeys(CItem& parent)
{
	for (auto const& tableInfo : m_audioTableInfos)
	{
		string const keysFilePath = tableInfo.isLocalized ? (tableInfo.name + "/" + g_language + "/keys.txt") : (tableInfo.name + "/keys.txt");
		ICryPak* const pCryPak = gEnv->pCryPak;

		if (pCryPak->IsFileExist(keysFilePath.c_str()))
		{
			ParseKeysFile(keysFilePath, parent);
		}
		else
		{
			string const filePath = tableInfo.isLocalized ? (tableInfo.name + "/" + g_language) : (tableInfo.name);
			LoadKeysFromFiles(parent, filePath);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadKeysFromFiles(CItem& parent, string const& filePath)
{
	ICryPak* const pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	intptr_t const handle = pCryPak->FindFirst(filePath + "/*.*", &fd);

	if (handle != -1)
	{
		do
		{
			if (fd.attrib & _A_SUBDIR)
			{
				string const subFolderName = fd.name;

				if ((subFolderName != ".") && (subFolderName != ".."))
				{
					CItem* const pFolderItem = CreateItem(subFolderName, EItemType::Folder, &parent, EItemFlags::IsContainer);
					string const subFolderPath = filePath + "/" + fd.name;

					LoadKeysFromFiles(*pFolderItem, subFolderPath);
				}
			}
			else
			{
				string const fileName = fd.name;

				if ((fileName != ".") && (fileName != "..") && !fileName.empty())
				{
					string::size_type const posExtension = fileName.rfind('.');

					if (posExtension != string::npos)
					{
						string const fileExtension = fileName.data() + posExtension;

						if ((_stricmp(fileExtension, ".mp3") == 0) ||
						    (_stricmp(fileExtension, ".ogg") == 0) ||
						    (_stricmp(fileExtension, ".wav") == 0) ||
						    (_stricmp(fileExtension, ".mp2") == 0) ||
						    (_stricmp(fileExtension, ".flac") == 0) ||
						    (_stricmp(fileExtension, ".aiff") == 0))
						{
							CreateItem(PathUtil::RemoveExtension(fileName), EItemType::Key, &parent, EItemFlags::None);
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
CItem* CProjectLoader::CreateItem(
	string const& name,
	EItemType const type,
	CItem* const pParent,
	EItemFlags const flags,
	EPakStatus const pakStatus /*= EPakStatus::None*/,
	string const& filePath /*= ""*/)
{
	ControlId const id = Utils::GetId(type, name, pParent, m_rootItem);
	auto pItem = static_cast<CItem*>(m_impl.GetItem(id));

	if (pItem == nullptr)
	{
		pItem = new CItem(name, id, type, flags, pakStatus, filePath);

		if (pParent != nullptr)
		{
			pParent->AddChild(pItem);
		}
		else
		{
			m_rootItem.AddChild(pItem);
		}

		if (type == EItemType::Event)
		{
			pItem->SetPathName(Utils::GetPathName(pItem, m_rootItem));
		}
		else if (type == EItemType::MixerGroup)
		{
			m_emptyMixerGroups.push_back(pItem);
		}

		m_itemCache[id] = pItem;
	}

	return pItem;
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::RemoveEmptyMixerGroups()
{
	auto iter = m_emptyMixerGroups.begin();
	auto iterEnd = m_emptyMixerGroups.end();

	while (iter != iterEnd)
	{
		auto const pMixerGroup = *iter;

		if (pMixerGroup != nullptr)
		{
			auto const pParent = static_cast<CItem* const>(pMixerGroup->GetParent());

			if (pParent != nullptr)
			{
				pParent->RemoveChild(pMixerGroup);
			}

			size_t const numChildren = pMixerGroup->GetNumChildren();

			for (size_t i = 0; i < numChildren; ++i)
			{
				auto const pChild = static_cast<CItem* const>(pMixerGroup->GetChildAt(i));

				if (pChild != nullptr)
				{
					pMixerGroup->RemoveChild(pChild);
				}
			}

			auto const id = pMixerGroup->GetId();
			auto const cacheIter = m_itemCache.find(id);

			if (cacheIter != m_itemCache.end())
			{
				m_itemCache.erase(cacheIter);
			}

			delete pMixerGroup;
		}

		if (iter != (iterEnd - 1))
		{
			(*iter) = m_emptyMixerGroups.back();
		}

		m_emptyMixerGroups.pop_back();
		iter = m_emptyMixerGroups.begin();
		iterEnd = m_emptyMixerGroups.end();
	}

	m_emptyMixerGroups.clear();
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::RemoveEmptyEditorFolders(CItem* const pEditorFolder)
{
	if (pEditorFolder->GetNumChildren() == 0)
	{
		m_rootItem.RemoveChild(pEditorFolder);
		ItemCache::const_iterator const it(m_itemCache.find(pEditorFolder->GetId()));

		if (it != m_itemCache.end())
		{
			m_itemCache.erase(it);
		}

		delete pEditorFolder;
	}
}
} // namespace Fmod
} // namespace Impl
} // namespace ACE
