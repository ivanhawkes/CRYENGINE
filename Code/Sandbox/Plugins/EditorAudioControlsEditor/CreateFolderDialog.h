// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Controls/EditorDialog.h>

namespace ACE
{
class CCreateFolderDialog final : public CEditorDialog
{
	Q_OBJECT

public:

	CCreateFolderDialog() = delete;

	explicit CCreateFolderDialog(QWidget* const pParent);

signals:

	void SignalSetFolderName(QString const& folderName);

private:

	void OnAccept();

	QString m_folderName;
};
} // namespace ACE
