// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"
#include "../Common/Textures/TextureManager.h"
#include "../Common/Textures/TextureHelpers.h"
#include "../Common/RendElements/FlareSoftOcclusionQuery.h"
#include "../Common/ReverseDepth.h"
#include "GraphicsPipeline/TiledLightVolumes.h"
#include "GraphicsPipeline/ClipVolumes.h"
#include "GraphicsPipeline/DeferredDecals.h"
#include "GraphicsPipeline/ShadowMask.h"
#include "GraphicsPipeline/Water.h"
#if defined(FEATURE_SVO_GI)
	#include "D3D_SVO.h"
#endif

#include "Common/RenderView.h"
#include "Common/RendererResources.h"

#define MAX_VIS_AREAS 32

// MSAA potential optimizations todo:
//  - long term: port all functionality to compute, including all extra effects passes.

// About MSAA:
// - Please be careful when accessing or rendering into msaa'ed targets. When adding new techniques please make sure to test
// - For post process technique to be MSAA friendly, do either:
//    - Use compute. Single pass and as efficient as gets. Context switches might be problematic, until all lighting pipeline done like this.
//    - For non compute, require 2 passes. One at pixel frequency, other at sub sample frequency.
//				 - Reuse existing sample frequency regions on stencil via stencilread/write mask:
//						- If not possible, tag pixel frequency regions using stencil + m_pMSAAMaskRT
//						- Alternative poor man version, do clip in shader.

#pragma warning(push)
#pragma warning(disable: 4244)

