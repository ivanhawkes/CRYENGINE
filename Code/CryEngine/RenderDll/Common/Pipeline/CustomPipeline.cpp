#include "StdAfx.h"
#include "CustomPipeline.h"
#include "PassRenderer.h"
#include "StageResourceProvider.h"
#include "../RenderView.h"
#include "GraphicsPipeline/Common/GraphicsPipeline.h"

using namespace Cry;
using namespace Cry::Renderer;
using namespace Cry::Renderer::Pipeline;

static CCustomPipeline* g_pStageRenderer = nullptr;
static ICustomPipelinePtr g_pIPipelineUnknown;



CPrimitiveRenderPass* CPassHeap::GetUsable()
{
	if (m_freeList.begin() == m_freeList.end())
		m_useList.emplace_front();
	else
		m_useList.splice_after(m_useList.before_begin(), m_freeList, m_freeList.before_begin());

	return &*m_useList.begin();
}

void CPassHeap::FreeUsable()
{
	for (auto& pass : m_useList)
		pass.Reset();
	
	m_freeList.splice_after(m_freeList.before_begin(), m_useList);
}

CRenderPrimitive* CPrimitiveHeap::GetUsable()
{
	if (m_freeList.begin() == m_freeList.end())
		m_useList.emplace_front();
	else
		m_useList.splice_after(m_useList.before_begin(), m_freeList, m_freeList.before_begin());

	return &*m_useList.begin();
}

void CPrimitiveHeap::FreeUsable()
{
	for (auto& prim : m_useList)
		prim.Reset();

	m_freeList.splice_after(m_freeList.before_begin(), m_useList);
}

CConstantBuffer* CConstantBufferHeap::GetUsable()
{
	assert(maxBufferSize != 0, "[Custom Pipeline] Constant buffer requested but no size specified");

	CryAutoCriticalSectionNoRecursive threadSafe(m_lock);

	if (m_freeList.begin() == m_freeList.end())
		m_useList.emplace_front(gRenDev->m_DevBufMan.CreateConstantBuffer(maxBufferSize));
	else
		m_useList.splice_after(m_useList.before_begin(), m_freeList, m_freeList.before_begin());

	return *m_useList.begin();
}

void CConstantBufferHeap::FreeUsable()
{
	CryAutoCriticalSectionNoRecursive threadSafe(m_lock);

	m_freeList.splice_after(m_freeList.before_begin(), m_useList);
}

void CConstantBufferHeap::SetSize(uint32 maxBufferSize)
{
	m_freeList.clear();
	m_useList.clear();

	maxBufferSize = maxBufferSize;
}

CCustomPipeline* CCustomPipeline::Get()
{
	return g_pStageRenderer;
}

CCustomPipeline* CCustomPipeline::Create()
{
	if (g_pStageRenderer)
		return g_pStageRenderer;

	g_pIPipelineUnknown = GetOrCreateCustomPipeline();
	if (!g_pIPipelineUnknown)
		return nullptr;

	g_pStageRenderer = static_cast<CCustomPipeline*>(g_pIPipelineUnknown.get());
	return g_pStageRenderer;
}

void CCustomPipeline::Destroy()
{
	g_pStageRenderer->Shutdown();

	g_pIPipelineUnknown.reset();
	g_pStageRenderer = nullptr;
}


CCustomPipeline::CCustomPipeline()
{
	CryLogAlways("Creating custom pipeline");
	m_pResourceProvider = std::make_unique<Renderer::CStageResourceProvider>();
}

void CCustomPipeline::Shutdown()
{
	for (auto& pStage : m_stageList)
	{
		StageDestructionsArguments args{ pStage,true };
		pStage->callbacks.destructionCallback(args);
	}
	m_stageList.clear();
	m_renderList.clear();
}


void CCustomPipeline::ExecuteRenderThreadCommand(TRenderThreadCommand command)
{
	gcpRendD3D->ExecuteRenderThreadCommand(std::forward<TRenderThreadCommand>(command));
}

void CCustomPipeline::CreateRenderStage(const char* name, uint32 hash, SStageCallbacks callbacks, uint32 maxConstantBufferSize)
{
	assert(callbacks.creationCallback && callbacks.destructionCallback, "Failed to create render stage. No creation and destruction callbacks were provided.");

	ExecuteRenderThreadCommand([pThis = this, cName = string(name), hash, cCb = std::move(callbacks), size = maxConstantBufferSize]() {
		pThis->RT_CreateRenderStage(std::move(cName), hash, std::move(cCb), size);
	});
}

