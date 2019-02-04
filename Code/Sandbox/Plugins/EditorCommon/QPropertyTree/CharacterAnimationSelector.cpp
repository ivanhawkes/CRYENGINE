// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"

#include <Models/CharacterAnimationsModel.h>
#include <Dialogs/TreeViewDialog.h>
#include "IResourceSelectorHost.h"

#include <CryAnimation/ICryAnimation.h>
#include <CryEntitySystem/IEntity.h>

SResourceSelectionResult CharacterAnimationSelector(const SResourceSelectorContext& context, const char* previousValue, IEntity* pEntity)
{
	SResourceSelectionResult result { false, previousValue };

	if (!pEntity || !pEntity->GetCharacter(0) || !pEntity->GetCharacter(0)->GetIAnimationSet())
	{
		return result;
	}

	IAnimationSet* pAnimSet = pEntity->GetCharacter(0)->GetIAnimationSet();
	CCharacterAnimationsModel* pModel = new CCharacterAnimationsModel(*pAnimSet);
	CTreeViewDialog dialog(context.parentWidget);
	QString selectedValue(previousValue);

	dialog.Initialize(pModel, 0);

	for (int32 row = pModel->rowCount(); row--;)
	{
		const QModelIndex index = pModel->index(row, 0);
		if (pModel->data(index, Qt::DisplayRole).value<QString>() == selectedValue)
		{
			dialog.SetSelectedValue(index, false);
		}
	}

	result.selectionAccepted = dialog.exec();
	if (result.selectionAccepted)
	{
		QModelIndex index = dialog.GetSelected();
		if (index.isValid())
		{
			result.selectedResource = index.data().value<QString>().toLocal8Bit().data();
		}
	}

	return result;
}

dll_string ValidateCharacterAnimation(const SResourceSelectorContext& context, const char* newValue, const char* previousValue, IEntity* pEntity)
{
	if (!newValue || !*newValue)
		return dll_string();

	if (!pEntity || !pEntity->GetCharacter(0) || !pEntity->GetCharacter(0)->GetIAnimationSet())
		return previousValue;

	IAnimationSet* pAnimSet = pEntity->GetCharacter(0)->GetIAnimationSet();
	CCharacterAnimationsModel* pModel = new CCharacterAnimationsModel(*pAnimSet);
	int itemCount = pModel->rowCount();
	for (int i = 0; i < itemCount; ++i)
	{
		QModelIndex index = pModel->index(i, 0);
		if (pModel->data(index, Qt::DisplayRole).value<QString>() == newValue)
			return newValue;
	}

	return previousValue;
}

dll_string ValidateTrackCharacterAnimation(const SResourceSelectorContext& context, const char* newValue, const char* previousValue, IEntity* pEntity)
{
	return newValue;
}

REGISTER_RESOURCE_VALIDATING_SELECTOR("TrackCharacterAnimation", CharacterAnimationSelector, ValidateTrackCharacterAnimation, "")
REGISTER_RESOURCE_VALIDATING_SELECTOR("CharacterAnimation", CharacterAnimationSelector, ValidateCharacterAnimation, "")