CDeferredShading* CDeferredShading::m_pInstance = NULL;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::ReleaseData(std::shared_ptr<CGraphicsPipeline> pGraphicsPipeline)
{
	CRY_ASSERT(pGraphicsPipeline);
	auto* tiledLights = pGraphicsPipeline->GetStage<CTiledLightVolumesStage>();
	tiledLights->Clear();

	m_shadowPoolAlloc.SetUse(0);
	stl::free_container(m_shadowPoolAlloc);

	m_blockPack.FreeContainers();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::SetupPasses(CRenderView* pRenderView)
{
	m_pDiffuseRT = CRendererResources::s_ptexSceneDiffuse;

	const auto& viewInfo = pRenderView->GetViewInfo(CCamera::eEye_Left);

	m_pCamFront = viewInfo.cameraVZ;
	m_pCamFront.Normalize();
	m_pCamPos = viewInfo.cameraOrigin;

	m_fCamFar = viewInfo.farClipPlane;
	m_fCamNear = viewInfo.nearClipPlane;

	m_pView = viewInfo.viewMatrix;
	m_pView.Transpose();

	m_mViewProj = viewInfo.cameraProjMatrix;
	m_mViewProj.Transpose();

	m_nCurTargetWidth = m_pDiffuseRT->GetWidth();
	m_nCurTargetHeight = m_pDiffuseRT->GetHeight();

	SetupGlobalConsts(pRenderView);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::SetupGlobalConsts(CRenderView* pRenderView)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	//set world basis
	float maskRTWidth = float(m_nCurTargetWidth);
	float maskRTHeight = float(m_nCurTargetHeight);
	Vec4r vWBasisX, vWBasisY, vWBasisZ, vCamPos;
	Vec4 vParamValue, vMag;

	CShadowUtils::ProjectScreenToWorldExpansionBasis(rd->m_IdentityMatrix, pRenderView->GetCamera(CCamera::eEye_Left), pRenderView->m_vProjMatrixSubPixoffset, maskRTWidth, maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, vCamPos, true);

	const auto& downscaleFactor = gRenDev->GetRenderQuality().downscaleFactor;
	vWorldBasisX = vWBasisX / downscaleFactor.x;
	vWorldBasisY = vWBasisY / downscaleFactor.y;
	vWorldBasisZ = vWBasisZ;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::GetLightRenderSettings(const CRenderView* pRenderView, const SRenderLight* const __restrict pDL, bool& bStencilMask, bool& bUseLightVolumes, EShapeMeshType& meshType)
{
	const bool bAreaLight = (pDL->m_Flags & DLF_AREA_LIGHT) && pDL->m_fAreaWidth && pDL->m_fAreaHeight && pDL->m_fLightFrustumAngle;

	if (CRenderer::CV_r_deferredshadingLightVolumes)
	{
		if (bAreaLight)
		{
			// area lights use non-uniform box volume
			// need to do more complex box intersection test
			float fExpensionRadius = pDL->m_fRadius * 1.08f;
			Vec3 vScale(fExpensionRadius, fExpensionRadius, fExpensionRadius);

			Matrix34 mObjInv = CShadowUtils::GetAreaLightMatrix(pDL, vScale);
			mObjInv.Invert();

			// check if volumes bounding box intersects the near clipping plane
			const CCamera& camera = pRenderView->GetCamera(CCamera::eEye_Left);
			const Plane* pNearPlane(camera.GetFrustumPlane(FR_PLANE_NEAR));
			Vec3 pntOnNearPlane(camera.GetPosition() - pNearPlane->DistFromPlane(camera.GetPosition()) * pNearPlane->n);
			Vec3 pntOnNearPlaneOS(mObjInv.TransformPoint(pntOnNearPlane));

			Vec3 nearPlaneOS_n(mObjInv.TransformVector(pNearPlane->n));
			f32 nearPlaneOS_d(-nearPlaneOS_n.Dot(pntOnNearPlaneOS));

			// get extreme lengths
			float t(fabsf(nearPlaneOS_n.x) + fabsf(nearPlaneOS_n.y) + fabsf(nearPlaneOS_n.z));

			float t0 = t + nearPlaneOS_d;
			float t1 = -t + nearPlaneOS_d;

			if (t0 * t1 > 0.0f)
				bUseLightVolumes = true;
			else
				bStencilMask = true;
		}
		else
		{
			const float kDLRadius = pDL->m_fRadius;
			const float fSmallLightBias = 0.5f;
			// the light mesh tessellation and near clipping plane require some bias when testing if inside sphere
			// higher bias for low radius lights
			float fSqLightRadius = kDLRadius * max(-0.1f * kDLRadius + 1.5f, 1.22f);
			fSqLightRadius = max(kDLRadius + fSmallLightBias, fSqLightRadius); //always add on a minimum bias, for very small light's sake
			fSqLightRadius *= fSqLightRadius;
			if (fSqLightRadius < pDL->m_Origin.GetSquaredDistance(m_pCamPos))
				bUseLightVolumes = true;
			else
				bStencilMask = true;
		}
	}

	Vec4 pLightRect = Vec4(pDL->m_sX, pDL->m_sY, pDL->m_sWidth, pDL->m_sHeight);

	float fCurTargetWidth = (float)(m_nCurTargetWidth);
	float fCurTargetHeight = (float)(m_nCurTargetHeight);

	if (!iszero(CRenderer::CV_r_DeferredShadingLightLodRatio))
	{
		if (CRenderer::CV_r_DeferredShadingLightStencilRatio > 0.01f)
		{
			const float fLightLodRatioScale = CRenderer::CV_r_DeferredShadingLightLodRatio;
			float fLightArea = pLightRect.z * pLightRect.w;
			float fScreenArea = fCurTargetHeight * fCurTargetWidth;
			float fLightRatio = fLightLodRatioScale * (fLightArea / fScreenArea);

			const float fDrawVolumeThres = 0.005f;
			if (fLightRatio < fDrawVolumeThres)
			{
				bUseLightVolumes = false;
			}

			if (fLightRatio > 4 * CRenderer::CV_r_DeferredShadingLightStencilRatio)
				meshType = SHAPE_PROJECTOR2;
			else if (fLightRatio > 2 * CRenderer::CV_r_DeferredShadingLightStencilRatio)
				meshType = SHAPE_PROJECTOR1;
		}
		else
		{
			const float fLightLodRatioScale = CRenderer::CV_r_DeferredShadingLightLodRatio;
			float fLightArea = pLightRect.z * pLightRect.w;
			float fScreenArea = fCurTargetHeight * fCurTargetWidth;
			float fLightRatio = fLightLodRatioScale * (fLightArea / fScreenArea);

			const float fDrawVolumeThres = 0.005f;
			if (fLightRatio < fDrawVolumeThres)
			{
				bUseLightVolumes = false;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

struct CubemapsCompare
{
	bool operator()(const SRenderLight& l0, const SRenderLight& l1) const
	{
		// Cubes sort by: Sort priority first, light radius, lastly by entity id (insertion order every frame is not guaranteed)
		if (l0.m_nSortPriority != l1.m_nSortPriority)
			return l0.m_nSortPriority < l1.m_nSortPriority;

		if (l0.m_fRadius == l1.m_fRadius)
			return l0.m_nEntityId < l1.m_nEntityId;

		return l0.m_fRadius > l1.m_fRadius;
	}
};

struct CubemapsCompareInv
{
	bool operator()(const SRenderLight& l0, const SRenderLight& l1) const
	{
		// Cubes sort by: Sort priority first, light radius, lastly by entity id (insertion order every frame is not guaranteed)
		if (l0.m_nSortPriority != l1.m_nSortPriority)
			return l0.m_nSortPriority > l1.m_nSortPriority;

		if (l0.m_fRadius == l1.m_fRadius)
			return l0.m_nEntityId > l1.m_nEntityId;

		return l0.m_fRadius < l1.m_fRadius;
	}
};

struct LightsCompare
{
	bool operator()(const SRenderLight& l0, const SRenderLight& l1) const
	{
		if (!(l0.m_Flags & DLF_CASTSHADOW_MAPS) && (l1.m_Flags & DLF_CASTSHADOW_MAPS))
		{
			return true;
		}
		else
			return false;
	}
};

struct DeffDecalSort
{
	bool operator()(const SDeferredDecal& decal0, const SDeferredDecal& decal1) const
	{
		uint bBump0 = (decal0.nFlags & DECAL_HAS_NORMAL_MAP);
		uint bBump1 = (decal1.nFlags & DECAL_HAS_NORMAL_MAP);
		//bump-mapped decals first
		if (bBump0 != bBump1)
			return (bBump0 < bBump1);
		return
		  (decal0.nSortOrder < decal1.nSortOrder);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::Release()
{
	CRendererResources::DestroyDeferredMaps();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CRenderer::EF_ReleaseDeferredData()
{
	if (CDeferredShading::IsValid())
		CDeferredShading::Instance().ReleaseData(m_pActiveGraphicsPipeline);
}