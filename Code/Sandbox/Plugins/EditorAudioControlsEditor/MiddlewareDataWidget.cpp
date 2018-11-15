// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MiddlewareDataWidget.h"

#include "Common.h"
#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"
#include "SystemControlsWidget.h"
#include "AssetIcons.h"
#include "FileImporterDialog.h"
#include "Common/IImpl.h"

#include <PathUtils.h>
#include <QtUtil.h>
#include <FileDialogs/SystemFileDialog.h>

#include <QDir>
#include <QVBoxLayout>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CMiddlewareDataWidget::CMiddlewareDataWidget(QWidget* const pParent)
	: QWidget(pParent)
	, m_pLayout(new QVBoxLayout(this))
	, m_pImplDataPanel(nullptr)
{
	m_pLayout->setContentsMargins(0, 0, 0, 0);
	InitImplDataWidget();

	g_implementationManager.SignalOnBeforeImplementationChange.Connect([this]()
		{
			ClearImplDataWidget();
		}, reinterpret_cast<uintptr_t>(this));

	g_implementationManager.SignalOnAfterImplementationChange.Connect([this]()
		{
			InitImplDataWidget();
		}, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CMiddlewareDataWidget::~CMiddlewareDataWidget()
{
	g_implementationManager.SignalOnBeforeImplementationChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implementationManager.SignalOnAfterImplementationChange.DisconnectById(reinterpret_cast<uintptr_t>(this));

	ClearImplDataWidget();
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::InitImplDataWidget()
{
	if (g_pIImpl != nullptr)
	{
		m_pImplDataPanel = g_pIImpl->CreateDataPanel();
		m_pLayout->addWidget(m_pImplDataPanel);

		g_pIImpl->SignalGetConnectedSystemControls.Connect([&](ControlId const id, SControlInfos& controlInfos)
			{
				GetConnectedControls(id, controlInfos);
			}, reinterpret_cast<uintptr_t>(this));

		g_pIImpl->SignalSelectConnectedSystemControl.Connect([&](ControlId const systemControlId, ControlId const implItemId)
			{
				if (g_pSystemControlsWidget != nullptr)
				{
				  g_pSystemControlsWidget->SelectConnectedSystemControl(systemControlId, implItemId);
				}

			}, reinterpret_cast<uintptr_t>(this));

		g_pIImpl->SignalImportFiles.Connect([&](ExtensionFilterVector const& extensionFilters, QStringList const& supportedType, QString const& targetFolderName, bool const isLocalized)
			{
				OnImportFiles(extensionFilters, supportedType, targetFolderName, isLocalized);
			}, reinterpret_cast<uintptr_t>(this));

		g_pIImpl->SignalFilesDropped.Connect([&](FileImportInfos const& fileImportInfos, QString const& targetFolderName, bool const isLocalized)
			{
				OpenFileImporter(fileImportInfos, targetFolderName, isLocalized);
			}, reinterpret_cast<uintptr_t>(this));
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::ClearImplDataWidget()
{
	if (g_pIImpl != nullptr)
	{
		g_pIImpl->SignalGetConnectedSystemControls.DisconnectById(reinterpret_cast<uintptr_t>(this));
		g_pIImpl->SignalSelectConnectedSystemControl.DisconnectById(reinterpret_cast<uintptr_t>(this));
		g_pIImpl->SignalImportFiles.DisconnectById(reinterpret_cast<uintptr_t>(this));
		g_pIImpl->SignalFilesDropped.DisconnectById(reinterpret_cast<uintptr_t>(this));
		m_pLayout->removeWidget(m_pImplDataPanel);
		m_pImplDataPanel = nullptr;
		g_pIImpl->DestroyDataPanel();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::GetConnectedControls(ControlId const implItemId, SControlInfos& controlInfos)
{
	auto const& controls = g_assetsManager.GetControls();

	for (auto const pControl : controls)
	{
		if (pControl->GetConnection(implItemId) != nullptr)
		{
			SControlInfo info(pControl->GetName(), pControl->GetId(), GetAssetIcon(pControl->GetType()));
			controlInfos.emplace_back(info);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::OnImportFiles(
	ExtensionFilterVector const& extensionFilters,
	QStringList const& supportedTypes,
	QString const& targetFolderName,
	bool const isLocalized)
{
	g_pIImpl->OnFileImporterOpened();
	CSystemFileDialog::RunParams runParams;
	runParams.extensionFilters = extensionFilters;
	runParams.title = tr("Import Audio Files");
	runParams.buttonLabel = tr("Import");
	std::vector<QString> const importedFiles = CSystemFileDialog::RunImportMultipleFiles(runParams, this);
	g_pIImpl->OnFileImporterClosed();

	if (!importedFiles.empty())
	{
		FileImportInfos fileInfos;

		for (auto const& filePath : importedFiles)
		{
			QFileInfo const& fileInfo(filePath);

			if (fileInfo.isFile())
			{
				fileInfos.emplace_back(fileInfo, supportedTypes.contains(fileInfo.suffix(), Qt::CaseInsensitive));
			}
		}

		OpenFileImporter(fileInfos, targetFolderName, isLocalized);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::OpenFileImporter(FileImportInfos const& fileImportInfos, QString const& targetFolderName, bool const isLocalized)
{
	FileImportInfos fileInfos = fileImportInfos;

	QString assetsPath = QtUtil::ToQString(PathUtil::GetGameFolder() + "/" + g_implInfo.assetsPath.c_str());
	QString localizedAssetsPath = QtUtil::ToQString(PathUtil::GetGameFolder() + "/" + g_implInfo.localizedAssetsPath.c_str());
	QString const targetFolderPath = (isLocalized ? localizedAssetsPath : assetsPath) + "/" + targetFolderName;

	QDir const targetFolder(targetFolderPath);
	QString const fullTargetPath = targetFolder.absolutePath() + "/";

	for (auto& fileInfo : fileInfos)
	{
		if (fileInfo.isTypeSupported && fileInfo.sourceInfo.isFile())
		{
			QString const targetPath = fullTargetPath + fileInfo.parentFolderName + fileInfo.sourceInfo.fileName();
			QFileInfo const targetFile(targetPath);

			fileInfo.targetInfo = targetFile;

			if (fileInfo.sourceInfo == fileInfo.targetInfo)
			{
				fileInfo.actionType = SFileImportInfo::EActionType::SameFile;
			}
			else
			{
				fileInfo.actionType = (targetFile.isFile() ? SFileImportInfo::EActionType::Replace : SFileImportInfo::EActionType::New);
			}
		}
	}

	assetsPath = QDir(assetsPath).absolutePath();
	localizedAssetsPath = QDir(localizedAssetsPath).absolutePath();

	auto const pFileImporterDialog = new CFileImporterDialog(fileInfos, assetsPath, localizedAssetsPath, fullTargetPath, targetFolderName, isLocalized, this);
	g_pIImpl->OnFileImporterOpened();

	QObject::connect(pFileImporterDialog, &CFileImporterDialog::destroyed, [&]()
		{
			g_pIImpl->OnFileImporterClosed();
		});

	pFileImporterDialog->exec();
}
} // namespace ACE
