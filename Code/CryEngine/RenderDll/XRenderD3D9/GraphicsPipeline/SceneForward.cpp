// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SceneForward.h"

#include "D3D_SVO.h"
#include "Common/ReverseDepth.h"
#include "Common/RendElements/Stars.h"

#include "GraphicsPipeline/Fog.h"
#include "GraphicsPipeline/VolumetricFog.h"
#include "GraphicsPipeline/TiledLightVolumes.h"
#include "GraphicsPipeline/ClipVolumes.h"
#include "GraphicsPipeline/TiledShading.h"

struct SPerPassConstantBuffer
{
	CFogStage::SForwardParams                 cbFog;
	CVolumetricFogStage::SForwardParams       cbVoxelFog;
	CShadowUtils::SShadowCascadesSamplingInfo cbShadowSampling;
	CSceneForwardStage::SCloudShadingParams   cbClouds;
#if defined(FEATURE_SVO_GI)
	CSvoRenderer::SForwardParams              cbSVOGI;
#endif
};

CSceneForwardStage::CSceneForwardStage()
	: m_opaquePassResources()
	, m_transparentPassResources()
	, m_eyeOverlayPassResources()
{}

void CSceneForwardStage::Init()
{
	// Create per-pass resources
#if RENDERER_ENABLE_FULL_PIPELINE
	m_pOpaquePassResourceSet      = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
	m_pTransparentPassResourceSet = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
	m_pEyeOverlayPassResourceSet  = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
#endif

#if RENDERER_ENABLE_MOBILE_PIPELINE
	m_pOpaquePassResourceSetMobile      = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
	m_pTransparentPassResourceSetMobile = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
#endif

	m_pPerPassCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SPerPassConstantBuffer));
	
	bool bSuccess = PreparePerPassResources(true);
	assert(bSuccess);

#if RENDERER_ENABLE_FULL_PIPELINE
	// Create resource layout
	m_pOpaqueResourceLayout      = gcpRendD3D->GetGraphicsPipeline().CreateScenePassLayout(m_opaquePassResources);
	m_pTransparentResourceLayout = gcpRendD3D->GetGraphicsPipeline().CreateScenePassLayout(m_transparentPassResources);
	m_pEyeOverlayResourceLayout  = gcpRendD3D->GetGraphicsPipeline().CreateScenePassLayout(m_eyeOverlayPassResources);

	// Freeze resource-set layout (assert  will fire when violating the constraint)
	m_opaquePassResources     .AcceptChangedBindPoints();
	m_transparentPassResources.AcceptChangedBindPoints();
	m_eyeOverlayPassResources .AcceptChangedBindPoints();
#endif

#if RENDERER_ENABLE_MOBILE_PIPELINE
	// Opaque mobile forward scene pass
	m_pOpaqueResourceLayoutMobile      = gcpRendD3D->GetGraphicsPipeline().CreateScenePassLayout(m_opaquePassResourcesMobile);
	m_pTransparentResourceLayoutMobile = gcpRendD3D->GetGraphicsPipeline().CreateScenePassLayout(m_transparentPassResourcesMobile);

	// Freeze resource-set layout (assert  will fire when violating the constraint)
	m_opaquePassResourcesMobile.     AcceptChangedBindPoints();
	m_transparentPassResourcesMobile.AcceptChangedBindPoints();
#endif

	m_forwardOpaquePass.SetLabel("OPAQUE");
	m_forwardOpaquePass.SetFlags(CSceneRenderPass::ePassFlags_ReverseDepth | CSceneRenderPass::ePassFlags_VrProjectionPass | CSceneRenderPass::ePassFlags_RenderNearest);
	m_forwardOpaquePass.SetPassResources(m_pOpaqueResourceLayout, m_pOpaquePassResourceSet);

	m_forwardEyeOverlayPass.SetLabel("OVERLAYS");
	m_forwardEyeOverlayPass.SetFlags(CSceneRenderPass::ePassFlags_ReverseDepth);
	m_forwardEyeOverlayPass.SetPassResources(m_pEyeOverlayResourceLayout, m_pEyeOverlayPassResourceSet);

	m_forwardOverlayPass.SetLabel("OVERLAYS");
	m_forwardOverlayPass.SetFlags(CSceneRenderPass::ePassFlags_ReverseDepth);
	m_forwardOverlayPass.SetPassResources(m_pOpaqueResourceLayout, m_pOpaquePassResourceSet);

	m_forwardTransparentBWPass.SetLabel("TRANSPARENT_BW");
	m_forwardTransparentBWPass.SetFlags(CSceneRenderPass::ePassFlags_ReverseDepth);
	m_forwardTransparentBWPass.SetPassResources(m_pTransparentResourceLayout, m_pTransparentPassResourceSet);

	m_forwardTransparentAWPass.SetLabel("TRANSPARENT_AW");
	m_forwardTransparentAWPass.SetFlags(CSceneRenderPass::ePassFlags_ReverseDepth | CSceneRenderPass::ePassFlags_RenderNearest);
	m_forwardTransparentAWPass.SetPassResources(m_pTransparentResourceLayout, m_pTransparentPassResourceSet);

	m_forwardTransparentLoResPass.SetLabel("TRANSPARENT_SUBRES");
	m_forwardTransparentLoResPass.SetFlags(CSceneRenderPass::ePassFlags_ReverseDepth);
	m_forwardTransparentLoResPass.SetPassResources(m_pTransparentResourceLayout, m_pTransparentPassResourceSet);

	m_forwardHDRPass.SetLabel("FORWARD_AFTER_POSTFX_HDR");
	m_forwardHDRPass.SetFlags(CSceneRenderPass::ePassFlags_None);
	m_forwardHDRPass.SetPassResources(m_pTransparentResourceLayout, m_pTransparentPassResourceSet);

	m_forwardLDRPass.SetLabel("FORWARD_AFTER_POSTFX_LDR");
	m_forwardLDRPass.SetFlags(CSceneRenderPass::ePassFlags_None);
	m_forwardLDRPass.SetPassResources(m_pTransparentResourceLayout, m_pTransparentPassResourceSet);

#if RENDERER_ENABLE_MOBILE_PIPELINE
	m_forwardOpaquePassMobile.SetLabel("OPAQUE");
	m_forwardOpaquePassMobile.SetFlags(CSceneRenderPass::ePassFlags_ReverseDepth | CSceneRenderPass::ePassFlags_VrProjectionPass | CSceneRenderPass::ePassFlags_RenderNearest);
	m_forwardOpaquePassMobile.SetPassResources(m_pOpaqueResourceLayoutMobile, m_pOpaquePassResourceSetMobile);

	m_forwardOverlayPassMobile.SetLabel("OVERLAYS");
	m_forwardOverlayPassMobile.SetFlags(CSceneRenderPass::ePassFlags_ReverseDepth | CSceneRenderPass::ePassFlags_VrProjectionPass | CSceneRenderPass::ePassFlags_RenderNearest);
	m_forwardOverlayPassMobile.SetPassResources(m_pOpaqueResourceLayoutMobile, m_pOpaquePassResourceSetMobile);

	m_forwardTransparentPassMobile.SetLabel("TRANSPARENT");
	m_forwardTransparentPassMobile.SetFlags(CSceneRenderPass::ePassFlags_ReverseDepth | CSceneRenderPass::ePassFlags_RenderNearest);
	m_forwardTransparentPassMobile.SetPassResources(m_pTransparentResourceLayoutMobile, m_pTransparentPassResourceSetMobile);
#endif

#if RENDERER_ENABLE_FULL_PIPELINE
	m_forwardOpaqueRecursivePass.SetLabel("OPAQUE_RECURSIVE");
	m_forwardOpaqueRecursivePass.SetFlags(CSceneRenderPass::ePassFlags_ReverseDepth);
	m_forwardOpaqueRecursivePass.SetPassResources(m_pOpaqueResourceLayout, m_pOpaquePassResourceSet);

	m_forwardOverlayRecursivePass.SetLabel("OVERLAY_RECURSIVE");
	m_forwardOverlayRecursivePass.SetFlags(CSceneRenderPass::ePassFlags_ReverseDepth);
	m_forwardOverlayRecursivePass.SetPassResources(m_pOpaqueResourceLayout, m_pOpaquePassResourceSet);

	m_forwardTransparentRecursivePass.SetLabel("TRANSPARENT_RECURSIVE");
	m_forwardTransparentRecursivePass.SetFlags(CSceneRenderPass::ePassFlags_ReverseDepth);
	m_forwardTransparentRecursivePass.SetPassResources(m_pTransparentResourceLayout, m_pTransparentPassResourceSet);
#endif
}

