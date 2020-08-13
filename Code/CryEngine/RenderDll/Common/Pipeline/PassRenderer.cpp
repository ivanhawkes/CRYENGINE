#include "StdAfx.h"
#include "PassRenderer.h"
#include "StageResourceProvider.h"


using namespace Cry::Renderer::Pipeline;
using namespace Pass;

void RenderPass::UpdatePassData(CPrimitiveRenderPass& pass, const SPassParams& params)
{
	pass.SetViewport(params.viewPort);
	pass.SetTargetClearMask(params.clearMask);

	auto pTex = static_cast<CTexture*>(params.pColorTarget);
	if (pTex && pTex != pass.GetRenderTarget(0))
		pass.SetRenderTarget(0, pTex);

	pTex = static_cast<CTexture*>(params.pDepthsTarget);
	if (pTex && pTex != pass.GetDepthTarget())
		pass.SetDepthTarget(pTex);

	if (!params.scissorRect.IsZero())
		pass.SetScissor(true, { (long)params.scissorRect.x, (long)params.scissorRect.y, (long)params.scissorRect.z, (long)params.scissorRect.w });
	else
		pass.SetScissor(false, { 0,0,0,0 });

}

void RenderPass::BeginPass(CPrimitiveRenderPass& pass)
{
	pass.BeginAddingPrimitives();
}

void RenderPass::AddPrimitive(CPrimitiveRenderPass& pass, const SPrimitiveParams& primitiveParams, CRenderPrimitive& primitive, SStageDataStorage& storage)
{
	//Convert shader reflection params
	auto flags = (CRenderPrimitive::EPrimitiveFlags)primitiveParams.shaderParams.flags;
	primitive.SetFlags(flags);

	//Set the primitive type
	auto primitiveType = (CRenderPrimitive::EPrimitiveType)primitiveParams.type;
	primitive.SetPrimitiveType(primitiveType);

	//Set render statres for the primitive
	primitive.SetRenderState(primitiveParams.renderStateFlags);
	primitive.SetStencilState(primitiveParams.stencilState, primitiveParams.stencilRef, primitiveParams.stencilReadMask, primitiveParams.stencilWriteMask);
	primitive.SetCullMode(primitiveParams.cullMode);

	//Convert and set scissor rect for this primitive
	D3D11_RECT rect = { primitiveParams.scissorRect.x, primitiveParams.scissorRect.y, primitiveParams.scissorRect.z, primitiveParams.scissorRect.w };
	primitive.SetScissorRect(rect);

	//Draw primitive geometry
	DrawPrimitive(primitive, primitiveParams.drawParams);
	//Update shader parameters for primitive (textures,samplers etc)
	SetPrimitiveShaderParams(primitive, primitiveParams.shaderParams);

	//Add and compile constants for the primitive
	SetShaderConstants(primitive, primitiveParams.constantParameters, pass, true, storage);
	if (primitive.Compile(pass) != CRenderPrimitive::EDirtyFlags::eDirty_None)
		SetShaderConstants(primitive, primitiveParams.constantParameters, pass, false, storage);

	pass.AddPrimitive(&primitive);
}

void RenderPass::DrawPrimitive(CRenderPrimitive& primitive, const SDrawParams& params)
{
	switch (params.type)
	{
	case EDrawParamTypes::ExternalBuffers:
		DrawPrimitive(primitive, static_cast<const SDrawParamsExternalBuffers&>(params));
		break;
	case EDrawParamTypes::RenderMesh:
		DrawPrimitive(primitive, static_cast<const SDrawParamsRenderMesh&>(params));
		break;
	}
}

void RenderPass::DrawPrimitive(CRenderPrimitive& primitive, const SDrawParamsRenderMesh& params)
{
	auto* pRenderMesh = static_cast<CRenderMesh*>(params.pRenderMesh);

	if (!pRenderMesh)
		return;

	pRenderMesh->RT_CheckUpdate(pRenderMesh->_GetVertexContainer(), pRenderMesh->_GetVertexFormat(), 0);

	buffer_handle_t hVertexStream = pRenderMesh->_GetVBStream(VSF_GENERAL);
	buffer_handle_t hIndexStream = pRenderMesh->_GetIBStream();

	if (hVertexStream != ~0u && hIndexStream != ~0u)
	{
		const auto primType = pRenderMesh->_GetPrimitiveType();
		const auto numIndex = pRenderMesh->_GetNumInds();

		primitive.SetCustomVertexStream(hVertexStream, pRenderMesh->_GetVertexFormat(), pRenderMesh->GetStreamStride(VSF_GENERAL));
		primitive.SetCustomIndexStream(hIndexStream, (sizeof(vtx_idx) == 2 ? Index16 : Index32));
		primitive.SetDrawInfo(primType, params.vtxOffset, params.idxOffset, params.idxCount);
	}
}

void RenderPass::DrawPrimitive(CRenderPrimitive& primitive, const SDrawParamsExternalBuffers& params)
{
	primitive.SetCustomVertexStream(params.inputBuffer, params.inputLayout, params.inputStride, params.inputByteOffset);
	primitive.SetCustomIndexStream(params.idxBuffer, params.isIDX32 ? Index32 : Index16);

	primitive.SetDrawInfo((ERenderPrimitiveType)params.drawType, params.inputOffset, params.idxOffset, params.idxSize);
}