_smart_ptr<SStageBase> CCustomPipeline::RT_CreateRenderStage(string name, uint32 hash, SStageCallbacks renderCallback, uint32 maxConstantBufferSize)
{
	auto stageEntry = std::find_if(m_stageList.begin(), m_stageList.end(), [hash](auto& entry) { return hash == entry->hash; });
	if (stageEntry != m_stageList.end())
		return nullptr;

	_smart_ptr<SStage> pStage = new SStage(std::move(name), hash);
	pStage->callbacks = std::move(renderCallback);
	pStage->dataStorage.constantBuffers.SetSize(maxConstantBufferSize);
	pStage->dataStorage.clearRegionPass = std::make_unique<CClearRegionPass>(&*gcpRendD3D->GetActiveGraphicsPipeline());

	auto stage = pStage.get();

	m_stageList.push_back(pStage);

	StageCreationArguments args{ pStage };

	if (stage->callbacks.creationCallback)
		stage->callbacks.creationCallback(args);

	if (stage->callbacks.renderCallback)
		RT_AddRenderEntry(stage->name, hash, [this, stage](StageRenderArguments& args) { RenderStage(*stage); });

	return pStage;
}

void CCustomPipeline::RT_RemoveStage(uint32 hash)
{
	auto stageEntry = std::find_if(m_stageList.begin(), m_stageList.end(), [hash](auto& entry) { return hash == entry->hash; });
	if (stageEntry != m_stageList.end())
		return;

	RT_RemoveRenderEntry(hash);

	auto pStage = std::move(*stageEntry._Ptr);
	m_stageList.erase(stageEntry);

	StageDestructionsArguments args{ pStage };

	pStage->callbacks.destructionCallback(args);
}

void CCustomPipeline::RT_Render()
{
	for (auto& renderEntry : m_renderList)
	{
		PROFILE_LABEL_SCOPE_DYNAMIC(renderEntry.name, "CustomStage");

		StageRenderArguments args{};
		renderEntry.renderCall(args);
	}
	//Clear stage data storage
	for (auto stage : m_stageList)
		RT_ResetDynamicStageData(*stage);
}

void CCustomPipeline::RT_AddRenderEntry(const char* name, uint32 hash, TStageRenderCallback renderCallback, int sort /*= 0*/)
{
	auto entry = std::find(m_renderList.begin(), m_renderList.end(), hash);
	if (entry != m_renderList.end())
		return;

	m_renderList.emplace_back(name, hash, std::move(renderCallback));
}

void CCustomPipeline::RT_RemoveRenderEntry(uint32 hash)
{
	auto entry = std::find(m_renderList.begin(), m_renderList.end(), hash);
	if (entry != m_renderList.end())
		m_renderList.erase(entry);
}

uint32 CCustomPipeline::RT_AllocatePass(SStageBase& stageBase, const Pass::SPassCreationParams& params)
{
	SStage& stage = static_cast<SStage&>(stageBase);

	stage.dataStorage.registeredPasses.emplace_back();
	auto& pass = stage.dataStorage.registeredPasses.back();

	pass.SetLabel(params.passName);
	RenderPass::UpdatePassData(pass, params);

	return stage.dataStorage.registeredPasses.size() - 1;
}

IStageResourceProvider* CCustomPipeline::GetResourceProvider() const
{
	return m_pResourceProvider.get();
}

void CCustomPipeline::RT_BeginPass(SStageBase& stageBase, uint32 passIDX)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (stage.pActivePass || passIDX >= stage.dataStorage.registeredPasses.size())
		return;

	stage.pActivePass = &stage.dataStorage.registeredPasses[passIDX];
	RenderPass::BeginPass(*stage.pActivePass);
}

void CCustomPipeline::RT_BeginNewPass(SStageBase& stageBase, const Pass::SPassParams& params)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (stage.pActivePass)
		return;

	stage.pActivePass = stage.dataStorage.dynamicPasses.GetUsable();
	RenderPass::UpdatePassData(*stage.pActivePass, params);
	RenderPass::BeginPass(*stage.pActivePass);
}

void CCustomPipeline::RT_BeginNewPass(SStageBase& stageBase)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (stage.pActivePass)
		return;

	stage.pActivePass = stage.dataStorage.dynamicPasses.GetUsable();
	RenderPass::BeginPass(*stage.pActivePass);
}

void CCustomPipeline::RT_BeginPass(SStageBase& stageBase, uint32 passIDX, const Pass::SPassParams& params)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (stage.pActivePass || passIDX >= stage.dataStorage.registeredPasses.size())
		return;

	stage.pActivePass = &stage.dataStorage.registeredPasses[passIDX];
	RenderPass::UpdatePassData(*stage.pActivePass, params);
	RenderPass::BeginPass(*stage.pActivePass);

}

