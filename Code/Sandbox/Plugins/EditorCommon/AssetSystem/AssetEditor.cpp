// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "AssetEditor.h"

#include "AssetFilesGroupProvider.h"
#include "AssetManager.h"
#include "AssetType.h"
#include "Browser/AssetBrowserDialog.h"
#include "Controls/QuestionDialog.h"
#include "Controls/SingleSelectionDialog.h"
#include "CryExtension/CryGUID.h"
#include "DragDrop.h"
#include "EditableAsset.h"
#include "EditorFramework/Events.h"
#include "FileOperationsExecutor.h"
#include "FileUtils.h"
#include "Loader/AssetLoaderHelpers.h"
#include "PathUtils.h"
#include "QtUtil.h"
#include "ThreadingUtils.h"

#include <IEditor.h>

#include <QCloseEvent>
#include <QToolButton>

namespace Private_AssetEditor
{

//! Makes a temporary copy of files in construct time
//!	and moves them back in the destructor.
class CAutoFileRecovery
{
public:
	CAutoFileRecovery(const std::vector<string>& files)
	{
		if (files.empty())
		{
			return;
		}

		static const string tempPrefix = GetTemporaryDirectoryPath();

		m_files.reserve(files.size());

		ICryPak* const pCryPak = GetISystem()->GetIPak();
		pCryPak->MakeDir(tempPrefix.c_str());
		for (const string& file : files)
		{
			const string tempFilemane = PathUtil::Make(tempPrefix, CryGUID::Create().ToString(), "tmp");
			if (!pCryPak->CopyFileOnDisk(file.c_str(), tempFilemane.c_str(), false))
			{
				break;
			}

			m_files.emplace_back(file, tempFilemane);
		}

		if (files.size() != m_files.size())
		{
			Discard();
		}
	}

	bool IsValid() const { return !m_files.empty() || !m_files.capacity(); }

	void Discard()
	{
		ICryPak* const pCryPak = GetISystem()->GetIPak();
		for (const auto& file : m_files)
		{
			pCryPak->RemoveFile(file.second.c_str());
		}
		m_files.clear();
	}

	virtual ~CAutoFileRecovery()
	{
		ICryPak* const pCryPak = GetISystem()->GetIPak();
		for (const auto& file : m_files)
		{
			pCryPak->CopyFileOnDisk(file.second.c_str(), file.first.c_str(), false);
			pCryPak->RemoveFile(file.second.c_str());
		}
	}
private:
	static string GetTemporaryDirectoryPath()
	{
		char path[ICryPak::g_nMaxPath] = {};
		return GetISystem()->GetIPak()->AdjustFileName("%USER%/temp", path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING | ICryPak::FLAGS_ADD_TRAILING_SLASH);
	}

private:
	std::vector<std::pair<string, string>> m_files;
};

class CAutoAssetRecovery : public CAutoFileRecovery
{
public:
	CAutoAssetRecovery(const CAsset& asset)
		: CAutoFileRecovery(GetAssetFiles(asset, false))
	{
	}

	static std::vector<string> GetAssetFiles(const CAsset& asset, bool bIncludeSourceFile)
	{
		std::vector<string> files = asset.GetType()->GetAssetFiles(asset, bIncludeSourceFile, true);

		files.erase(std::remove_if(files.begin(), files.end(), [](const string& filename)
		{
			return !GetISystem()->GetIPak()->IsFileExist(filename.c_str(), ICryPak::eFileLocation_OnDisk);
		}), files.end());

		return files;
	}
};

}

CAssetEditor* CAssetEditor::OpenAssetForEdit(const char* szEditorClassName, CAsset* pAsset)
{
	CRY_ASSERT(pAsset);
	IPane* pPane = GetIEditor()->CreateDockable(szEditorClassName);
	if (pPane)
	{
		CAssetEditor* assetEditor = static_cast<CAssetEditor*>(pPane);
		if (assetEditor->OpenAsset(pAsset))
		{
			return assetEditor;
		}
	}
	return nullptr;
}

CAssetEditor::CAssetEditor(const char* assetType, QWidget* pParent /*= nullptr*/)
	: CDockableEditor(pParent)
	, m_assetBeingEdited(nullptr)
{
	auto type = CAssetManager::GetInstance()->FindAssetType(assetType);
	CRY_ASSERT(type);//type must exist
	m_supportedAssetTypes.push_back(type);

	m_pLockAction = GetIEditor()->GetICommandManager()->CreateNewAction("asset.toggle_instant_editing");

	Init();
}