void CSceneForwardStage::Update()
{
	const CRenderView*   pRenderView = RenderView();
	const CRenderOutput* pRenderOutput = pRenderView->GetRenderOutput();
	const SRenderViewport& viewport = pRenderView->GetViewport();

	CTexture* pCTexture = pRenderView->GetColorTarget();
	CTexture* pZTexture = pRenderView->GetDepthTarget();

	CTexture* pCTextureOut = pRenderOutput->GetColorTarget();
	CTexture* pZTextureOut = pRenderOutput->GetDepthTarget();

	CStandardGraphicsPipeline* p = static_cast<CStandardGraphicsPipeline*>(&GetGraphicsPipeline());
	EShaderRenderingFlags flags = (EShaderRenderingFlags)p->GetRenderFlags();
	const bool isForwardMinimal = (flags & SHDF_FORWARD_MINIMAL) != 0;

	if (!isForwardMinimal)
	{
#if RENDERER_ENABLE_FULL_PIPELINE
		CTexture* pDepthTexture = pRenderView->GetDepthTarget();

		// Opaque forward scene pass
		m_forwardOpaquePass.SetViewport(viewport);
		m_forwardOpaquePass.SetRenderTargets(
			// Depth
			pDepthTexture,
			// Color 0
			CRendererResources::s_ptexHDRTarget
		);

		// Overlay forward scene pass
		m_forwardEyeOverlayPass.SetViewport(viewport);
		m_forwardEyeOverlayPass.SetRenderTargets(
			// Depth
			pDepthTexture,
			// Color 0
			CRendererResources::s_ptexSceneDiffuse 
		);

		m_forwardOverlayPass.SetViewport(viewport);
		m_forwardOverlayPass.SetRenderTargets(
			// Depth
			pDepthTexture,
			// Color 0
			CRendererResources::s_ptexHDRTarget
		);

		// Transparent forward scene passes
		m_forwardTransparentBWPass.SetViewport(viewport);
		m_forwardTransparentBWPass.SetRenderTargets(
			// Depth
			pDepthTexture,
			// Color 0
			CRendererResources::s_ptexHDRTarget
		);

		m_forwardTransparentAWPass.SetViewport(viewport);
		m_forwardTransparentAWPass.SetRenderTargets(
			// Depth
			pDepthTexture,
			// Color 0
			CRendererResources::s_ptexHDRTarget
		);

		m_forwardTransparentLoResPass.SetViewport(viewport);
		m_forwardTransparentLoResPass.SetRenderTargets(
			// Depth
			CRendererResources::s_ptexSceneDepthScaled[0],//[1]//[2]
			// Color 0
			CRendererResources::s_ptexHDRTargetScaled[0][0]//[1][0]//[2][0]
		);

		m_forwardHDRPass.SetViewport(viewport);
		m_forwardHDRPass.SetRenderTargets(
			// Depth
			pZTexture,
			// Color 0
			// TODO: CPostEffectContext::GetDstBackBufferTexture() pre-EnableAltBackBuffer()
			CRendererResources::s_ptexDisplayTargetDst
		);

		m_forwardLDRPass.SetViewport(viewport);
		m_forwardLDRPass.SetRenderTargets(
			// Depth
			pZTextureOut,
			// Color 0
			// TODO: CPostEffectContext::GetDstBackBufferTexture() post-EnableAltBackBuffer()
			pCTextureOut
		);
#endif

#if RENDERER_ENABLE_MOBILE_PIPELINE
		m_forwardOpaquePassMobile.SetViewport(viewport);
		m_forwardOpaquePassMobile.SetRenderTargets(
			// Depth
			pRenderView->GetDepthTarget(),
			// Color 0
			CRendererResources::s_ptexHDRTarget
		);

		m_forwardOverlayPassMobile.SetViewport(viewport);
		m_forwardOverlayPassMobile.SetRenderTargets(
			// Depth
			pRenderView->GetDepthTarget(),
			// Color 0
			CRendererResources::s_ptexHDRTarget
		);
		
		m_forwardTransparentPassMobile.SetViewport(viewport);
		m_forwardTransparentPassMobile.SetRenderTargets(
			// Depth
			pRenderView->GetDepthTarget(),
			// Color 0
			CRendererResources::s_ptexHDRTarget
		);
#endif
	}

#if RENDERER_ENABLE_FULL_PIPELINE
	// Recursive passes need to be updated in the non-recursive update as well because of PSO creation
	{
		CTexture* pDepthTexture = pRenderView->GetDepthTarget();
		CTexture* pColorTexture = pRenderView->GetColorTarget();

		// Opaque forward recursive scene pass
		m_forwardOpaqueRecursivePass.SetViewport(viewport);
		m_forwardOpaqueRecursivePass.SetRenderTargets(
			// Depth
			pDepthTexture,
			// Color 0
			pColorTexture
		);

		// Overlay forward recursive scene pass
		m_forwardOverlayRecursivePass.SetViewport(viewport);
		m_forwardOverlayRecursivePass.SetRenderTargets(
			// Depth
			pDepthTexture,
			// Color 0
			pColorTexture
		);

		m_forwardTransparentRecursivePass.SetViewport(viewport);
		m_forwardTransparentRecursivePass.SetRenderTargets(
			// Depth
			pDepthTexture,
			// Color 0
			pColorTexture
		);
	}
#endif

	// Information flow:
	//  CRendererResources::s_ptexHDRTarget ->                                PostFX HDR
	//  CRendererResources::s_ptexDisplayTarget ->       After PostFX HDR and PostFX LDR
	//  gcpRendD3D->GetCurrentTargetOutput()             After PostFX LDR

	if (!CRendererCVars::CV_r_HDRTexFormat)
		CClearSurfacePass::Execute(CRendererResources::s_ptexLinearDepthFixup, Clr_Empty);
}

bool CSceneForwardStage::CreatePipelineState(const SGraphicsPipelineStateDescription& desc,
                                             CDeviceGraphicsPSOPtr& outPSO,
                                             EPass passId,
                                             const std::function<void(CDeviceGraphicsPSODesc& psoDesc, const SGraphicsPipelineStateDescription& desc)> &customState)
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	outPSO = nullptr;

	const bool bRecursive = (passId == ePass_ForwardRecursive);	
	const bool bMobile    = (passId == ePass_ForwardMobile);

	const uint64 objectFlags = desc.objectFlags;
	CSceneRenderPass* pSceneRenderPass = 
		 bRecursive ? &m_forwardTransparentRecursivePass : 
		(bMobile    ? &m_forwardTransparentPassMobile    : 
			          &m_forwardTransparentBWPass);

	CDeviceGraphicsPSODesc psoDesc(nullptr, desc);
	
	if (bRecursive)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_SECONDARY_VIEW];

	if (customState)
	{
		customState(psoDesc, desc);

		if (bRecursive)
		{
			// NOTE: Only CREParticle and CREFogVolume use custom-state.
			//       CREParticle is rendered for recursive pass without shadow and depth buffer access.
			//       CREFogVolume isn't rendered for recursive pass.
			psoDesc.m_ShaderFlags_RT &= ~(g_HWSR_MaskBit[HWSR_PARTICLE_SHADOW] | g_HWSR_MaskBit[HWSR_SOFT_PARTICLE]);
		}
	}
	else
	{
		const int32 shaderFlags = ((CShader*)desc.shaderItem.m_pShader)->GetFlags();
		const int32 shaderFlags2 = ((CShader*)desc.shaderItem.m_pShader)->GetFlags2();
		const bool bAlphaBlended = desc.shaderItem.m_pShaderResources && desc.shaderItem.m_pShaderResources->IsTransparent();
		const bool bOpaquePass = !bAlphaBlended && (desc.shaderItem.m_nPreprocessFlags & FB_TILED_FORWARD);
		const bool bOverlay = (objectFlags & (FOB_TERRAIN_LAYER | FOB_DECAL)) || (shaderFlags & EF_DECAL);
		const bool bHair = (shaderFlags2 & EF2_HAIR) != 0;
		const bool bEyeOverlay = (shaderFlags2 & EF2_EYE_OVERLAY) != 0;
		const bool bAfterHDRPostProcess = (shaderFlags2 & EF2_AFTERHDRPOSTPROCESS) != 0;
		const bool bAfterLDRPostProcess = (shaderFlags2 & EF2_AFTERPOSTPROCESS) != 0;
		const bool bEmissive = (desc.shaderItem.m_pShaderResources && desc.shaderItem.m_pShaderResources->IsEmissive());

#if !RENDERER_ENABLE_FULL_PIPELINE
		// HACK: only opaque forward is supported on mobile currently
		if (!(bOpaquePass && passId == ePass_ForwardMobile))
			return true;
#endif

		if (!GetStdGraphicsPipeline().FillCommonScenePassStates(desc, psoDesc))
			return true;

		if (bRecursive)
		{
			if (!(objectFlags & FOB_NO_FOG))
				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_FOG];

			if (bOpaquePass)
				pSceneRenderPass = &m_forwardOpaqueRecursivePass;
		}
		else if (bOpaquePass)
		{
			// Disable alpha testing/depth writes if object renders using a z-prepass and requested technique is not z prepass
			if (passId == ePass_ForwardPrepassed)
			{
				psoDesc.m_RenderState &= ~(GS_STENCIL | GS_DEPTHWRITE | GS_DEPTHFUNC_MASK | GS_ALPHATEST);
				psoDesc.m_RenderState |= GS_DEPTHFUNC_EQUAL;
				psoDesc.m_ShaderFlags_RT &= ~(g_HWSR_MaskBit[HWSR_ALPHATEST] | g_HWSR_MaskBit[HWSR_DISSOLVE]);
			}
			else if (passId == ePass_Forward)
			{
				if (CRenderer::CV_r_DeferredShadingTiled > CTiledShadingStage::eDeferredMode_Off)
					psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_TILED_SHADING];

				pSceneRenderPass = &m_forwardOpaquePass;
			}
			else if (passId == ePass_ForwardMobile)
			{
				pSceneRenderPass = &m_forwardOpaquePassMobile;
			}
		}
		else
		{
			if (!bOverlay && !(objectFlags & FOB_NO_FOG))
			{
				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_FOG];

				// volumetric fog supports only general pass currently.
				if (pRenderer->m_bVolumetricFogEnabled)
				{
					psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_VOLUMETRIC_FOG];
				}
			}

			if (CRenderer::CV_r_DeferredShadingTiled > CTiledShadingStage::eDeferredMode_Off)
				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_TILED_SHADING];
		}

		if (bRecursive && (bHair || bAfterHDRPostProcess || bAfterLDRPostProcess || bEyeOverlay))
		{
			// recursive pass doesn't support EF2_HAIR and EF2_AFTERHDRPOSTPROCESS/EF2_AFTERPOSTPROCESS.
			return true;
		}
		else if (bHair)
		{
			psoDesc.m_ShaderFlags_RT &= ~(g_HWSR_MaskBit[HWSR_TILED_SHADING] |g_HWSR_MaskBit[HWSR_QUALITY1]);
			if (CRenderer::CV_r_DeferredShadingTiled > CTiledShadingStage::eDeferredMode_Off &&
				CRenderer::CV_r_DeferredShadingTiledHairQuality > 0)
			{
				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_TILED_SHADING];

				if (CRenderer::CV_r_DeferredShadingTiledHairQuality > 1)
					psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_QUALITY1];
			}

			psoDesc.m_RenderState = (psoDesc.m_RenderState & ~(GS_BLEND_MASK | GS_DEPTHWRITE | GS_DEPTHFUNC_MASK | GS_STENCIL));
			if ((shaderFlags2 & EF2_DEPTH_FIXUP) && CRendererCVars::CV_r_HDRTexFormat)  // Thin hair feature enabled
			{
				psoDesc.m_RenderState |= GS_DEPTHFUNC_LEQUAL;
				psoDesc.m_RenderState |= GS_BLSRC_SRC1ALPHA | GS_BLDST_ONEMINUSSRC1ALPHA | GS_BLALPHA_MIN;
			}
			else
			{
				psoDesc.m_RenderState |= GS_DEPTHFUNC_EQUAL;
			}

			if ((shaderFlags2 & EF2_DEPTH_FIXUP) && !CRendererCVars::CV_r_HDRTexFormat)
				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];

			pSceneRenderPass = &m_forwardTransparentBWPass;
		}
		else if (bAlphaBlended || bOverlay || bEmissive)
		{
			if (!bAlphaBlended && bEmissive)
			{
				// Handle emissive materials
				psoDesc.m_RenderState = (psoDesc.m_RenderState & ~(GS_BLEND_MASK | GS_DEPTHWRITE | GS_DEPTHFUNC_MASK | GS_STENCIL));
				psoDesc.m_RenderState |= GS_DEPTHFUNC_LEQUAL;
				if (!bRecursive)
					psoDesc.m_RenderState |= GS_BLSRC_ONE | GS_BLDST_ONE | GS_NOCOLMASK_A;
			}
			else
			{
				psoDesc.m_RenderState = (psoDesc.m_RenderState & ~(GS_BLEND_MASK | GS_DEPTHWRITE | GS_DEPTHFUNC_MASK | GS_STENCIL));

				if ((shaderFlags2 & EF2_DEPTH_FIXUP) && CRendererCVars::CV_r_HDRTexFormat)
				{
					psoDesc.m_RenderState |= GS_DEPTHFUNC_LEQUAL;
					if (shaderFlags2 & EF2_DEPTH_FIXUP_REPLACE)
					{
						psoDesc.m_RenderState |= GS_BLSRC_SRC1ALPHA_A_ONE | GS_BLDST_ONEMINUSSRC1ALPHA_A_ZERO; // == "Replace alpha"
					}
					else
					{
						psoDesc.m_RenderState |= GS_BLSRC_SRC1ALPHA | GS_BLDST_ONEMINUSSRC1ALPHA | GS_BLALPHA_MIN;
					}
				}
				else
				{
					psoDesc.m_RenderState |= GS_DEPTHFUNC_LEQUAL | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NOCOLMASK_A;
				}

				if ((shaderFlags2 & EF2_DEPTH_FIXUP) && !CRendererCVars::CV_r_HDRTexFormat)
					psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];

				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_ALPHABLEND];
			}

			if (bOverlay)
			{
				if (passId == ePass_Forward)
				{
					pSceneRenderPass = bRecursive ? &m_forwardOverlayRecursivePass : &m_forwardOverlayPass;
				}
				else if (passId == ePass_ForwardMobile)
				{
					pSceneRenderPass = &m_forwardOverlayPassMobile;
				}
			}
		}
		else if (bEyeOverlay)
		{
			pSceneRenderPass = &m_forwardEyeOverlayPass;
		}
		else if (bAfterHDRPostProcess)
		{
			psoDesc.m_CullMode = eCULL_None;
			pSceneRenderPass = &m_forwardHDRPass;
		}
		else if (bAfterLDRPostProcess)
		{
			psoDesc.m_CullMode = eCULL_None;
			pSceneRenderPass = &m_forwardLDRPass;
		}
	}

	{
		psoDesc.m_RenderState |= ReverseDepthHelper::ConvertDepthFunc(psoDesc.m_RenderState);
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_REVERSE_DEPTH];
	}

	psoDesc.m_pResourceLayout = pSceneRenderPass->GetResourceLayout();
	psoDesc.m_pRenderPass     = pSceneRenderPass->GetRenderPass();

	if (!psoDesc.m_pResourceLayout || !psoDesc.m_pRenderPass)
		return false;

	outPSO = GetDeviceObjectFactory().CreateGraphicsPSO(psoDesc);
	return outPSO != nullptr;
}

