// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ViewWidget.h"

#include <QWidget>

namespace CryParticleEditor {

class CEffectAssetModel;

class CEffectAssetWidget : public QWidget
{
	Q_OBJECT

public:
	CEffectAssetWidget(CEffectAssetModel* pEffectAssetModel, QWidget* pParent = nullptr);
	~CEffectAssetWidget();

	const pfx2::IParticleEffect* GetEffect() const;
	pfx2::IParticleEffect*       GetEffect();
	const char*                      GetName() const;

	void                             OnDeleteSelected();
	void                             CopyComponents();
	void                             OnPasteComponent();
	void                             OnNewComponent();

	bool                             MakeNewComponent(const char* szTemplateName);

protected:
	// QWidget
	virtual void customEvent(QEvent* event) override;
	virtual void paintEvent(QPaintEvent* event) override;
	// ~QWidget

private:
	void OnBeginEffectAssetChange();
	void OnEndEffectAssetChange();
	void OnEffectEdited(int nComp, int nFeature);

private:
	CEffectAssetModel*             m_pEffectAssetModel;
	CryParticleEditor::CGraphView* m_pGraphView;
	bool                           m_updated = false;
};

}