CAssetEditor::CAssetEditor(const QStringList& assetTypes, QWidget* pParent /*= nullptr*/)
	: CDockableEditor(pParent)
	, m_assetBeingEdited(nullptr)
{
	m_supportedAssetTypes.reserve(assetTypes.size());
	for (const QString& typeName : assetTypes)
	{
		auto type = CAssetManager::GetInstance()->FindAssetType(typeName.toStdString().c_str());
		CRY_ASSERT(type);//type must exist
		m_supportedAssetTypes.push_back(type);
	}

	m_pLockAction = GetIEditor()->GetICommandManager()->CreateNewAction("asset.toggle_instant_editing");

	Init();
}

void CAssetEditor::Init()
{
	InitGenericMenu();

	setAcceptDrops(true);
}

bool CAssetEditor::OpenAsset(CAsset* pAsset)
{
	//An asset can only be opened once in one editor
	if (pAsset->IsBeingEdited())
		return false;

	if (pAsset == m_assetBeingEdited)
		return true;

	if (!Close())
	{
		// User cancelled closing of currently opened asset.
		return false;
	}

	if (!OnOpenAsset(pAsset))
	{
		return false;
	}

	AddRecentFile(QString(pAsset->GetMetadataFile()));
	SetAssetBeingEdited(pAsset);

	CEditableAsset editableAsset(*pAsset);
	editableAsset.SetOpenedInAssetEditor(this);

	return true;
}

bool CAssetEditor::CanOpenAsset(CAsset* pAsset)
{
	return pAsset && CanOpenAsset(pAsset->GetType());
}

bool CAssetEditor::CanOpenAsset(const CAssetType* pType)
{
	if (!pType)
		return false;

	return std::find(m_supportedAssetTypes.begin(), m_supportedAssetTypes.end(), pType) != m_supportedAssetTypes.end();
}

void CAssetEditor::InitGenericMenu()
{
	AddToMenu(CEditor::MenuItems::FileMenu);

	AddToMenu(CEditor::MenuItems::Open);
	AddToMenu(CEditor::MenuItems::Close);
	AddToMenu(CEditor::MenuItems::Save);
	AddToMenu(CEditor::MenuItems::RecentFiles);

	//TODO: help menu doesn't always occupy last position
	AddToMenu(CEditor::MenuItems::HelpMenu);
	AddToMenu(CEditor::MenuItems::Help);

	InitNewMenu();
}

int CAssetEditor::GetNewableAssetCount() const
{
	int newableAssets = 0;
	for (const auto& type : m_supportedAssetTypes)
	{
		newableAssets += type->CanBeCreated();
	}
	return newableAssets;
}

void CAssetEditor::UpdateWindowTitle()
{
	if (m_assetBeingEdited)
	{
		if (m_assetBeingEdited->IsModified())
			setWindowTitle(QString(m_assetBeingEdited->GetName()) + " *");
		else
			setWindowTitle(m_assetBeingEdited->GetName().c_str());

		setWindowIcon(m_assetBeingEdited->GetType()->GetIcon());
	}
	else
	{
		setWindowTitle(GetPaneTitle());
		setWindowIcon(QIcon());//TODO : this should be the pane's default icon, panes already have an icon from the Tools menu
	}
}

void CAssetEditor::SetAssetBeingEdited(CAsset* pAsset)
{
	if (m_assetBeingEdited == pAsset)
		return;

	if (m_assetBeingEdited)
	{
		m_assetBeingEdited->signalChanged.DisconnectObject(this);
	}

	m_assetBeingEdited = pAsset;

	UpdateWindowTitle();

	if (pAsset)
	{
		CAssetManager::GetInstance()->signalBeforeAssetsRemoved.Connect([this](const std::vector<CAsset*>& assets)
		{
			if (std::find(assets.begin(), assets.end(), GetAssetBeingEdited()) != assets.end())
			{
			  OnCloseAsset();
			  CRY_ASSERT(GetAssetBeingEdited() != nullptr);
			  signalAssetClosed(GetAssetBeingEdited());
			  SetAssetBeingEdited(nullptr);
			}
		}, (uintptr_t)this);

		pAsset->signalChanged.Connect(this, &CAssetEditor::OnAssetChanged);
	}
	else
	{
		CAssetManager::GetInstance()->signalBeforeAssetsRemoved.DisconnectById((uintptr_t)this);
	}
}

