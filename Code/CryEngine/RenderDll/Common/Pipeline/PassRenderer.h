#pragma once
#include "CryRenderer/Pipeline/RenderPass.h"

namespace Cry
{
	namespace Renderer
	{
		namespace Pipeline
		{
			namespace RenderPass
			{
				static void UpdatePassData(CPrimitiveRenderPass& pass, const Pipeline::Pass::SPassParams& params);

				static void BeginPass(CPrimitiveRenderPass& pass);

				static void BeginPass(CPrimitiveRenderPass& pass, const Pipeline::Pass::SPassParams& updateParams)
				{
					UpdatePassData(pass, updateParams);
					BeginPass(pass);
				}

				static void AddPrimitive(CPrimitiveRenderPass& pass, const Pipeline::Pass::SPrimitiveParams& primitiveParams, CRenderPrimitive& primitive, SStageDataStorage& storage);

				static void DrawPrimitive(CRenderPrimitive& primitive, const Pipeline::Pass::SDrawParams& params);

				static void DrawPrimitive(CRenderPrimitive& primitive, const Pipeline::Pass::SDrawParamsExternalBuffers& params);

				static void DrawPrimitive(CRenderPrimitive& primitive, const Pipeline::Pass::SDrawParamsRenderMesh& params);

				static void SetPrimitiveShaderParams(CRenderPrimitive& primitive, const Pipeline::Pass::SShaderParams& params);

				static void SetShaderConstants(CRenderPrimitive& primitive, const Pipeline::Pass::SConstantParams& params, CPrimitiveRenderPass& pass, bool bPreCompile, SStageDataStorage& storage);

				static void SetShaderConstants(CRenderPrimitive& primitive, const Pipeline::Pass::SNamedConstantsParams& params, CPrimitiveRenderPass& pass, SStageDataStorage& storage);

				static void SetShaderConstants(CRenderPrimitive& primitive, const Pipeline::Pass::SInlineConstantParams& params, CPrimitiveRenderPass& pass, SStageDataStorage& storage);

				static void SetShaderConstants(CRenderPrimitive& primitive, const Pipeline::Pass::SInlineMultiValueConstantParams& params, CPrimitiveRenderPass& pass, SStageDataStorage& storage);
			}
		}
	}
}