bool CSceneForwardStage::CreatePipelineStates(DevicePipelineStatesArray* pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache)
{
	DevicePipelineStatesArray& stageStates = pStateArray[m_stageID];

	if (pStateCache->Find(stateDesc, stageStates))
		return true;

	bool bFullyCompiled = true;

	SGraphicsPipelineStateDescription _stateDesc = stateDesc;

	_stateDesc.technique = TTYPE_GENERAL;
#if RENDERER_ENABLE_FULL_PIPELINE
	bFullyCompiled &= CreatePipelineState(_stateDesc, stageStates[ePass_Forward         ], ePass_Forward         );
	bFullyCompiled &= CreatePipelineState(_stateDesc, stageStates[ePass_ForwardPrepassed], ePass_ForwardPrepassed);
	bFullyCompiled &= CreatePipelineState(_stateDesc, stageStates[ePass_ForwardRecursive], ePass_ForwardRecursive);
#endif

#if RENDERER_ENABLE_MOBILE_PIPELINE
	bFullyCompiled &= CreatePipelineState(_stateDesc, stageStates[ePass_ForwardMobile   ], ePass_ForwardMobile   );
#endif

	if (bFullyCompiled)
	{
		pStateCache->Put(stateDesc, stageStates);
	}

	return bFullyCompiled;
}

bool CSceneForwardStage::PreparePerPassResources(bool bOnInit, bool bShadowMask, bool bFog)
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	CRenderView* pRenderView = RenderView();

	auto* pClipVolumes = GetStdGraphicsPipeline().GetClipVolumesStage();
	auto* pTiledLights = GetStdGraphicsPipeline().GetTiledLightVolumesStage();
	auto* pFogStage    = GetStdGraphicsPipeline().GetFogStage();
	auto* pVolFogStage = GetStdGraphicsPipeline().GetVolumetricFogStage();

#if defined(FEATURE_SVO_GI)
	auto* pSVOGIStage  = CSvoRenderer::GetInstance();