bool CAssetEditor::OnAboutToCloseAssetInternal(string& reason) const
{
	reason.clear();

	if (!m_assetBeingEdited)
	{
		return true;
	}

	if (m_assetBeingEdited->GetEditingSession())
	{
		return true;
	}

	if (m_assetBeingEdited->IsModified())
	{
		reason = QtUtil::ToString(tr("Asset '%1' has unsaved modifications.").arg(m_assetBeingEdited->GetName().c_str()));
		return false;
	}

	return OnAboutToCloseAsset(reason);
}

bool CAssetEditor::TryCloseAsset()
{
	if (!m_assetBeingEdited)
	{
		return true;
	}

	string reason;
	bool bClose = true;
	if (!GetIEditor()->IsMainFrameClosing() && !OnAboutToCloseAssetInternal(reason))
	{
		if (reason.empty())
		{
			// Show generic modification message.
			reason = QtUtil::ToString(tr("Asset '%1' has unsaved modifications.").arg(m_assetBeingEdited->GetName().c_str()));
		}

		const QString title = tr("Closing %1").arg(GetEditorName());
		const auto button = CQuestionDialog::SQuestion(title, QtUtil::ToQString(reason), QDialogButtonBox::Save | QDialogButtonBox::Discard | QDialogButtonBox::Cancel, QDialogButtonBox::Cancel);
		switch (button)
		{
		case QDialogButtonBox::Save:
			OnSave();
			bClose = true;
			break;
		case QDialogButtonBox::Discard:
			DiscardAssetChanges();
			bClose = true;
			break;
		case QDialogButtonBox::No:
		// Fall-through.
		// "No" is returned when a user clicked the "x" in the window bar.
		case QDialogButtonBox::Cancel:
			bClose = false;
			break;
		default:
			CRY_ASSERT(0 && "Unknown button");
			bClose = false;
			break;
		}
	}

	if (bClose)
	{
		OnCloseAsset();
		CRY_ASSERT(GetAssetBeingEdited() != nullptr);
		signalAssetClosed(GetAssetBeingEdited());
		SetAssetBeingEdited(nullptr);
		return true;
	}
	else
	{
		return false;
	}
}

void CAssetEditor::OnAssetChanged(CAsset& asset, int changeFlags)
{
	CRY_ASSERT(&asset == m_assetBeingEdited);

	if (changeFlags & (eAssetChangeFlags_Modified | eAssetChangeFlags_Open))
	{
		UpdateWindowTitle();
	}
}

void CAssetEditor::InitNewMenu()
{
	const int newableAssets = GetNewableAssetCount();

	if (!newableAssets)
	{
		return;
	}
	else if (newableAssets == 1)
	{
		AddToMenu(MenuItems::New);
	}
	else
	{
		CAbstractMenu* const pSubMenu = GetMenu(MenuItems::FileMenu)->CreateMenu(tr("New"), 0, 0);
		for (const auto& type : m_supportedAssetTypes)
		{
			if (type->CanBeCreated())
			{
				QAction* const pAction = pSubMenu->CreateAction(tr("%1").arg(type->GetUiTypeName()));
				connect(pAction, &QAction::triggered, [this, type]()
				{
					InternalNewAsset(type);
				});
			}
		}
	}
}

bool CAssetEditor::OnNew()
{
	const int newableAssetCount = GetNewableAssetCount();
	if (newableAssetCount == 1)
	{
		InternalNewAsset(m_supportedAssetTypes[0]);
	}
	else if (newableAssetCount > 1)
	{
		std::vector<string> assetTypeNames;
		std::transform(m_supportedAssetTypes.begin(), m_supportedAssetTypes.end(), std::back_inserter(assetTypeNames), [](const auto& t)
		{
			return t->GetTypeName();
		});
		CSingleSelectionDialog assetTypeSelection;
		assetTypeSelection.setWindowTitle(tr("New asset type"));
		assetTypeSelection.SetOptions(assetTypeNames);
		if (assetTypeSelection.Execute())
		{
			InternalNewAsset(m_supportedAssetTypes[assetTypeSelection.GetSelectedIndex()]);
		}
	}
	return true;
}