void RenderPass::SetPrimitiveShaderParams(CRenderPrimitive& primitive, const SShaderParams& params)
{
	auto pShader = static_cast<CShader*>(params.pShader);
	primitive.SetTechnique(pShader, params.techniqueLCCRC, params.rtMask);

	for (auto& texture : params.textures)
	{
		CTexture* pCTexture = static_cast<CTexture*>(texture.pTexture);
		primitive.SetTexture(texture.slot, pCTexture);
	}

	for (auto& samplerState : params.samplerStates)
	{
		primitive.SetSampler(samplerState.slot, samplerState.handle);
	}
}

void RenderPass::SetShaderConstants(CRenderPrimitive& primitive, const SConstantParams& params, CPrimitiveRenderPass& pass, bool bPreCompile, SStageDataStorage& storage)
{
	if (bPreCompile)
	{
		switch (params.type)
		{
		case EConstantParamsType::ConstantBuffer:
			SetShaderConstants(primitive, static_cast<const SInlineConstantParams&>(params), pass, storage);
			break;
		case Pass::EConstantParamsType::MultiValueConstantBuffer:
			SetShaderConstants(primitive, static_cast<const SInlineMultiValueConstantParams&>(params), pass, storage);
			break;
		default:
			break;
		}

	}
	else
	{
		switch (params.type)
		{
		case EConstantParamsType::NamedConstants:
			SetShaderConstants(primitive, static_cast<const SNamedConstantsParams&>(params), pass, storage);
			break;
		default:
			break;
		}
	}
}

void RenderPass::SetShaderConstants(CRenderPrimitive& primitive, const SInlineConstantParams& params, CPrimitiveRenderPass& pass, SStageDataStorage& storage)
{
	//Todo: Find a better way to handle this. Requires to much manual setup and probably not efficient way to handle resources
	for (auto& buffer : params.buffers)
	{
		CConstantBuffer* pBuffer = nullptr;
		if (buffer.externalBuffer != Buffers::CINVALID_BUFFER)
			pBuffer = reinterpret_cast<CConstantBuffer*>(buffer.externalBuffer);
		else
			pBuffer = storage.constantBuffers.GetUsable();

		if (buffer.isDirty)
			pBuffer->UpdateBuffer(buffer.value.newData, buffer.value.dataSize);

		primitive.SetInlineConstantBuffer((EConstantBufferShaderSlot)buffer.slot, pBuffer, (EShaderStage)buffer.stages);
	}
}

void Cry::Renderer::Pipeline::RenderPass::SetShaderConstants(CRenderPrimitive& primitive, const Pipeline::Pass::SInlineMultiValueConstantParams& params, CPrimitiveRenderPass& pass, SStageDataStorage& storage)
{
	for (auto& buffer : params.buffers)
	{
		CConstantBuffer* pBuffer = nullptr;
		if (buffer.externalBuffer != Buffers::CINVALID_BUFFER)
			pBuffer = reinterpret_cast<CConstantBuffer*>(buffer.externalBuffer);
		else
			pBuffer = storage.constantBuffers.GetUsable();

		if (buffer.isDirty && !buffer.values.empty())
		{
			void* pData = pBuffer->BeginWrite();
			for (auto &value : buffer.values)
			{
				memcpy(pData, value.newData, value.dataSize);
				pData = (uint8*)pData + value.dataSize;
			}
			pBuffer->EndWrite(true);
		}
			
		primitive.SetInlineConstantBuffer((EConstantBufferShaderSlot)buffer.slot, pBuffer, (EShaderStage)buffer.stages);
	}
}

void RenderPass::SetShaderConstants(CRenderPrimitive& primitive, const SNamedConstantsParams& params, CPrimitiveRenderPass& pass, SStageDataStorage& storage)
{
	auto& constantManager = primitive.GetConstantManager();
	constantManager.BeginNamedConstantUpdate();

	CStageResourceProvider* pResourceProvider = static_cast<CStageResourceProvider*>(CCustomPipeline::Get()->GetResourceProvider());

	for (auto& constant : params.namedConstants)
	{
		CCryNameR constantName = pResourceProvider->GetConstantName(constant.constantIDX);
		EHWShaderClass shaderClass = static_cast<EHWShaderClass>(constant.shaderClass);
		constantManager.SetNamedConstant(constantName, constant.value, shaderClass);
	}

	for (auto& constant : params.namedConstantArrays)
	{
		CCryNameR constantName = pResourceProvider->GetConstantName(constant.constantIDX);
		EHWShaderClass shaderClass = static_cast<EHWShaderClass>(constant.shaderClass);
		constantManager.SetNamedConstantArray(constantName, constant.values.Data(), constant.values.size(), shaderClass);
	}

	constantManager.EndNamedConstantUpdate(&pass.GetViewport(), gcpRendD3D->GetActiveGraphicsPipeline()->GetCurrentRenderView());
}



