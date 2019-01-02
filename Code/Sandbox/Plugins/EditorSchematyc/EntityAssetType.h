// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AssetSystem/AssetType.h>

namespace CrySchematycEditor {

class CEntityAssetType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CEntityAssetType);

	static const char* TypeName() { return "SchematycEntity"; }

	// CAssetType
	virtual const char*   GetTypeName() const override       { return TypeName(); }
	virtual const char*   GetUiTypeName() const override     { return QT_TR_NOOP("Schematyc Entity"); }

	virtual const char*   GetFileExtension() const override  { return "schematyc_ent"; }

	virtual bool          CanBeCreated() const override      { return true; }
	virtual bool          IsImported() const override        { return false; }
	virtual bool          CanBeEdited() const override       { return true; }
	virtual QColor        GetThumbnailColor() const override { return QColor(114, 169, 216); }

	virtual CAssetEditor* Edit(CAsset* pAsset) const override;
	virtual bool          RenameAsset(CAsset* pAsset, const char* szNewName) const override;
	virtual void          PreDeleteAssetFiles(const CAsset& asset) const override;

	virtual const char*   GetObjectClassName() const override { return "Entity"; }
	virtual string        GetObjectFilePath(const CAsset* pAsset) const override;

protected:
	virtual bool OnCreate(INewAsset& asset, const SCreateParams* pCreateParams) const override;

private:
	virtual CryIcon GetIconInternal() const override;
	// ~CAssetType

	Schematyc::IScript* GetScript(const CAsset& asset) const;
};

}