void CAssetEditor::InternalNewAsset(CAssetType* pAssetType)
{
	if (!Close())
		return;

	const string assetTypeName = pAssetType->GetTypeName();

	const string assetBasePath = CAssetBrowserDialog::CreateSingleAssetForType(assetTypeName, CAssetBrowserDialog::OverwriteMode::NoOverwrite);
	if (assetBasePath.empty())
	{
		return; // Operation cancelled by user.
	}

	const string assetPath = assetBasePath + string().Format(".%s.cryasset", pAssetType->GetFileExtension());
	if (pAssetType->Create(assetPath))
	{
		CAsset* const pAsset = CAssetManager::GetInstance()->FindAssetForMetadata(assetPath);
		if (pAsset)
		{
			OpenAsset(pAsset);
		}
	}
}

bool CAssetEditor::OnOpen()
{
	std::vector<string> supportedAssetTypeNames;
	supportedAssetTypeNames.reserve(m_supportedAssetTypes.size());
	for (auto& assetType : m_supportedAssetTypes)
	{
		supportedAssetTypeNames.push_back(assetType->GetTypeName());
	}

	CAsset* const asset = CAssetBrowserDialog::OpenSingleAssetForTypes(supportedAssetTypeNames);
	if (asset)
	{
		OpenAsset(asset);
	}
	return true;
}

bool CAssetEditor::OnOpenFile(const QString& path)
{
	auto asset = CAssetManager::GetInstance()->FindAssetForFile(path.toStdString().c_str());
	if (asset)
	{
		OpenAsset(asset);
	}
	return true;
}

bool CAssetEditor::Close()
{
	if (!GetAssetBeingEdited())
	{
		return true;
	}

	return TryCloseAsset();
}

bool CAssetEditor::OnClose()
{
	//Note: this is only the callback for menu action, every other place should call close()
	Close();
	return true;//returns true because the menu action is handled
}

bool CAssetEditor::CanQuit(std::vector<string>& unsavedChanges)
{
	string reason;
	if (!OnAboutToCloseAssetInternal(reason))
	{
		unsavedChanges.push_back(reason);
		return false;
	}
	return true;
}

void CAssetEditor::closeEvent(QCloseEvent* pEvent)
{
	if (TryCloseAsset())
	{
		pEvent->accept();
	}
	else
	{
		pEvent->ignore();
	}

	for (CAssetType* pAssetType : m_supportedAssetTypes)
	{
		if (pAssetType->GetInstantEditor() == this)
		{
			pAssetType->SetInstantEditor(nullptr);
		}
	}
}

void CAssetEditor::customEvent(QEvent* pEvent)
{
	CDockableEditor::customEvent(pEvent);

	if (!pEvent->isAccepted() && pEvent->type() == SandboxEvent::Command)
	{
		CommandEvent* pCommandEvent = static_cast<CommandEvent*>(pEvent);
		const string& command = pCommandEvent->GetCommand();

		if (command == "asset.toggle_instant_editing")
		{
			SetInstantEditingMode(m_pLockAction->isChecked());
		}
	}
}

void CAssetEditor::dragEnterEvent(QDragEnterEvent* pEvent)
{
	auto pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());
	if (pDragDropData->HasCustomData("Assets"))
	{
		QByteArray byteArray = pDragDropData->GetCustomData("Assets");
		QDataStream stream(byteArray);

		QVector<quintptr> tmp;
		stream >> tmp;

		QVector<CAsset*> assets = *reinterpret_cast<QVector<CAsset*>*>(&tmp);

		if (assets.size() == 1 && CanOpenAsset(assets[0]))
		{
			CDragDropData::ShowDragText(this, tr("Open"));
			pEvent->acceptProposedAction();
			return;
		}
	}

	if (pDragDropData->HasCustomData("EngineFilePaths"))
	{
		QByteArray byteArray = pDragDropData->GetCustomData("EngineFilePaths");
		QDataStream stream(byteArray);

		QStringList engineFilePaths;
		stream >> engineFilePaths;

		const auto meshType = CAssetManager::GetInstance()->FindAssetType("Mesh");

		if (engineFilePaths.size() == 1)
		{
			CAsset* asset = CAssetManager::GetInstance()->FindAssetForFile(engineFilePaths[0].toStdString().c_str());
			if (asset && CanOpenAsset(asset))
			{
				CDragDropData::ShowDragText(this, tr("Open"));
				pEvent->acceptProposedAction();
				return;
			}
		}
	}

	if (pDragDropData->HasFilePaths())
	{
		const auto filePaths = pDragDropData->GetFilePaths();
		const auto meshType = CAssetManager::GetInstance()->FindAssetType("Mesh");

		if (filePaths.size() == 1)
		{
			CAsset* asset = CAssetManager::GetInstance()->FindAssetForFile(filePaths[0].toStdString().c_str());
			if (asset && CanOpenAsset(asset))
			{
				CDragDropData::ShowDragText(this, tr("Open"));
				pEvent->acceptProposedAction();
				return;
			}
		}
	}
}