#endif

	CTexture* pShadowMask = bShadowMask ? CRendererResources::s_ptexShadowMask : CRendererResources::s_ptexBlack;

	enum EResourcesSubset
	{
		eResSubset_General                 = BIT(0),
		eResSubset_TiledShadingOpaque      = BIT(1),
		eResSubset_TiledShadingTransparent = BIT(2),
		eResSubset_Particles               = BIT(3),
		eResSubset_EyeOverlay              = BIT(4),
		eResSubset_ForwardShadows          = BIT(5),

		eResSubset_All                     = eResSubset_General   | eResSubset_TiledShadingOpaque | eResSubset_TiledShadingTransparent | 
		                                     eResSubset_Particles | eResSubset_EyeOverlay         | eResSubset_ForwardShadows,
		eResSubset_None                    = ~eResSubset_All
	};
	

	struct { CDeviceResourceSetDesc& resourceDesc; CDeviceResourceSet*  pResourceSet; uint32_t flags; } resourceSetsToBuild[] =
	{
#if RENDERER_ENABLE_FULL_PIPELINE
		{ m_opaquePassResources,            m_pOpaquePassResourceSet.get(),            eResSubset_General | eResSubset_TiledShadingOpaque      | eResSubset_Particles },
		{ m_transparentPassResources,       m_pTransparentPassResourceSet.get(),       eResSubset_General | eResSubset_TiledShadingTransparent | eResSubset_Particles | eResSubset_ForwardShadows },
		{ m_eyeOverlayPassResources,	    m_pEyeOverlayPassResourceSet.get(),        eResSubset_General | eResSubset_EyeOverlay              | eResSubset_Particles | eResSubset_ForwardShadows },
#endif

#if RENDERER_ENABLE_MOBILE_PIPELINE
		{ m_opaquePassResourcesMobile,      m_pOpaquePassResourceSetMobile.get(),      eResSubset_General | eResSubset_Particles},
		{ m_transparentPassResourcesMobile, m_pTransparentPassResourceSetMobile.get(), eResSubset_General | eResSubset_Particles},
#endif
	};

	if (!bOnInit)
	{
		PREFAST_SUPPRESS_WARNING(6263)
		CryStackAllocWithSizeCleared(SPerPassConstantBuffer, cb, CDeviceBufferManager::AlignBufferSizeForStreaming);

		if (pRenderView)
			CShadowUtils::GetShadowCascadesSamplingInfo(cb->cbShadowSampling, pRenderView);

		FillCloudShadingParams(cb->cbClouds, true);

		if (pFogStage)    pFogStage   ->FillForwardParams(cb->cbFog, bFog);
		if (pVolFogStage) pVolFogStage->FillForwardParams(cb->cbVoxelFog, bFog);

#if defined(FEATURE_SVO_GI)
		if (auto pSVOGIStage = CSvoRenderer::GetInstance())
		{
			pSVOGIStage->FillForwardParams(cb->cbSVOGI, pSVOGIStage->IsActive());
		}
#endif

		m_pPerPassCB->UpdateBuffer(cb, cbSize);
	}

	for (uint32 i = 0; i < CRY_ARRAY_COUNT(resourceSetsToBuild); i++)
	{

		CDeviceResourceSetDesc& resources      = resourceSetsToBuild[i].resourceDesc;
		CDeviceResourceSet*     pResourceSet   = resourceSetsToBuild[i].pResourceSet;
		const uint32_t&         resourceSubset = resourceSetsToBuild[i].flags;

		const bool includeOpaquePassResources      = !!(resourceSubset & eResSubset_TiledShadingOpaque);
		const bool includeTransparentPassResources = !!(resourceSubset & eResSubset_TiledShadingTransparent);
		const bool includeEyeOverlayPassResources  = !!(resourceSubset & eResSubset_EyeOverlay);
		const bool includeForwardShadowResources   = !!(resourceSubset & eResSubset_ForwardShadows);

		// Samplers
		{
			auto materialSamplers = gcpRendD3D->GetGraphicsPipeline().GetDefaultMaterialSamplers();
			for (int i = 0; i < materialSamplers.size(); ++i)
			{
				resources.SetSampler(EEfResSamplers(i), materialSamplers[i], EShaderStage_AllWithoutCompute);
			}
			
			resources.SetSampler(8, EDefaultSamplerStates::PointWrap, EShaderStage_AllWithoutCompute);
			resources.SetSampler(9, EDefaultSamplerStates::PointClamp, EShaderStage_AllWithoutCompute);
			
			// Custom for pass
			resources.SetSampler(10, EDefaultSamplerStates::BilinearWrap, EShaderStage_AllWithoutCompute);
			resources.SetSampler(11, EDefaultSamplerStates::LinearCompare, EShaderStage_AllWithoutCompute);
		}

		// Textures
		{
			int nTerrainTex0 = 0, nTerrainTex1 = 0, nTerrainTex2 = 0;
			if (gEnv->p3DEngine && gEnv->p3DEngine->GetITerrain())
				gEnv->p3DEngine->GetITerrain()->GetAtlasTexId(nTerrainTex0, nTerrainTex1, nTerrainTex2);

			resources.SetTexture(ePerPassTexture_PerlinNoiseMap, CRendererResources::s_ptexPerlinNoiseMap, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			resources.SetTexture(ePerPassTexture_WindGrid, CRendererResources::s_ptexWindGrid, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			resources.SetTexture(ePerPassTexture_TerrainElevMap, CTexture::GetByID(nTerrainTex2), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			resources.SetTexture(ePerPassTexture_TerrainNormMap, CTexture::GetByID(nTerrainTex1), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			resources.SetTexture(ePerPassTexture_TerrainBaseMap, CTexture::GetByID(nTerrainTex0), EDefaultResourceViews::sRGB, EShaderStage_AllWithoutCompute);
			resources.SetTexture(ePerPassTexture_NormalsFitting, CRendererResources::s_ptexNormalsFitting, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			resources.SetTexture(ePerPassTexture_SceneLinearDepth, CRendererResources::s_ptexLinearDepth, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);

			resources.SetTexture(38, pShadowMask, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			resources.SetTexture(39, CRendererResources::s_ptexNoise3D, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			resources.SetTexture(40, CRendererResources::s_ptexEnvironmentBRDF, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			resources.SetTexture(45, (pRenderView && pRenderView->IsRecursive()) ? CRendererResources::s_ptexBlackCM : CRendererResources::s_ptexDefaultProbeCM, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);

			if (includeTransparentPassResources) // Transparent pass only: allow writing motion vectors
			{
				resources.SetTexture(2, CRendererResources::s_ptexVelocityObjects[0], EDefaultResourceViews::UnorderedAccess, EShaderStage_Pixel);
				if (!CRendererCVars::CV_r_HDRTexFormat)
					resources.SetTexture(3, CRendererResources::s_ptexLinearDepthFixup, CRendererResources::s_ptexLinearDepthFixupUAV, EShaderStage_Pixel);
			}			
		}

		// Particle resources
		if (resourceSubset & EResourcesSubset::eResSubset_Particles)
		{
			if (bOnInit)
			{
				resources.SetBuffer(
					EReservedTextureSlot_LightvolumeInfos,
					CDeviceBufferManager::GetNullBufferStructured(),
					EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetBuffer(
					EReservedTextureSlot_LightVolumeRanges,
					CDeviceBufferManager::GetNullBufferStructured(),
					EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);

				resources.SetBuffer(
					EReservedTextureSlot_ParticlePositionStream,
					CDeviceBufferManager::GetNullBufferStructured(),
					EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetBuffer(
					EReservedTextureSlot_ParticleAxesStream,
					CDeviceBufferManager::GetNullBufferStructured(),
					EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetBuffer(
					EReservedTextureSlot_ParticleColorSTStream,
					CDeviceBufferManager::GetNullBufferStructured(),
					EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			}
			else
			{
				const CLightVolumeBuffer& lightVolumes = GetStdGraphicsPipeline().GetLightVolumeBuffer();
				const CParticleBufferSet& particleBuffer = GetStdGraphicsPipeline().GetParticleBufferSet();

				resources.SetBuffer(
					EReservedTextureSlot_LightvolumeInfos,
					const_cast<CGpuBuffer*>(&lightVolumes.GetLightInfosBuffer()),
					EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetBuffer(
					EReservedTextureSlot_LightVolumeRanges,
					const_cast<CGpuBuffer*>(&lightVolumes.GetLightRangesBuffer()),
					EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);

				const auto positionStream = particleBuffer.GetPositionStream(RenderView()->GetFrameId());
				const auto axesStream     = particleBuffer.GetAxesStream(RenderView()->GetFrameId());
				const auto colorStream    = particleBuffer.GetColorSTsStream(RenderView()->GetFrameId());
				if (positionStream && axesStream && colorStream)
				{
					resources.SetBuffer(
						EReservedTextureSlot_ParticlePositionStream,
						const_cast<CGpuBuffer*>(positionStream),
						EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
					resources.SetBuffer(
						EReservedTextureSlot_ParticleAxesStream,
						const_cast<CGpuBuffer*>(axesStream),
						EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
					resources.SetBuffer(
						EReservedTextureSlot_ParticleColorSTStream,
						const_cast<CGpuBuffer*>(colorStream),
						EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				}
			}
		}

		// Tiled shading resources
		{
			if (bOnInit)
			{
				resources.SetBuffer(17, CDeviceBufferManager::GetNullBufferTyped(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetBuffer(18, CDeviceBufferManager::GetNullBufferStructured(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetBuffer(19, CDeviceBufferManager::GetNullBufferStructured(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetTexture(20, CRendererResources::s_ptexBlackCM, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetTexture(21, CRendererResources::s_ptexBlackCM, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetTexture(22, CRendererResources::s_ptexBlack, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);

				if (includeTransparentPassResources || !pTiledLights->IsSeparateVolumeListGen() || (CRendererCVars::CV_r_GraphicsPipelineMobile > 0))
				{
					resources.SetBuffer(17, CDeviceBufferManager::GetNullBufferTyped(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				}
			}
			else
			{
				resources.SetBuffer(17, pTiledLights->GetTiledOpaqueLightMaskBuffer(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetBuffer(18, pTiledLights->GetLightShadeInfoBuffer(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetBuffer(19, pClipVolumes->GetClipVolumeInfoBuffer(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetTexture(20, pTiledLights->GetSpecularProbeAtlas(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetTexture(21, pTiledLights->GetDiffuseProbeAtlas(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetTexture(22, pTiledLights->GetProjectedLightAtlas(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);

				if (includeTransparentPassResources || !pTiledLights->IsSeparateVolumeListGen() || (CRendererCVars::CV_r_GraphicsPipelineMobile > 0))
				{
					resources.SetBuffer(17, pTiledLights->GetTiledTranspLightMaskBuffer(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				}
			}

#if defined(FEATURE_SVO_GI)
			if (bOnInit || !CSvoRenderer::GetInstance()->IsActive() || !CSvoRenderer::GetInstance()->GetSpecularFinRT())
			{
				resources.SetTexture(46, CRendererResources::s_ptexBlack, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetTexture(47, CRendererResources::s_ptexBlack, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			}
			else
			{
				resources.SetTexture(46, CSvoRenderer::GetInstance()->GetDiffuseFinRT(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetTexture(47, CSvoRenderer::GetInstance()->GetSpecularFinRT(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			}
#endif

			resources.SetTexture(23, CRendererResources::s_ptexRT_ShadowPool, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			resources.SetTexture(24, CRendererResources::s_ptexSceneNormalsBent, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			resources.SetTexture(41, CRendererResources::s_ptexSceneDiffuse, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);  //  Eye AO overlay

			// Overwrite resources for transparent pass (need to be careful that the layout is still the same)
			if (includeForwardShadowResources)
			{
				resources.SetTexture(24, CRendererResources::s_ptexShadowJitterMap, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);

				CShadowUtils::SShadowCascades cascades;
				if (bOnInit)
				{
					std::fill(std::begin(cascades.pShadowMap), std::end(cascades.pShadowMap), CRendererResources::s_ptexFarPlane);
				}
				else
				{
					CShadowUtils::GetShadowCascades(cascades, RenderView());
				}

				resources.SetTexture(25, CRendererResources::s_ptexSceneTarget, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetTexture(26, cascades.pShadowMap[0], EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetTexture(27, cascades.pShadowMap[1], EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetTexture(28, cascades.pShadowMap[2], EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetTexture(29, cascades.pShadowMap[3], EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetTexture(30, CRendererResources::s_ptexWhite, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				resources.SetTexture(31, CRendererResources::s_ptexShadowJitterMap, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			}

			if (includeTransparentPassResources)
			{
				// volumetric fog supports only general pass currently so only transparent pass needs those textures.
				if (bOnInit || !pVolFogStage || !bFog)
				{
					resources.SetTexture(42, CRendererResources::s_ptexBlack, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
					resources.SetTexture(43, CRendererResources::s_ptexBlackCM, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
					resources.SetTexture(44, CRendererResources::s_ptexBlackCM, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				}
				else
				{
					resources.SetTexture(42, pVolFogStage->GetVolumetricFogTex(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
					resources.SetTexture(43, pVolFogStage->GetGlobalEnvProbeTex0(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
					resources.SetTexture(44, pVolFogStage->GetGlobalEnvProbeTex1(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				}
			}
			else if (includeEyeOverlayPassResources)
			{
				// Eye AO overlay pass resource must not contain eye AO overlay texture.
				resources.SetTexture(41, CRendererResources::s_ptexBlack, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			}
		}

		// Constant buffers
		{
			CConstantBufferPtr pPerPassCB;
			CConstantBufferPtr pPerViewCB;

			// Handle case when no view is available in the initialization of the stage
			if (bOnInit)
			{
				pPerPassCB = CDeviceBufferManager::GetNullConstantBuffer();
				pPerViewCB = CDeviceBufferManager::GetNullConstantBuffer();
			}
			else
			{
				pPerPassCB = m_pPerPassCB;
				pPerViewCB = GetStdGraphicsPipeline().GetMainViewConstantBuffer();
			}

			resources.SetConstantBuffer(eConstantBufferShaderSlot_PerPass, pPerPassCB, EShaderStage_AllWithoutCompute);
			resources.SetConstantBuffer(eConstantBufferShaderSlot_PerView, pPerViewCB, EShaderStage_AllWithoutCompute);
		}

		if (bOnInit)
			continue;

		CRY_ASSERT(!resources.HasChangedBindPoints()); // Cannot change resource layout after init. It is baked into the shaders
		pResourceSet->Update(resources);
	}

	bool allResourceSetsValid = true;
	for (int i = 0; i < CRY_ARRAY_COUNT(resourceSetsToBuild); ++i)
		allResourceSetsValid &= resourceSetsToBuild[i].pResourceSet->IsValid();

	return bOnInit || allResourceSetsValid;
}

void CSceneForwardStage::ExecuteOpaque()
{
	FUNCTION_PROFILER_RENDERER();
	PROFILE_LABEL_SCOPE("OPAQUE");

	CRenderView* pRenderView = RenderView();
	auto& renderItemDrawer = pRenderView->GetDrawer();

	PreparePerPassResources(false);

	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

	m_forwardEyeOverlayPass.PrepareRenderPassForUse(commandList);
	m_forwardOpaquePass.PrepareRenderPassForUse(commandList);
	m_forwardOverlayPass.PrepareRenderPassForUse(commandList);

	const uint32 bForwardFilter = CRenderer::CV_r_DeferredShadingTiled != CTiledShadingStage::eDeferredMode_Disabled ? FB_Z : 0;

	CZPrePassPredicate zpPredicate;
	int nearestFNum    = pRenderView->GetRenderItems(EFSLIST_FORWARD_OPAQUE_NEAREST).size();
	int nearestFNoZPre = pRenderView->FindRenderListSplit(zpPredicate, EFSLIST_FORWARD_OPAQUE_NEAREST, 0, nearestFNum);

	int generalFNum    = pRenderView->GetRenderItems(EFSLIST_FORWARD_OPAQUE).size();
	int generalFNoZPre = pRenderView->FindRenderListSplit(zpPredicate, EFSLIST_FORWARD_OPAQUE, 0, generalFNum);
	
	int nearestGNum    = pRenderView->GetRenderItems(EFSLIST_NEAREST_OBJECTS).size();
	int nearestGNoZPre = pRenderView->FindRenderListSplit(zpPredicate, EFSLIST_NEAREST_OBJECTS, 0, nearestGNum);

	int generalGNum    = pRenderView->GetRenderItems(EFSLIST_GENERAL).size();
	int generalGNoZPre = pRenderView->FindRenderListSplit(zpPredicate, EFSLIST_GENERAL, 0, generalGNum);

	{
		renderItemDrawer.InitDrawSubmission();

		m_forwardEyeOverlayPass.BeginExecution();
		m_forwardEyeOverlayPass.SetupDrawContext(m_stageID, ePass_Forward, TTYPE_GENERAL, FB_GENERAL);
		m_forwardEyeOverlayPass.DrawRenderItems(pRenderView, EFSLIST_EYE_OVERLAY);
		m_forwardEyeOverlayPass.EndExecution();

		m_forwardOpaquePass.BeginExecution();
		m_forwardOpaquePass.SetupDrawContext(m_stageID, ePass_Forward, TTYPE_GENERAL, FB_GENERAL | FB_TILED_FORWARD, FB_ZPREPASS);
		m_forwardOpaquePass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE_NEAREST, 0, nearestFNoZPre);
		if (CRenderer::CV_r_DeferredShadingTiled == CTiledShadingStage::eDeferredMode_Disabled)
			m_forwardOpaquePass.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS, 0, nearestGNoZPre);
		m_forwardOpaquePass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE, 0, generalFNoZPre);
		if (CRenderer::CV_r_DeferredShadingTiled == CTiledShadingStage::eDeferredMode_Disabled)
			m_forwardOpaquePass.DrawRenderItems(pRenderView, EFSLIST_GENERAL, 0, generalGNoZPre);
		m_forwardOpaquePass.EndExecution();

		m_forwardOpaquePass.BeginExecution();
		m_forwardOpaquePass.SetupDrawContext(m_stageID, ePass_ForwardPrepassed, TTYPE_GENERAL, FB_GENERAL | FB_TILED_FORWARD | FB_ZPREPASS);
		m_forwardOpaquePass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE_NEAREST, nearestFNoZPre, nearestFNum);
		if (CRenderer::CV_r_DeferredShadingTiled == CTiledShadingStage::eDeferredMode_Disabled)
			m_forwardOpaquePass.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS, nearestGNoZPre, nearestGNum);
		m_forwardOpaquePass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE, generalFNoZPre, generalFNum);
		if (CRenderer::CV_r_DeferredShadingTiled == CTiledShadingStage::eDeferredMode_Disabled)
			m_forwardOpaquePass.DrawRenderItems(pRenderView, EFSLIST_GENERAL, generalGNoZPre, generalGNum);
		m_forwardOpaquePass.EndExecution();

		m_forwardOverlayPass.BeginExecution();
		m_forwardOverlayPass.SetupDrawContext(m_stageID, ePass_ForwardPrepassed, TTYPE_GENERAL, FB_GENERAL | FB_TILED_FORWARD, bForwardFilter);
		m_forwardOverlayPass.DrawRenderItems(pRenderView, EFSLIST_TERRAINLAYER);
		m_forwardOverlayPass.DrawRenderItems(pRenderView, EFSLIST_DECAL);
		m_forwardOverlayPass.DrawRenderItems(pRenderView, EFSLIST_SKY);
		m_forwardOverlayPass.EndExecution();

		renderItemDrawer.JobifyDrawSubmission();
		renderItemDrawer.WaitForDrawSubmission();
	}
}

void CSceneForwardStage::ExecuteTransparent(bool bBelowWater)
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	CRenderView* pRenderView = RenderView();

	CSceneRenderPass& scenePass = bBelowWater ? m_forwardTransparentBWPass : m_forwardTransparentAWPass;

	PreparePerPassResources(false);

	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

	scenePass.PrepareRenderPassForUse(commandList);

	auto& renderItemDrawer = pRenderView->GetDrawer();
	renderItemDrawer.InitDrawSubmission();

	scenePass.BeginExecution();
	scenePass.SetupDrawContext(m_stageID, ePass_Forward, TTYPE_GENERAL, FB_GENERAL);
	scenePass.DrawRenderItems(pRenderView, bBelowWater ? EFSLIST_TRANSP_BW : EFSLIST_TRANSP_AW);
	if (!bBelowWater)
	{
		scenePass.DrawRenderItems(pRenderView, EFSLIST_TRANSP_NEAREST);
	}
	scenePass.EndExecution();

	renderItemDrawer.JobifyDrawSubmission();
	renderItemDrawer.WaitForDrawSubmission();
}

void CSceneForwardStage::ExecuteTransparentBelowWater()
{
	FUNCTION_PROFILER_RENDERER();

	ExecuteTransparent(true);
}

void CSceneForwardStage::ExecuteTransparentAboveWater()
{
	FUNCTION_PROFILER_RENDERER();

	ExecuteTransparent(false);
}

void CSceneForwardStage::ExecuteTransparentDepthFixup()
{
	FUNCTION_PROFILER_RENDERER();
	PROFILE_LABEL_SCOPE("MERGE_DEPTH");

	CTexture* pSrcRT  = CRendererResources::s_ptexHDRTarget;
	CTexture* pDestRT = CRendererResources::s_ptexLinearDepth;

	if (!CRendererCVars::CV_r_HDRTexFormat)
		pSrcRT = CRendererResources::s_ptexLinearDepthFixup;

	CFullscreenPass& screenPass = m_depthFixupPass;
	if (!screenPass.IsDirty())
	{
		screenPass.Execute();
		return;
	}

	static CCryNameTSCRC techName("TranspDepthFixupMerge");

	uint64 rtMask = !CRendererCVars::CV_r_HDRTexFormat ? g_HWSR_MaskBit[HWSR_SAMPLE1] : 0;

	screenPass.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
	screenPass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
	screenPass.SetRenderTarget(0, pDestRT);
	screenPass.SetTechnique(CShaderMan::s_shPostEffects, techName, rtMask);
	screenPass.SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE | GS_BLEND_OP_MIN);
	screenPass.SetRequirePerViewConstantBuffer(true);
	screenPass.SetTexture(0, pSrcRT);
	screenPass.BeginConstantUpdate();
	screenPass.Execute();
}

void CSceneForwardStage::ExecuteTransparentLoRes(int subRes)
{
	FUNCTION_PROFILER_RENDERER();

	CRenderView* pRenderView = RenderView();
	if (pRenderView->GetRenderItems(EFSLIST_HALFRES_PARTICLES).empty())
		return;

	PROFILE_LABEL_SCOPE("FORWARD_TRANSPARENT_SUBRES");

	CTexture* pSourceDS = CRendererResources::s_ptexLinearDepthScaled[subRes];
	CTexture* pTargetDS = CRendererResources::s_ptexSceneDepthScaled[subRes];
	CTexture* pTargetRT = CRendererResources::s_ptexHDRTargetScaled[subRes][0];

	CClearSurfacePass::Execute(pTargetRT, Clr_Empty);

	{
		PROFILE_LABEL_SCOPE("COPY_DEPTH_HALF");

		static CCryNameTSCRC techCopy("CopyToDeviceDepth");

		m_depthCopyPass.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
		m_depthCopyPass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		m_depthCopyPass.SetTechnique(CShaderMan::s_shPostEffects, techCopy, 0);
		m_depthCopyPass.SetRequirePerViewConstantBuffer(true);
		m_depthCopyPass.SetDepthTarget(pTargetDS);
		m_depthCopyPass.SetState(GS_DEPTHWRITE | GS_DEPTHFUNC_NOTEQUAL);
		m_depthCopyPass.SetTexture(0, pSourceDS);

		m_depthCopyPass.BeginConstantUpdate();
		m_depthCopyPass.Execute();
	}

	CSceneRenderPass& scenePass = m_forwardTransparentLoResPass;//[subRes];
	scenePass.ExchangeRenderTarget(0, pTargetRT);
	scenePass.ExchangeDepthTarget(pTargetDS);

	PreparePerPassResources(false);

	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

	scenePass.PrepareRenderPassForUse(commandList);

	auto& renderItemDrawer = pRenderView->GetDrawer();
	renderItemDrawer.InitDrawSubmission();

	scenePass.BeginExecution();
	scenePass.SetupDrawContext(m_stageID, ePass_Forward, TTYPE_GENERAL, FB_GENERAL);
	scenePass.DrawRenderItems(pRenderView, EFSLIST_HALFRES_PARTICLES);
	scenePass.EndExecution();

	renderItemDrawer.JobifyDrawSubmission();
	renderItemDrawer.WaitForDrawSubmission();

	m_depthUpscalePass.Execute(
		CRendererResources::s_ptexLinearDepth, // TODO: Scaled[subRes - 1]
		pTargetRT,
		pSourceDS,
		CRendererResources::s_ptexHDRTarget, // TODO: Scaled[subRes - 1]
		CRendererCVars::CV_r_ParticlesHalfResBlendMode == 0
	);
}

void CSceneForwardStage::ExecuteAfterPostProcessHDR()
{
	FUNCTION_PROFILER_RENDERER();

	CRenderView* pRenderView = RenderView();
	if (pRenderView->GetRenderItems(EFSLIST_AFTER_HDRPOSTPROCESS).empty())
		return;

	PROFILE_LABEL_SCOPE("POST_EFFECTS_HDR_AP");

	CSceneRenderPass& scenePass = m_forwardHDRPass;
	// TODO: CPostEffectContext::GetDstBackBufferTexture() pre-EnableAltBackBuffer()
	scenePass.ExchangeRenderTarget(0, CRendererResources::s_ptexDisplayTargetDst);

	PreparePerPassResources(false);

	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

	scenePass.PrepareRenderPassForUse(commandList);

	auto& renderItemDrawer = pRenderView->GetDrawer();
	renderItemDrawer.InitDrawSubmission();

	scenePass.BeginExecution();
	scenePass.SetupDrawContext(m_stageID, ePass_Forward, TTYPE_GENERAL, FB_GENERAL);
	scenePass.DrawRenderItems(pRenderView, EFSLIST_AFTER_HDRPOSTPROCESS);
	scenePass.EndExecution();

	renderItemDrawer.JobifyDrawSubmission();
	renderItemDrawer.WaitForDrawSubmission();
}

void CSceneForwardStage::ExecuteAfterPostProcessLDR()
{
	FUNCTION_PROFILER_RENDERER();

	CRenderView* pRenderView = RenderView();
	if (pRenderView->GetRenderItems(EFSLIST_AFTER_POSTPROCESS).empty())
		return;

	PROFILE_LABEL_SCOPE("POST_EFFECTS_LDR_AP");

	CSceneRenderPass& scenePass = m_forwardLDRPass;
	// TODO: CPostEffectContext::GetDstBackBufferTexture() post-EnableAltBackBuffer()
	scenePass.ExchangeRenderTarget(0, RenderView()->GetRenderOutput()->GetColorTarget());

	PreparePerPassResources(false);

	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

	scenePass.PrepareRenderPassForUse(commandList);

	auto& renderItemDrawer = pRenderView->GetDrawer();
	renderItemDrawer.InitDrawSubmission();

	scenePass.BeginExecution();
	scenePass.SetupDrawContext(m_stageID, ePass_Forward, TTYPE_GENERAL, FB_GENERAL);
	scenePass.DrawRenderItems(pRenderView, EFSLIST_AFTER_POSTPROCESS);
	scenePass.EndExecution();

	renderItemDrawer.JobifyDrawSubmission();
	renderItemDrawer.WaitForDrawSubmission();
}

void CSceneForwardStage::ExecuteMobile()
{
	PROFILE_LABEL_SCOPE("FORWARD_MOBILE");

	CRenderView* pRenderView = RenderView();
	auto& renderItemDrawer = pRenderView->GetDrawer();

	D3DViewPort viewport = RenderViewportToD3D11Viewport(pRenderView->GetViewport());

	PreparePerPassResources(false);

	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

	m_forwardOpaquePassMobile.PrepareRenderPassForUse(commandList);
	m_forwardOverlayPassMobile.PrepareRenderPassForUse(commandList);
	m_forwardTransparentPassMobile.PrepareRenderPassForUse(commandList);

	{
		renderItemDrawer.InitDrawSubmission();

		m_forwardOpaquePassMobile.BeginExecution();
		m_forwardOpaquePassMobile.SetupDrawContext(m_stageID, ePass_ForwardMobile, TTYPE_GENERAL, FB_TILED_FORWARD);
		m_forwardOpaquePassMobile.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE_NEAREST);
		if (CRenderer::CV_r_DeferredShadingTiled == CTiledShadingStage::eDeferredMode_Disabled)
			m_forwardOpaquePassMobile.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS);
		m_forwardOpaquePassMobile.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE);
		if (CRenderer::CV_r_DeferredShadingTiled == CTiledShadingStage::eDeferredMode_Disabled)
			m_forwardOpaquePassMobile.DrawRenderItems(pRenderView, EFSLIST_GENERAL);
		m_forwardOpaquePassMobile.EndExecution();

		m_forwardOverlayPassMobile.BeginExecution();
		m_forwardOverlayPassMobile.SetupDrawContext(m_stageID, ePass_ForwardMobile, TTYPE_GENERAL, FB_GENERAL);
		m_forwardOverlayPassMobile.DrawRenderItems(pRenderView, EFSLIST_TERRAINLAYER);
		m_forwardOverlayPassMobile.DrawRenderItems(pRenderView, EFSLIST_DECAL);
		m_forwardOverlayPassMobile.DrawRenderItems(pRenderView, EFSLIST_SKY);
		m_forwardOverlayPassMobile.EndExecution();

		m_forwardTransparentPassMobile.BeginExecution();
		m_forwardTransparentPassMobile.SetupDrawContext(m_stageID, ePass_ForwardMobile, TTYPE_GENERAL, FB_GENERAL);
		m_forwardTransparentPassMobile.DrawRenderItems(pRenderView, EFSLIST_TRANSP_AW);
		m_forwardTransparentPassMobile.DrawRenderItems(pRenderView, EFSLIST_TRANSP_NEAREST);
		m_forwardTransparentPassMobile.EndExecution();

		renderItemDrawer.JobifyDrawSubmission();
		renderItemDrawer.WaitForDrawSubmission();
	}
}

void CSceneForwardStage::ExecuteMinimum(CTexture* pColorTex, CTexture* pDepthTex)
{
	PROFILE_LABEL_SCOPE("FORWARD_MINIMUM");

	CD3D9Renderer* pRenderer = gcpRendD3D;

	CRenderView* pRenderView = RenderView();
	auto& renderItemDrawer = pRenderView->GetDrawer();

	CRY_ASSERT(pRenderer->m_nGraphicsPipeline == 3);

	D3DViewPort viewport = RenderViewportToD3D11Viewport(RenderView()->GetViewport());
	if (pRenderView->IsRecursive())
	{
		viewport = { 0.f, 0.f, float(pColorTex->GetWidth()), float(pColorTex->GetHeight()), 0.0f, 1.0f };
	}

	m_forwardOpaqueRecursivePass.ExchangeRenderTarget(0, pColorTex);
	m_forwardOpaqueRecursivePass.ExchangeDepthTarget(pDepthTex);

	m_forwardOverlayRecursivePass.ExchangeRenderTarget(0, pColorTex);
	m_forwardOverlayRecursivePass.ExchangeDepthTarget(pDepthTex);

	m_forwardTransparentRecursivePass.ExchangeRenderTarget(0, pColorTex);
	m_forwardTransparentRecursivePass.ExchangeDepthTarget(pDepthTex);

	const bool bShadowMask = false;
	const bool bFog = RenderView()->IsGlobalFogEnabled();
	PreparePerPassResources(false, bShadowMask, bFog);

	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

	m_forwardOpaqueRecursivePass.PrepareRenderPassForUse(commandList);
	m_forwardOverlayRecursivePass.PrepareRenderPassForUse(commandList);
	m_forwardTransparentRecursivePass.PrepareRenderPassForUse(commandList);

	{
		renderItemDrawer.InitDrawSubmission();

		m_forwardOpaqueRecursivePass.BeginExecution();
		m_forwardOpaqueRecursivePass.SetupDrawContext(m_stageID, ePass_ForwardRecursive, TTYPE_GENERAL, FB_GENERAL);
		m_forwardOpaqueRecursivePass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE_NEAREST);
		m_forwardOpaqueRecursivePass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE);
		m_forwardOpaqueRecursivePass.EndExecution();

		m_forwardOverlayRecursivePass.BeginExecution();
		m_forwardOverlayRecursivePass.SetupDrawContext(m_stageID, ePass_ForwardRecursive, TTYPE_GENERAL, FB_GENERAL);
		m_forwardOverlayRecursivePass.DrawRenderItems(pRenderView, EFSLIST_TERRAINLAYER);
		m_forwardOverlayRecursivePass.DrawRenderItems(pRenderView, EFSLIST_DECAL);
		m_forwardOverlayRecursivePass.DrawRenderItems(pRenderView, EFSLIST_SKY);
		m_forwardOverlayRecursivePass.EndExecution();

		m_forwardTransparentRecursivePass.BeginExecution();
		m_forwardTransparentRecursivePass.SetupDrawContext(m_stageID, ePass_ForwardRecursive, TTYPE_GENERAL, FB_GENERAL);
		m_forwardTransparentRecursivePass.DrawRenderItems(pRenderView, EFSLIST_TRANSP_BW);
		m_forwardTransparentRecursivePass.DrawRenderItems(pRenderView, EFSLIST_TRANSP_AW);
		m_forwardTransparentRecursivePass.DrawRenderItems(pRenderView, EFSLIST_TRANSP_NEAREST);
		m_forwardTransparentRecursivePass.EndExecution();

		renderItemDrawer.JobifyDrawSubmission();
		renderItemDrawer.WaitForDrawSubmission();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sky
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CSceneForwardStage::SetSkyRE(CRESky* pSkyRE)
{
	m_pSkyRE = pSkyRE;
}

void CSceneForwardStage::SetSkyRE(CREHDRSky* pHDRSkyRE)
{
	m_pHDRSkyRE = pHDRSkyRE;
}

void CSceneForwardStage::SetSkyMat(IMaterial* pMat)
{
	m_pSkyMat = pMat;
}

void CSceneForwardStage::SetSkyParameters()
{
	if (!m_pSkyRE)
		return;

	// SkyBox
	{
		static CCryNameR skyBoxParamName("SkyDome_SkyBoxParams");
		const float skyBoxAngle = m_pSkyRE->m_fSkyBoxAngle;
		const float skyBoxScaling = 1.0f / std::max(0.0001f, m_pSkyRE->m_fSkyBoxStretching);
		const float skyBoxMultiplier = gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKYBOX_MULTIPLIER);
		const Vec4 skyBoxParams(skyBoxAngle, skyBoxScaling, skyBoxMultiplier, 0);
		m_skyPass.SetConstant(skyBoxParamName, skyBoxParams, eHWSC_Pixel);
	}
}

void CSceneForwardStage::SetHDRSkyParameters()
{
	if (!m_pHDRSkyRE)
		return;

	I3DEngine* p3DEngine = gEnv->p3DEngine;

	// SkyBox
	{
		IMaterial* skyMat = m_pSkyMat ? m_pSkyMat : gEnv->p3DEngine->GetSkyMaterial();
		CShaderResources* skyRes = nullptr;
		if (!skyMat || !skyMat->GetShaderItem().m_pShaderResources)
			return;

		skyRes = (CShaderResources*)skyMat->GetShaderItem().m_pShaderResources;

		static CCryNameR skyBoxParamName("SkyDome_SkyBoxParams");
		const float skyBoxAngle = 0.0f;// DEG2RAD(gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_SKYBOX_ANGLE));
		const float skyBoxScaling = 2.0f;// 1.0f / std::max(0.0001f, gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_SKYBOX_STRETCHING));
		const float skyBoxMultiplier = gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKYBOX_MULTIPLIER);
		Vec4 skyBoxParams(skyBoxAngle, skyBoxScaling, skyBoxMultiplier, 0);
		SShaderParam::GetValue("SkyboxAngle", &skyRes->GetParameters(), &skyBoxParams.x, 0);
		skyBoxParams.x = DEG2RAD(skyBoxParams.x);
		m_skyPass.SetConstant(skyBoxParamName, skyBoxParams, eHWSC_Pixel);
	}

	// Day sky
	{
		static CCryNameR mieName("SkyDome_PartialMieInScatteringConst");
		static CCryNameR rayleighName("SkyDome_PartialRayleighInScatteringConst");
		static CCryNameR sunDirectionName("SkyDome_SunDirection");
		static CCryNameR phaseName("SkyDome_PhaseFunctionConstants");

		auto renderParams = m_pHDRSkyRE->m_pRenderParams;
		m_skyPass.SetConstant(mieName, renderParams->m_partialMieInScatteringConst, eHWSC_Pixel);
		m_skyPass.SetConstant(rayleighName, renderParams->m_partialRayleighInScatteringConst, eHWSC_Pixel);
		m_skyPass.SetConstant(sunDirectionName, renderParams->m_sunDirection, eHWSC_Pixel);
		m_skyPass.SetConstant(phaseName, renderParams->m_phaseFunctionConsts, eHWSC_Pixel);
	}

	// Night sky
	{
		static CCryNameR nightColBaseName("SkyDome_NightSkyColBase");
		static CCryNameR nightColDeltaName("SkyDome_NightSkyColDelta");
		static CCryNameR nightZenithColShiftName("SkyDome_NightSkyZenithColShift");

		Vec3 nightSkyHorizonCol;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_HORIZON_COLOR, nightSkyHorizonCol);
		Vec3 nightSkyZenithCol;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_COLOR, nightSkyZenithCol);
		float nightSkyZenithColShift(p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_SHIFT));
		const float minNightSkyZenithGradient(-0.1f);

		Vec4 colBase(nightSkyHorizonCol, 0);
		Vec4 colDelta(nightSkyZenithCol - nightSkyHorizonCol, 0);
		Vec4 zenithColShift(1.0f / (nightSkyZenithColShift - minNightSkyZenithGradient), -minNightSkyZenithGradient / (nightSkyZenithColShift - minNightSkyZenithGradient), 0, 0);

		m_skyPass.SetConstant(nightColBaseName, colBase, eHWSC_Pixel);
		m_skyPass.SetConstant(nightColDeltaName, colDelta, eHWSC_Pixel);
		m_skyPass.SetConstant(nightZenithColShiftName, zenithColShift, eHWSC_Pixel);
	}

	// Moon
	{
		static CCryNameR moonTexGenRightName("SkyDome_NightMoonTexGenRight");
		static CCryNameR moonTexGenUpName("SkyDome_NightMoonTexGenUp");
		static CCryNameR moonDirSizeName("SkyDome_NightMoonDirSize");
		static CCryNameR moonColorName("SkyDome_NightMoonColor");
		static CCryNameR moonInnerScaleName("SkyDome_NightMoonInnerCoronaColorScale");
		static CCryNameR moonOuterScaleName("SkyDome_NightMoonOuterCoronaColorScale");

		Vec3 nightMoonColor;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_COLOR, nightMoonColor);
		Vec4 moonColor(nightMoonColor, 0);

		Vec3 nightMoonInnerCoronaColor;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_COLOR, nightMoonInnerCoronaColor);
		float nightMoonInnerCoronaScale(1.0f + 1000.0f * p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_SCALE));
		Vec4 moonInnerCoronaColorScale(nightMoonInnerCoronaColor, nightMoonInnerCoronaScale);

		Vec3 nightMoonOuterCoronaColor;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_COLOR, nightMoonOuterCoronaColor);
		float nightMoonOuterCoronaScale(1.0f + 1000.0f * p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_SCALE));
		Vec4 moonOuterCoronaColorScale(nightMoonOuterCoronaColor, nightMoonOuterCoronaScale);

		m_skyPass.SetConstant(moonColorName, moonColor, eHWSC_Pixel);
		m_skyPass.SetConstant(moonInnerScaleName, moonInnerCoronaColorScale, eHWSC_Pixel);
		m_skyPass.SetConstant(moonOuterScaleName, moonOuterCoronaColorScale, eHWSC_Pixel);

		Vec3 mr;
		p3DEngine->GetGlobalParameter(E3DPARAM_SKY_MOONROTATION, mr);
		float moonLati = -gf_PI + gf_PI * mr.x / 180.0f;
		float moonLong = 0.5f * gf_PI - gf_PI * mr.y / 180.0f;

		float sinLonR = sinf(-0.5f * gf_PI);
		float cosLonR = cosf(-0.5f * gf_PI);
		float sinLatR = sinf(moonLati + 0.5f * gf_PI);
		float cosLatR = cosf(moonLati + 0.5f * gf_PI);
		Vec3 moonTexGenRight(sinLonR * cosLatR, sinLonR * sinLatR, cosLonR);
		m_paramMoonTexGenRight = Vec4(moonTexGenRight, 0);
		m_skyPass.SetConstant(moonTexGenRightName, m_paramMoonTexGenRight, eHWSC_Pixel);

		float sinLonU = sinf(moonLong + 0.5f * gf_PI);
		float cosLonU = cosf(moonLong + 0.5f * gf_PI);
		float sinLatU = sinf(moonLati);
		float cosLatU = cosf(moonLati);
		Vec3 moonTexGenUp(sinLonU * cosLatU, sinLonU * sinLatU, cosLonU);
		m_paramMoonTexGenUp = Vec4(moonTexGenUp, 0);
		m_skyPass.SetConstant(moonTexGenUpName, m_paramMoonTexGenUp, eHWSC_Pixel);

		Vec3 nightMoonDirection;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_DIRECTION, nightMoonDirection);
		float nightMoonSize(25.0f - 24.0f * clamp_tpl(p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_SIZE), 0.0f, 1.0f));
		m_paramMoonDirSize = Vec4(nightMoonDirection, true ? nightMoonSize : 9999.0f);
		m_skyPass.SetConstant(moonDirSizeName, m_paramMoonDirSize, eHWSC_Pixel);
	}
}

void CSceneForwardStage::SetMatParameters()
{
	IMaterial* skyMat = m_pSkyMat ? m_pSkyMat : gEnv->p3DEngine->GetSkyMaterial();
	CShaderResources* skyRes = nullptr;
	if (!skyMat || !skyMat->GetShaderItem().m_pShaderResources)
		return;

	skyRes = (CShaderResources*)skyMat->GetShaderItem().m_pShaderResources;

	// SkyBox
	{
		static CCryNameR skyBoxExposureName("SkyDome_SkyBoxExposure");
		const ColorF skyBoxEmittance = skyRes->GetFinalEmittance();
		Vec4 skyBoxExposure(skyBoxEmittance.r, skyBoxEmittance.g, skyBoxEmittance.b, 1.0f);
		//SShaderParam::GetValue("Exposure", &skyRes->GetParameters(), &skyBoxExposure.w, 0);
		m_skyPass.SetConstant(skyBoxExposureName, skyBoxExposure, eHWSC_Pixel);

		static CCryNameR skyBoxOpacityName("SkyDome_SkyBoxOpacity");
		const ColorF skyBoxFilter = skyRes->GetColorValue(EFTT_DIFFUSE) * skyRes->GetStrengthValue(EFTT_OPACITY);
		Vec4 skyBoxOpacity(skyBoxFilter.r, skyBoxFilter.g, skyBoxFilter.b, 1.0f);
		//SShaderParam::GetValue("Opacity", &skyRes->GetParameters(), &skyBoxOpacity.w, 0);
		m_skyPass.SetConstant(skyBoxOpacityName, skyBoxOpacity, eHWSC_Pixel);
	}
}

static void FillSkyTextureData(CTexture* pTexture, const void* pData, const uint32 width, const uint32 height, const uint32 pitch)
{
	assert(pTexture && pTexture->GetWidth() == width && pTexture->GetHeight() == height && pTexture->GetDevTexture());
	const SResourceMemoryAlignment layout = SResourceMemoryAlignment::Linear<CryHalf4>(width, height);
	CRY_ASSERT(pitch == layout.rowStride);

	GPUPIN_DEVICE_TEXTURE(gcpRendD3D->GetPerformanceDeviceContext(), pTexture->GetDevTexture());
	GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(pData, pTexture->GetDevTexture(), layout);
}

void CSceneForwardStage::ExecuteSky(CTexture* pColorTex, CTexture* pDepthTex)
{
	FUNCTION_PROFILER_RENDERER();

	if (!m_pHDRSkyRE && !m_pSkyRE)
		return;

	if (m_pHDRSkyRE && !m_pHDRSkyRE->m_pRenderParams)
		return;

	PROFILE_LABEL_SCOPE("SKY_PASS");

	PreparePerPassResources(false);

	CRenderView* pRenderView = RenderView();

	const bool applyFog = pRenderView->IsGlobalFogEnabled() && !(GetGraphicsPipeline().IsPipelineFlag(CGraphicsPipeline::EPipelineFlags::NO_SHADER_FOG));
	bool isProcedualSky = false;
	bool hasSkyDomeTexture = false;

	// Update sky dome texture if new data is available
	int timestamp = 0;

	m_pSkyDomeTextureMie = CRendererResources::s_ptexBlack;
	m_pSkyDomeTextureRayleigh = CRendererResources::s_ptexBlack;
	m_pSkyMoonTex = CRendererResources::s_ptexBlack;
	if (m_pHDRSkyRE)
	{
		if (m_pHDRSkyRE->m_skyDomeTextureLastTimeStamp != m_pHDRSkyRE->m_pRenderParams->m_skyDomeTextureTimeStamp)
		{
			FillSkyTextureData(m_pHDRSkyRE->m_pSkyDomeTextureMie, m_pHDRSkyRE->m_pRenderParams->m_pSkyDomeTextureDataMie,
			                   SSkyLightRenderParams::skyDomeTextureWidth, SSkyLightRenderParams::skyDomeTextureHeight, m_pHDRSkyRE->m_pRenderParams->m_skyDomeTexturePitch);
			FillSkyTextureData(m_pHDRSkyRE->m_pSkyDomeTextureRayleigh, m_pHDRSkyRE->m_pRenderParams->m_pSkyDomeTextureDataRayleigh,
			                   SSkyLightRenderParams::skyDomeTextureWidth, SSkyLightRenderParams::skyDomeTextureHeight, m_pHDRSkyRE->m_pRenderParams->m_skyDomeTexturePitch);

			// Update time stamp of last update
			m_pHDRSkyRE->m_skyDomeTextureLastTimeStamp = m_pHDRSkyRE->m_pRenderParams->m_skyDomeTextureTimeStamp;
		}

		m_pSkyDomeTextureMie = m_pHDRSkyRE->m_pSkyDomeTextureMie;
		m_pSkyDomeTextureRayleigh = m_pHDRSkyRE->m_pSkyDomeTextureRayleigh;
		if (m_pHDRSkyRE->m_moonTexId > 0)
			m_pSkyMoonTex = CTexture::GetByID(m_pHDRSkyRE->m_moonTexId);

		timestamp = m_pHDRSkyRE->m_skyDomeTextureLastTimeStamp;
		isProcedualSky = true;
	}

	CTexture* pSkyDomeTex = CRendererResources::s_ptexBlack;
	IMaterial* skyMat = m_pSkyMat ? m_pSkyMat : gEnv->p3DEngine->GetSkyMaterial();
	if (skyMat && skyMat->GetShaderItem().m_pShaderResources)
	{
		auto* pResources = (CShaderResources*)skyMat->GetShaderItem().m_pShaderResources;

		if (auto pSkyTexInfo = pResources->m_Textures[EFTT_DIFFUSE])
		{
			pSkyDomeTex = CTexture::ForName(pSkyTexInfo->m_Name, FT_DONT_STREAM, eTF_Unknown);
			hasSkyDomeTexture = true;
		}
	}

	D3DViewPort viewport = RenderViewportToD3D11Viewport(RenderView()->GetViewport());
	if (pRenderView->IsRecursive())
	{
		viewport = { 0.f, 0.f, float(pColorTex->GetWidth()), float(pColorTex->GetHeight()), 0.0f, 1.0f };
	}
	CRY_ASSERT(pColorTex->GetWidth() == viewport.Width && pColorTex->GetHeight() == viewport.Height);

	uint64 rtMask = 0;

	rtMask |= isProcedualSky    ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0;
	rtMask |= hasSkyDomeTexture ? g_HWSR_MaskBit[HWSR_SAMPLE1] : 0;
	rtMask |= applyFog          ? g_HWSR_MaskBit[HWSR_FOG    ] : 0;

	if (m_skyPass.IsDirty(rtMask, timestamp, pSkyDomeTex->GetTextureID(), pDepthTex->GetTextureID()))
	{
		const SSamplerState      samplerDescLinearWrapU(FILTER_LINEAR, eSamplerAddressMode_Wrap, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, 0);
		const SamplerStateHandle samplerStateLinearWrapU = GetDeviceObjectFactory().GetOrCreateSamplerStateHandle(samplerDescLinearWrapU);

		static CCryNameTSCRC techSkyPass("SkyPass");
		m_skyPass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		m_skyPass.SetTechnique(CShaderMan::s_ShaderStars, techSkyPass, rtMask);
		m_skyPass.SetRequirePerViewConstantBuffer(true);
		m_skyPass.SetRenderTarget(0, pColorTex);
		m_skyPass.SetDepthTarget(pDepthTex);
		m_skyPass.SetState(GS_DEPTHFUNC_EQUAL);
		m_skyPass.SetTexture(0, pSkyDomeTex);
		m_skyPass.SetSampler(0, EDefaultSamplerStates::LinearClamp);

		m_skyPass.SetTexture(1, m_pSkyDomeTextureMie);
		m_skyPass.SetTexture(2, m_pSkyDomeTextureRayleigh);
		m_skyPass.SetTexture(3, m_pSkyMoonTex);
		m_skyPass.SetSampler(1, samplerStateLinearWrapU);
	}

	m_skyPass.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPass, m_pPerPassCB, EShaderStage_AllWithoutCompute);
	m_skyPass.BeginConstantUpdate();
	SetSkyParameters();
	SetHDRSkyParameters();
	SetMatParameters();
	m_skyPass.Execute();

	// Stars
	float starIntensity = gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_STAR_INTENSITY);
	if (m_pHDRSkyRE && m_pHDRSkyRE->m_pStars && m_pHDRSkyRE->m_pStars->m_pStarMesh && starIntensity > 1e-3f)
	{
		m_starsPass.SetRenderTarget(0, pColorTex);
		m_starsPass.SetDepthTarget(pDepthTex);
		m_starsPass.SetViewport(viewport);
		m_starsPass.BeginAddingPrimitives();

		const bool bReverseDepth = true;
		const uint64 rtMask = bReverseDepth ? g_HWSR_MaskBit[HWSR_REVERSE_DEPTH] : 0;
		const int32 depthState = bReverseDepth ? GS_DEPTHFUNC_GEQUAL : GS_DEPTHFUNC_LEQUAL;

		static CCryNameTSCRC techStars("Stars");
		m_starsPrimitive.SetFlags(CRenderPrimitive::eFlags_ReflectShaderConstants);
		m_starsPrimitive.SetTechnique(CShaderMan::s_ShaderStars, techStars, rtMask);
		m_starsPrimitive.SetRenderState(depthState | GS_BLSRC_ONE | GS_BLDST_ONE);
		m_starsPrimitive.SetCullMode(eCULL_None);

		CRenderMesh* pStarMesh = static_cast<CRenderMesh*>(m_pHDRSkyRE->m_pStars->m_pStarMesh.get());
		buffer_handle_t hVertexStream = pStarMesh->_GetVBStream(VSF_GENERAL);

		m_starsPrimitive.SetCustomVertexStream(hVertexStream, pStarMesh->_GetVertexFormat(), pStarMesh->GetStreamStride(VSF_GENERAL));
		m_starsPrimitive.SetDrawInfo(eptTriangleList, 0, 0, 6 * m_pHDRSkyRE->m_pStars->m_numStars);
		m_starsPrimitive.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, gcpRendD3D->GetGraphicsPipeline().GetMainViewConstantBuffer(), EShaderStage_Vertex);
		m_starsPrimitive.Compile(m_starsPass);

		{
			static CCryNameR nameMoonTexGenRight("SkyDome_NightMoonTexGenRight");
			static CCryNameR nameMoonTexGenUp("SkyDome_NightMoonTexGenUp");
			static CCryNameR nameMoonDirSize("SkyDome_NightMoonDirSize");
			static CCryNameR nameStarSize("SkyDome_StarSize");
			static CCryNameR nameStarIntensity("SkyDome_StarIntensity");

			m_starsPrimitive.GetConstantManager().BeginNamedConstantUpdate();

			m_starsPrimitive.GetConstantManager().SetNamedConstant(nameMoonTexGenRight, m_paramMoonTexGenRight, eHWSC_Vertex);
			m_starsPrimitive.GetConstantManager().SetNamedConstant(nameMoonTexGenUp, m_paramMoonTexGenUp, eHWSC_Vertex);
			m_starsPrimitive.GetConstantManager().SetNamedConstant(nameMoonDirSize, m_paramMoonDirSize, eHWSC_Vertex);

			const float size = 5.0f * min(1.f, min(viewport.Width / 1280.f, viewport.Height / 720.f));
			float flickerTime(gEnv->pTimer->GetCurrTime());
			Vec4 paramStarSize(size / (float)viewport.Width, size / (float)viewport.Height, 0, flickerTime * 0.5f);
			m_starsPrimitive.GetConstantManager().SetNamedConstant(nameStarSize, paramStarSize, eHWSC_Vertex);
			Vec4 paramStarIntensity(starIntensity * min(1.0f, size), 0, 0, 0);
			m_starsPrimitive.GetConstantManager().SetNamedConstant(nameStarIntensity, paramStarIntensity, eHWSC_Pixel);

			m_starsPrimitive.GetConstantManager().EndNamedConstantUpdate(&m_starsPass.GetViewport());

			m_starsPass.AddPrimitive(&m_starsPrimitive);
			m_starsPass.Execute();
		}
	}

//	m_pSkyRE = nullptr;
//	m_pHDRSkyRE = nullptr;
//	m_pSkyMat = nullptr;
}

void CSceneForwardStage::FillCloudShadingParams(SCloudShadingParams& cloudParams, bool enable) const
{
	SRenderViewShaderConstants& paramsPF = RenderView()->GetShaderConstants();

	if (enable)
	{
		cloudParams.CloudShadingColorSun = Vec4(paramsPF.pCloudShadingColorSun, 0.0f);
		cloudParams.CloudShadingColorSky = Vec4(paramsPF.pCloudShadingColorSky, 0.0f);
	}
	else
	{
		// turning off by parameters.
		cloudParams.CloudShadingColorSun = Vec4(0.0f);
		cloudParams.CloudShadingColorSky = Vec4(0.0f);
	}
}
