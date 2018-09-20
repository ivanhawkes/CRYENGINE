// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EffectAsset.h"

#include <AssetSystem/EditableAsset.h>

namespace CryParticleEditor
{

const char* CEffectAsset::GetName() const
{
	/*
	   if (m_pExplorerEntry && !m_pExplorerEntry->name.empty())
	   return m_pExplorerEntry->name.c_str();
	   else
	   return m_pEffect->GetName();
	 */
	return "STUFF";
}

pfx2::IParticleEffectPfx2* CEffectAsset::GetEffect()
{
	return m_pEffect;
}

CryParticleEditor::CParticleGraphModel* CEffectAsset::GetModel()
{
	return m_pModel.get();
}

void CEffectAsset::SetEffect(pfx2::IParticleEffectPfx2* pEffect)
{
	m_pEffect = pEffect;
	m_pModel = pEffect ? stl::make_unique<CParticleGraphModel>(*pEffect) : nullptr;
}

bool CEffectAsset::MakeNewComponent(const char* szTemplateName)
{
	if (m_pModel)
	{
		return m_pModel->CreateNode(szTemplateName, QPointF()) != nullptr;
	}
	return false;
}

}