void CAssetEditor::dropEvent(QDropEvent* pEvent)
{
	auto pDragDropData = CDragDropData::FromMimeData(pEvent->mimeData());
	if (pDragDropData->HasCustomData("Assets"))
	{
		QByteArray byteArray = pDragDropData->GetCustomData("Assets");
		QDataStream stream(byteArray);

		QVector<quintptr> tmp;
		stream >> tmp;

		QVector<CAsset*> assets = *reinterpret_cast<QVector<CAsset*>*>(&tmp);

		if (assets.size() == 1 && CanOpenAsset(assets[0]))
		{
			OpenAsset(assets[0]);
			pEvent->acceptProposedAction();
			return;
		}
	}

	if (pDragDropData->HasCustomData("EngineFilePaths"))
	{
		QByteArray byteArray = pDragDropData->GetCustomData("EngineFilePaths");
		QDataStream stream(byteArray);

		QStringList engineFilePaths;
		stream >> engineFilePaths;

		const auto meshType = CAssetManager::GetInstance()->FindAssetType("Mesh");

		if (engineFilePaths.size() == 1)
		{
			CAsset* asset = CAssetManager::GetInstance()->FindAssetForFile(engineFilePaths[0].toStdString().c_str());
			if (asset && CanOpenAsset(asset))
			{
				OpenAsset(asset);
				pEvent->acceptProposedAction();
				return;
			}
		}
	}

	if (pDragDropData->HasFilePaths())
	{
		const auto filePaths = pDragDropData->GetFilePaths();
		const auto meshType = CAssetManager::GetInstance()->FindAssetType("Mesh");

		if (filePaths.size() == 1)
		{
			CAsset* asset = CAssetManager::GetInstance()->FindAssetForFile(filePaths[0].toStdString().c_str());
			if (asset && CanOpenAsset(asset))
			{
				OpenAsset(asset);
				pEvent->acceptProposedAction();
				return;
			}
		}
	}
}

QToolButton* CAssetEditor::CreateLockButton()
{
	if (m_pLockButton)
	{
		return m_pLockButton;
	}

	m_pLockButton = new QToolButton();
	m_pLockButton->setDefaultAction(m_pLockAction);

	const bool foundInstantEditor = std::any_of(m_supportedAssetTypes.cbegin(), m_supportedAssetTypes.cend(), [](const CAssetType* pType)
	{
		return pType->GetInstantEditor() != nullptr;
	});

	if (!foundInstantEditor)
	{
		SetInstantEditingMode(true);
	}

	return m_pLockButton;
}

void CAssetEditor::DiscardAssetChanges()
{
	CAsset* pAsset = GetAssetBeingEdited();
	if (pAsset)
	{
		CEditableAsset editAsset(*pAsset);
		OnDiscardAssetChanges(editAsset);
		pAsset->SetModified(false);
	}
}

bool CAssetEditor::OnSave()
{
	CAsset* pAsset = GetAssetBeingEdited();
	if (pAsset)
	{
		pAsset->Save();
	}
	return true;
}