void CCustomPipeline::RT_AddPrimitives(SStageBase& stageBase, const TArray<Pass::SPrimitiveParams>& params)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (!stage.pActivePass)
		return;

	for (auto& param : params)
	{
		auto pRimitive = stage.dataStorage.primitives.GetUsable();
		RenderPass::AddPrimitive(*stage.pActivePass, param, *pRimitive, stage.dataStorage);
	}
}

void CCustomPipeline::RT_AddPrimitive(SStageBase& stageBase, const Pass::SPrimitiveParams& param)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (!stage.pActivePass)
		return;

	auto pRimitive = stage.dataStorage.primitives.GetUsable();
	if (!pRimitive)
		return;

	RenderPass::AddPrimitive(*stage.pActivePass, param, *pRimitive, stage.dataStorage);
}

void CCustomPipeline::RT_EndPass(SStageBase& stageBase, bool bExecute)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (!stage.pActivePass)
		return;
	
	if (bExecute)
		stage.pActivePass->Execute();
	
	stage.pActivePass = nullptr;
}

void CCustomPipeline::RT_UpdatePassData(SStageBase& stageBase, uint32 passIDX, const Pass::SPassParams& params)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (passIDX >= stage.dataStorage.registeredPasses.size())
		return;

	auto pass = &stage.dataStorage.registeredPasses[passIDX];
	RenderPass::UpdatePassData(*pass, params);
}

void CCustomPipeline::RenderStage(SStage& stage)
{
	StageRenderArguments args{};
	stage.callbacks.renderCallback(args);
}

void CCustomPipeline::RT_ResetDynamicStageData(SStageBase& stageBase)
{
	auto& stage = static_cast<SStage&>(stageBase);

	stage.dataStorage.dynamicPasses.FreeUsable();
	stage.dataStorage.primitives.FreeUsable();
	stage.dataStorage.constantBuffers.FreeUsable();
}

void Cry::Renderer::Pipeline::CCustomPipeline::RT_StretchToColorTarget(ITexture* pSrc, uint32 stateMask)
{
	if (!pSrc)
		return;

	auto pPipeline = &*gcpRendD3D->GetActiveGraphicsPipeline();

	if (!m_compositionPass)
		m_compositionPass = std::make_unique<CStretchRectPass>(pPipeline);

	auto pColorTarget = pPipeline->GetCurrentRenderView()->GetColorTarget();
	//auto pColorTarget = pPipeline->GetPipelineResources().m_pTexDisplayTargetDst;

	if (!pColorTarget)
		return;

	m_compositionPass->Execute(static_cast<CTexture*>(pSrc), pColorTarget, true, stateMask);
}


void Cry::Renderer::Pipeline::CCustomPipeline::RT_ClearSurface(SStageBase& stageBase, const ITexture* pColorTex, const ColorF& cClear)
{
	CClearSurfacePass::Execute(static_cast<const CTexture*>(pColorTex), cClear);
}

void Cry::Renderer::Pipeline::CCustomPipeline::RT_ClearSurfaceRegion(SStageBase& stageBase, ITexture* pTex, const ColorF& cClear, const uint numRects, const Vec4_tpl<ulong>* pRects)
{
	static_cast<SStage&>(stageBase).dataStorage.clearRegionPass->Execute(static_cast<CTexture*>(pTex), cClear, numRects, (RECT*)pRects);
}

void Cry::Renderer::Pipeline::CCustomPipeline::RT_ClearDepthSurfaceRegion(SStageBase& stageBase, ITexture* pDepthTex, const int nFlags, const float cDepth, const uint8 cStencil, const uint numRects, const Vec4_tpl<ulong>* pRects)
{
	static_cast<SStage&>(stageBase).dataStorage.clearRegionPass->Execute(static_cast<CTexture*>(pDepthTex), nFlags, cDepth, cStencil, numRects, (RECT*)pRects);
}

void Cry::Renderer::Pipeline::CCustomPipeline::RT_ClearDepthSurface(SStageBase& stageBase, const ITexture* pDepthTex, const int nFlags, const float cDepth, const uint8 cStencil)
{
	CClearSurfacePass::Execute(static_cast<const CTexture*>(pDepthTex), nFlags, cDepth, cStencil);
}

CRYREGISTER_SINGLETON_CLASS(Cry::Renderer::Pipeline::CCustomPipeline);