bool CAssetEditor::OnSaveAs()
{
	if (!m_assetBeingEdited)
	{
		return true;
	}

	if (!m_assetBeingEdited->GetType()->CanBeCopied() || !m_assetBeingEdited->GetEditingSession())
	{
		CRY_ASSERT_MESSAGE(m_assetBeingEdited->GetType()->CanBeCopied() && m_assetBeingEdited->GetEditingSession(), "%s asset type does not handle \"Save as\"", m_assetBeingEdited->GetType()->GetUiTypeName());
		return true;
	}

	CRY_ASSERT(!m_supportedAssetTypes.empty());
	CAssetType* const pAssetType = m_supportedAssetTypes[0];
	const string assetTypeName = pAssetType->GetTypeName();

	// #TODO: Let user save any of the supported asset types.
	const string assetBasePath = CAssetBrowserDialog::SaveSingleAssetForType(assetTypeName);
	if (assetBasePath.empty())
	{
		return true; // Operation cancelled by user.
	}
	const string newAssetPath = PathUtil::AdjustCasing(pAssetType->MakeMetadataFilename(assetBasePath));

	if (newAssetPath.CompareNoCase(m_assetBeingEdited->GetMetadataFile()) == 0)
	{
		return OnSave();
	}

	CAssetManager* const pAssetManager = CAssetManager::GetInstance();
	CAsset* pAsset = pAssetManager->FindAssetForMetadata(newAssetPath);
	if (pAsset)
	{
		// Cancel if unable to delete.
		pAssetManager->DeleteAssetsWithFiles({ pAsset });
		pAsset = pAssetManager->FindAssetForMetadata(newAssetPath);
		if (pAsset)
		{
			return true;
		}
	}

	// Create a copy.
	CAssetType::SCreateParams createParams;
	createParams.pSourceAsset = m_assetBeingEdited;
	if (!m_assetBeingEdited->GetType()->Create(newAssetPath, &createParams))
	{
		return true;
	}

	pAsset = pAssetManager->FindAssetForMetadata(newAssetPath);
	if (pAsset)
	{
		// Close previous asset and unconditionally discard all changes.
		DiscardAssetChanges();
		OnCloseAsset();
		CRY_ASSERT(GetAssetBeingEdited() != nullptr);
		signalAssetClosed(GetAssetBeingEdited());
		SetAssetBeingEdited(nullptr);

		OpenAsset(pAsset);
	}
	return true;
}

bool CAssetEditor::SaveBackup(const string& backupFolder)
{
	CRY_ASSERT(m_assetBeingEdited);
	using namespace Private_AssetEditor;

	// 1. Make a temporary copy of the current asset files.
	// 2. Save asset changes.
	// 3. Move updated files to the backupFolder.
	// 4. Restore old files from the temp copy.

	CAutoAssetRecovery tempCopy(*m_assetBeingEdited);
	
	if (!tempCopy.IsValid() || !OnSave())
	{
		return false;
	}

	std::vector<string> files = CAutoAssetRecovery::GetAssetFiles(*m_assetBeingEdited, false);

	ICryPak* const pCryPak = GetISystem()->GetIPak();
	pCryPak->MakeDir(backupFolder.c_str());
	const string assetsRoot = PathUtil::GetGameProjectAssetsPath();
	for (const auto& file : files)
	{
		CRY_ASSERT(strncmp(file.c_str(), assetsRoot.c_str(), assetsRoot.size()));
		CryPathString destFile = PathUtil::Make(backupFolder.c_str(), file.c_str() + assetsRoot.size());
		pCryPak->MakeDir(PathUtil::GetDirectory(destFile.c_str()));
		FileUtils::MoveFileAllowOverwrite(file.c_str(), destFile.c_str());
	}
	
	// tempCopy restores asset files.
	return true;
}

void CAssetEditor::SetInstantEditingMode(bool isActive)
{
	if (isActive)
	{
		for (CAssetType* pAssetType : m_supportedAssetTypes)
		{
			if (pAssetType->GetInstantEditor() == this)
			{
				continue;
			}

			if (pAssetType->GetInstantEditor())
			{
				pAssetType->GetInstantEditor()->SetInstantEditingMode(false);
			}
			pAssetType->SetInstantEditor(this);
		}
	}
	else
	{
		for (CAssetType* pAssetType : m_supportedAssetTypes)
		{
			if (pAssetType->GetInstantEditor() == this)
			{
				pAssetType->SetInstantEditor(nullptr);
			}
		}
	}

	if (m_pLockAction && m_pLockAction->isChecked() != isActive)
	{
		m_pLockAction->setChecked(isActive);
	}
}
